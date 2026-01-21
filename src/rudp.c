#define _GNU_SOURCE
#include "rudp.h"
#include "logger.h"

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_FRAME_SIZE ((1 << 16) - 1)
#define MAX_TRANSMISION_ATTEMPTS 30
#define ACK_TIMEOUT_USEC 2000000
#define ACK_ACK_TIMEOUT_USEC 1000000
#define ACK_ACK_DELAY_USEC 0
#define ACK_ACK_SEND_COUNT 1
#define MAX_RUDP_PEERS 32
#define MAX_RUDP_PENDING 256

static int wait_for_frame(
    int socket, const struct sockaddr_in* expected_addr, frame_type_t expected_type, uint8_t expected_seq,
    int timeout_us, void* out_buf, size_t out_bufsize
);
static int pending_take_message_any(struct sockaddr_in* addr_out, void* out_buf, size_t* out_len);
static int pending_store_frame(const struct sockaddr_in* addr, const void* buf, size_t len);
static int send_ack_frame(int socket, const struct sockaddr_in* to, socklen_t to_len, uint8_t seq_bit);
static int send_ack_ack_burst(int socket, const struct sockaddr_in* dest, socklen_t dest_len, uint8_t seq_bit);
static int
receive_message_frame(int socket, struct sockaddr_in* source_addr, void* out_buf, size_t out_buf_size, size_t* out_len);

typedef struct {
    uint8_t send_seq_bit;
    uint8_t expected_seq_bit;
} rudp_state_t;

typedef struct {
    int used;
    struct sockaddr_in addr;
    rudp_state_t state;
} rudp_peer_t;

typedef struct {
    int used;
    struct sockaddr_in addr;
    size_t len;
    char data[MAX_FRAME_SIZE];
} rudp_pending_frame_t;

static rudp_peer_t peers[MAX_RUDP_PEERS];
static rudp_pending_frame_t pending_frames[MAX_RUDP_PENDING];

static int addr_equal(const struct sockaddr_in* a, const struct sockaddr_in* b) {
    return a->sin_family == b->sin_family && a->sin_port == b->sin_port && a->sin_addr.s_addr == b->sin_addr.s_addr;
}

static rudp_peer_t* get_peer(const struct sockaddr_in* addr, int create) {
    for (size_t i = 0; i < MAX_RUDP_PEERS; i++) {
        if (peers[i].used && addr_equal(&peers[i].addr, addr)) {
            return &peers[i];
        }
    }
    if (!create) {
        return NULL;
    }
    for (size_t i = 0; i < MAX_RUDP_PEERS; i++) {
        if (!peers[i].used) {
            peers[i].used = 1;
            peers[i].addr = *addr;
            peers[i].state.send_seq_bit = 0;
            peers[i].state.expected_seq_bit = 0;
            return &peers[i];
        }
    }
    return NULL;
}

int rudp_sendto(int socket, const void* buf, size_t len, const struct sockaddr_in* dest_addr, socklen_t addrlen) {
    size_t total_size = sizeof(header_t) + len;
    if (total_size > MAX_FRAME_SIZE) {
        LOG_ERROR("Exceeded UDP datagram size (%d)", total_size);
        return -1;
    }

    rudp_peer_t* peer = get_peer(dest_addr, 1);
    if (!peer) {
        LOG_ERROR("RUDP peer table full");
        return -1;
    }

    frame_t* frame = malloc(total_size);
    frame->header.frame_type = MESSAGE;
    frame->header.seq_bit = peer->state.send_seq_bit;
    memcpy(frame->data, buf, len);

    int ack_received = 0;
    int attempts = 0;
    while (!ack_received) {
        if (sendto(socket, frame, total_size, 0, (struct sockaddr*) dest_addr, addrlen) < 0) {
            LOG_ERROR("Sending UDP frame: %s", strerror(errno));
            free(frame);
            return -1;
        }

        frame_t ack_frame;
        int ack_result = wait_for_frame(
            socket, dest_addr, ACK, peer->state.send_seq_bit, ACK_TIMEOUT_USEC, &ack_frame, sizeof(ack_frame)
        );
        if (ack_result < 0) {
            free(frame);
            return -1;
        }

        if (ack_result == 0) {
            attempts++;
            LOG_DEBUG(
                "Timeout waiting for ACK(%u), resending message (retry number %d)", peer->state.send_seq_bit, attempts
            );
            if (attempts >= MAX_TRANSMISION_ATTEMPTS) {
                LOG_WARN("ACK retries exhausted for seq=%u", peer->state.send_seq_bit);
                free(frame);
                return -1;
            }
            continue;
        }

        LOG_DEBUG("Received appropriate ACK(%u)", peer->state.send_seq_bit);
        ack_received = 1;
    }

    free(frame);

    if (send_ack_ack_burst(socket, dest_addr, addrlen, peer->state.send_seq_bit) < 0) {
        return -1;
    }

    peer->state.send_seq_bit ^= 1;

    return 0;
}

int rudp_recvfrom(int socket, void* buf, size_t len, struct sockaddr_in* source_addr, socklen_t addrlen) {
    size_t total_size = sizeof(header_t) + len;
    if (total_size > MAX_FRAME_SIZE) {
        LOG_ERROR("Exceeded UDP datagram size (%d)", total_size);
        return -1;
    }

    while (1) {
        char frame_buf[MAX_FRAME_SIZE];
        size_t frame_len = 0;
        int recv_result = receive_message_frame(socket, source_addr, frame_buf, sizeof(frame_buf), &frame_len);
        if (recv_result != 0) {
            return recv_result;
        }

        (void) frame_len;
        frame_t* message_frame = (frame_t*) frame_buf;

        rudp_peer_t* peer = get_peer(source_addr, 1);
        if (!peer) {
            LOG_ERROR("RUDP peer table full");
            return -1;
        }

        int is_expected = (message_frame->header.seq_bit == peer->state.expected_seq_bit);
        if (is_expected) {
            LOG_DEBUG("Received appropriate message(%u)", peer->state.expected_seq_bit);
            memcpy(buf, message_frame->data, len);
        } else {
            LOG_DEBUG(
                "Received duplicate message(%u), expected(%u)", message_frame->header.seq_bit,
                peer->state.expected_seq_bit
            );
        }

        int ack_ack_received = 0;
        int attempts = 0;
        while (!ack_ack_received && attempts < MAX_TRANSMISION_ATTEMPTS) {
            if (send_ack_frame(socket, source_addr, addrlen, message_frame->header.seq_bit) < 0) {
                return -1;
            }

            frame_t ack_ack_frame;
            int ack_ack_result = wait_for_frame(
                socket, source_addr, ACK_ACK, message_frame->header.seq_bit, ACK_ACK_TIMEOUT_USEC, &ack_ack_frame,
                sizeof(ack_ack_frame)
            );
            if (ack_ack_result < 0) {
                return -1;
            }

            if (ack_ack_result == 0) {
                attempts++;
                LOG_DEBUG(
                    "Timeout waiting for ACK_ACK(%u), resending ACK (retry number %d)", message_frame->header.seq_bit,
                    attempts
                );
                continue;
            }

            LOG_DEBUG("Received appropriate ACK_ACK(%u)", message_frame->header.seq_bit);
            ack_ack_received = 1;
        }

        if (!ack_ack_received) {
            LOG_WARN("ACK-ACK was not received. Even though token is being forwarded.");
        }

        if (is_expected) {
            peer->state.expected_seq_bit ^= 1;
            return 0;
        }
    }
}

static int pending_take_frame(
    const struct sockaddr_in* expected_addr, frame_type_t expected_type, uint8_t expected_seq, void* out_buf,
    size_t out_bufsize
) {
    for (size_t i = 0; i < MAX_RUDP_PENDING; i++) {
        if (!pending_frames[i].used)
            continue;

        frame_t* f = (frame_t*) pending_frames[i].data;
        if (f->header.frame_type != expected_type)
            continue;
        if (f->header.seq_bit != expected_seq)
            continue;
        if (!addr_equal(&pending_frames[i].addr, expected_addr))
            continue;

        size_t copy_len = pending_frames[i].len < out_bufsize ? pending_frames[i].len : out_bufsize;
        memcpy(out_buf, pending_frames[i].data, copy_len);
        pending_frames[i].used = 0;
        return 1;
    }
    return 0;
}


static int pending_take_message_any(struct sockaddr_in* addr_out, void* out_buf, size_t* out_len) {
    for (size_t i = 0; i < MAX_RUDP_PENDING; i++) {
        if (!pending_frames[i].used) {
            continue;
        }
        frame_t* frame = (frame_t*) pending_frames[i].data;
        if (frame->header.frame_type != MESSAGE) {
            continue;
        }
        *addr_out = pending_frames[i].addr;
        *out_len = pending_frames[i].len;
        memcpy(out_buf, pending_frames[i].data, pending_frames[i].len);
        pending_frames[i].used = 0;
        return 1;
    }
    return 0;
}

static int pending_store_frame(const struct sockaddr_in* addr, const void* buf, size_t len) {
    if (len > MAX_FRAME_SIZE || len < sizeof(header_t))
        return 0;

    const frame_t* nf = (const frame_t*) buf;


    if (nf->header.frame_type != MESSAGE) {
        for (size_t i = 0; i < MAX_RUDP_PENDING; i++) {
            if (!pending_frames[i].used)
                continue;
            if (!addr_equal(&pending_frames[i].addr, addr))
                continue;

            frame_t* of = (frame_t*) pending_frames[i].data;
            if (of->header.frame_type == nf->header.frame_type && of->header.seq_bit == nf->header.seq_bit) {
                pending_frames[i].len = len;
                memcpy(pending_frames[i].data, buf, len);
                return 1;
            }
        }
    }

    for (size_t i = 0; i < MAX_RUDP_PENDING; i++) {
        if (!pending_frames[i].used) {
            pending_frames[i].used = 1;
            pending_frames[i].addr = *addr;
            pending_frames[i].len = len;
            memcpy(pending_frames[i].data, buf, len);
            return 1;
        }
    }

    LOG_WARN("RUDP pending buffer full, dropping frame");
    return 0;
}


static int send_ack_frame(int socket, const struct sockaddr_in* to, socklen_t to_len, uint8_t seq_bit) {
    frame_t ack_frame = {.header = {.frame_type = ACK, .seq_bit = seq_bit}};
    if (sendto(socket, &ack_frame, sizeof(header_t), 0, (struct sockaddr*) to, to_len) < 0) {
        LOG_ERROR("Sending ACK(%u): %s", ack_frame.header.seq_bit, strerror(errno));
        return -1;
    }
    return 0;
}

static int send_ack_ack_burst(int socket, const struct sockaddr_in* dest, socklen_t dest_len, uint8_t seq_bit) {
    for (int i = 0; i < ACK_ACK_SEND_COUNT; i++) {
        frame_t ack_ack_frame = {.header = {.frame_type = ACK_ACK, .seq_bit = seq_bit}};
        if (sendto(socket, &ack_ack_frame, sizeof(header_t), 0, (struct sockaddr*) dest, dest_len) < 0) {
            LOG_ERROR("Sending ACK_ACK: %s", strerror(errno));
            return -1;
        }
        usleep(ACK_ACK_DELAY_USEC);
    }
    return 0;
}

int rudp_has_pending(void) {
    for (size_t i = 0; i < MAX_RUDP_PENDING; i++) {
        if (!pending_frames[i].used)
            continue;
        frame_t* f = (frame_t*) pending_frames[i].data;
        if (f->header.frame_type == MESSAGE)
            return 1;
    }
    return 0;
}


static int receive_message_frame(
    int socket, struct sockaddr_in* source_addr, void* out_buf, size_t out_buf_size, size_t* out_len
) {
    if (pending_take_message_any(source_addr, out_buf, out_len) == 1) {
        return 0;
    }

    while (1) {
        struct sockaddr_in from = {0};
        socklen_t from_len = sizeof(from);
        ssize_t n = recvfrom(socket, out_buf, out_buf_size, MSG_DONTWAIT, (struct sockaddr*) &from, &from_len);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 1;
            }
            LOG_ERROR("Receiving rudp frame");
            return -1;
        }
        if ((size_t) n < sizeof(header_t)) {
            continue;
        }
        frame_t* frame = (frame_t*) out_buf;
        if (frame->header.frame_type != MESSAGE) {
            pending_store_frame(&from, out_buf, (size_t) n);
            continue;
        }
        *source_addr = from;
        *out_len = (size_t) n;
        return 0;
    }
}

static int wait_for_frame(
    int socket, const struct sockaddr_in* expected_addr, frame_type_t expected_type, uint8_t expected_seq,
    int timeout_us, void* out_buf, size_t out_bufsize
) {
    struct timeval start;
    gettimeofday(&start, NULL);

    if (pending_take_frame(expected_addr, expected_type, expected_seq, out_buf, out_bufsize) == 1) {
        return 1;
    }


    while (1) {
        struct timeval now;
        gettimeofday(&now, NULL);
        int elapsed = (int) ((now.tv_sec - start.tv_sec) * 1000000 + (now.tv_usec - start.tv_usec));
        int remaining = timeout_us - elapsed;
        if (remaining <= 0) {
            return 0;
        }

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(socket, &rfds);

        struct timeval timeout = {.tv_sec = 0, .tv_usec = remaining};
        int ret = select(socket + 1, &rfds, NULL, NULL, &timeout);
        if (ret < 0) {
            return -1;
        }
        if (ret == 0) {
            return 0;
        }
        if (!FD_ISSET(socket, &rfds)) {
            continue;
        }

        char frame_buf[MAX_FRAME_SIZE];
        struct sockaddr_in from = {0};
        socklen_t from_len = sizeof(from);
        ssize_t n = recvfrom(socket, frame_buf, sizeof(frame_buf), 0, (struct sockaddr*) &from, &from_len);
        if (n < 0) {
            return -1;
        }
        if ((size_t) n < sizeof(header_t)) {
            continue;
        }
        frame_t* frame = (frame_t*) frame_buf;
        if (frame->header.frame_type == MESSAGE) {
            if (pending_store_frame(&from, frame_buf, (size_t) n)) {
                send_ack_frame(socket, &from, from_len, frame->header.seq_bit);
            }
            continue;
        }
        if (addr_equal(&from, expected_addr) && frame->header.frame_type == expected_type &&
            frame->header.seq_bit == expected_seq) {
            size_t copy_len = (size_t) n < out_bufsize ? (size_t) n : out_bufsize;
            memcpy(out_buf, frame_buf, copy_len);
            return 1;
        }

        pending_store_frame(&from, frame_buf, (size_t) n);
        continue;
    }
}
