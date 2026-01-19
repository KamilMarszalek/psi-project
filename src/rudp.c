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
#define MAX_TRANSMISION_ATTEMPTS 2000
#define TIMEOUT_USEC 500000
#define ACK_TIMEOUT_USEC 500000
#define ACK_ACK_WAIT_USEC 100000
#define MAX_RUDP_PEERS 32
#define MAX_RUDP_PENDING 64

static int wait_for_frame(
    int socket, const struct sockaddr_in* expected_addr, frame_type_t expected_type, uint8_t expected_seq,
    int timeout_us, void* out_buf, size_t out_bufsize
);
static int pending_take_message_any(struct sockaddr_in* addr_out, void* out_buf, size_t* out_len);
static void pending_store_message(const struct sockaddr_in* addr, const void* buf, size_t len);
static void ack_incoming_message(int socket, const struct sockaddr_in* from, socklen_t from_len, uint8_t seq_bit);

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

    frame_t ack_ack_frame = {.header = {.frame_type = ACK_ACK, .seq_bit = peer->state.send_seq_bit}};
    if (sendto(socket, &ack_ack_frame, sizeof(header_t), 0, (struct sockaddr*) dest_addr, addrlen) < 0) {
        LOG_ERROR("Sending ACK_ACK: %s", strerror(errno));
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

        if (pending_take_message_any(source_addr, frame_buf, &frame_len) == 0) {
            while (1) {
                struct sockaddr_in from = {0};
                socklen_t from_len = sizeof(from);
                ssize_t n = recvfrom(
                    socket, frame_buf, sizeof(frame_buf), MSG_DONTWAIT, (struct sockaddr*) &from, &from_len
                );
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
                frame_t* frame = (frame_t*) frame_buf;
                if (frame->header.frame_type != MESSAGE) {
                    continue;
                }
                *source_addr = from;
                frame_len = (size_t) n;
                break;
            }
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

        frame_t ack_frame = {.header = {.frame_type = ACK, .seq_bit = message_frame->header.seq_bit}};
        if (sendto(socket, &ack_frame, sizeof(header_t), 0, (struct sockaddr*) source_addr, addrlen) < 0) {
            LOG_ERROR("Sending ACK(%u): %s", ack_frame.header.seq_bit, strerror(errno));
            return -1;
        }

        frame_t ack_ack_frame;
        int ack_ack_result = wait_for_frame(
            socket, source_addr, ACK_ACK, message_frame->header.seq_bit, ACK_ACK_WAIT_USEC, &ack_ack_frame,
            sizeof(ack_ack_frame)
        );
        if (ack_ack_result < 0) {
            return -1;
        }
        if (ack_ack_result == 0) {
            LOG_WARN("ACK-ACK was not received. Even though token is being forwarded.");
        }

        if (is_expected) {
            peer->state.expected_seq_bit ^= 1;
            return 0;
        }
    }
}


static int pending_take_message_any(struct sockaddr_in* addr_out, void* out_buf, size_t* out_len) {
    for (size_t i = 0; i < MAX_RUDP_PENDING; i++) {
        if (!pending_frames[i].used) {
            continue;
        }
        frame_t* frame = (frame_t*) pending_frames[i].data;
        if (frame->header.frame_type != MESSAGE) {
            pending_frames[i].used = 0;
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

static void pending_store_message(const struct sockaddr_in* addr, const void* buf, size_t len) {
    if (len > MAX_FRAME_SIZE || len < sizeof(header_t)) {
        return;
    }
    frame_t* frame = (frame_t*) buf;
    if (frame->header.frame_type != MESSAGE) {
        return;
    }
    for (size_t i = 0; i < MAX_RUDP_PENDING; i++) {
        if (!pending_frames[i].used) {
            pending_frames[i].used = 1;
            pending_frames[i].addr = *addr;
            pending_frames[i].len = len;
            memcpy(pending_frames[i].data, buf, len);
            return;
        }
    }
    LOG_WARN("RUDP pending buffer full, dropping message");
}

static void ack_incoming_message(int socket, const struct sockaddr_in* from, socklen_t from_len, uint8_t seq_bit) {
    frame_t ack_frame = {.header = {.frame_type = ACK, .seq_bit = seq_bit}};
    if (sendto(socket, &ack_frame, sizeof(header_t), 0, (struct sockaddr*) from, from_len) < 0) {
        LOG_ERROR("Sending ACK(%u): %s", ack_frame.header.seq_bit, strerror(errno));
    }
}

int rudp_has_pending(void) {
    for (size_t i = 0; i < MAX_RUDP_PENDING; i++) {
        if (pending_frames[i].used) {
            return 1;
        }
    }
    return 0;
}

static int wait_for_frame(
    int socket, const struct sockaddr_in* expected_addr, frame_type_t expected_type, uint8_t expected_seq,
    int timeout_us, void* out_buf, size_t out_bufsize
) {
    struct timeval start;
    gettimeofday(&start, NULL);

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
            ack_incoming_message(socket, &from, from_len, frame->header.seq_bit);
            pending_store_message(&from, frame_buf, (size_t) n);
            continue;
        }
        if (addr_equal(&from, expected_addr) && frame->header.frame_type == expected_type &&
            frame->header.seq_bit == expected_seq) {
            size_t copy_len = (size_t) n < out_bufsize ? (size_t) n : out_bufsize;
            memcpy(out_buf, frame_buf, copy_len);
            return 1;
        }
    }
}
