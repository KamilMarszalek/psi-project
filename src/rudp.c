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
#include <sys/types.h>
#include <unistd.h>

#define MAX_FRAME_SIZE ((1 << 16) - 1)
#define MAX_TRANSMISION_ATTEMPTS 3
#define TIMEOUT_USEC 100000
#define ACK_TIMEOUT_USEC 100000
#define ACK_ACK_DELAY_USEC 250000

static int wait_for_packet(int socket, header_t* header, int timeout_us);
static int drain_socket(int socket);

typedef struct {
    uint8_t send_seq_bit;
    uint8_t expected_seq_bit;
} rudp_state_t;

rudp_state_t state = {.send_seq_bit = 0, .expected_seq_bit = 0};

int rudp_sendto(int socket, const void* buf, size_t len, const struct sockaddr_in* dest_addr, socklen_t addrlen) {
    size_t total_size = sizeof(header_t) + len;
    if (total_size > MAX_FRAME_SIZE) {
        LOG_ERROR("Exceeded UDP datagram size (%d)", total_size);
        return -1;
    }

    frame_t* frame = malloc(total_size);
    frame->header.frame_type = MESSAGE;
    frame->header.seq_bit = state.send_seq_bit;
    memcpy(frame->data, buf, len);

    int drained = drain_socket(socket);
    if (drained != 0) {
        LOG_DEBUG("Drained %d old packets (probably stalling ACK_ACKs)", drained);
    }

    int ack_received = 0;
    int attempts = 0;
    while (!ack_received && attempts < MAX_TRANSMISION_ATTEMPTS) {
        if (sendto(socket, frame, total_size, 0, (struct sockaddr*) dest_addr, addrlen) < 0) {
            LOG_ERROR("Sending UDP frame: %s", strerror(errno));
            free(frame);
            return -1;
        }

        header_t ack_frame;
        int ack_result = wait_for_packet(socket, &ack_frame, ACK_TIMEOUT_USEC);
        if (ack_result < 0) {
            free(frame);
            return -1;
        }

        if (ack_result == 0) {
            attempts++;
            LOG_DEBUG("Timeout waiting for ACK(%u), resending message (retry number %d)", state.send_seq_bit, attempts);
            continue;
        }

        if (ack_frame.frame_type == ACK && ack_frame.seq_bit == state.send_seq_bit) {
            LOG_DEBUG("Received appropriate ACK(%u)", state.send_seq_bit);
            ack_received = 1;
        } else {
            LOG_DEBUG(
                "Received unknown package (type=%d, seq=%u) while waiting for ACK(%u)", ack_frame.frame_type,
                ack_frame.seq_bit, state.send_seq_bit
            );
        }
    }

    free(frame);

    if (ack_received == 0) {
        LOG_WARN("Failed to transfer token after %d attemps - token might be lost!", attempts);
    }

    for (int i = 0; i < MAX_TRANSMISION_ATTEMPTS; i++) {
        header_t ack_ack_frame = {.frame_type = ACK_ACK, .seq_bit = state.send_seq_bit};
        if (sendto(socket, &ack_ack_frame, sizeof(header_t), 0, (struct sockaddr*) dest_addr, addrlen) < 0) {
            LOG_ERROR("Sending ACK_ACK: %s", strerror(errno));
            return -1;
        }
        usleep(ACK_ACK_DELAY_USEC);
    }

    state.send_seq_bit ^= 1;

    return 0;
}

int rudp_recvfrom(int socket, void* buf, size_t len, struct sockaddr_in* source_addr, socklen_t addrlen) {
    size_t total_size = sizeof(header_t) + len;
    if (total_size > MAX_FRAME_SIZE) {
        LOG_ERROR("Exceeded UDP datagram size (%d)", total_size);
        return -1;
    }

    frame_t* message_frame = malloc(total_size);

    while (1) {
        if (recvfrom(socket, message_frame, total_size, 0, (struct sockaddr*) source_addr, &addrlen) < 0) {
            LOG_ERROR("Receiving rudp frame");
            free(message_frame);
            return -1;
        }

        if (message_frame->header.frame_type == MESSAGE && message_frame->header.seq_bit == state.expected_seq_bit) {
            LOG_DEBUG("Received appropriate message(%u)", state.expected_seq_bit);
            memcpy(buf, message_frame->data, len);
            break;
        }
    }

    free(message_frame);

    int ack_ack_received = 0;
    int attemps = 0;
    while (!ack_ack_received && attemps < MAX_TRANSMISION_ATTEMPTS) {
        header_t ack_frame = {.frame_type = ACK, .seq_bit = state.expected_seq_bit};
        if (sendto(socket, &ack_frame, sizeof(header_t), 0, (struct sockaddr*) source_addr, addrlen) < 0) {
            LOG_ERROR("Sending ACK(%u): %s", ack_frame.seq_bit, strerror(errno));
            perror("sending ACK");
            return -1;
        }

        header_t ack_ack_frame;
        int ack_ack_result = wait_for_packet(socket, &ack_ack_frame, TIMEOUT_USEC);
        if (ack_ack_result < 0) {
            return -1;
        }

        if (ack_ack_result == 0) {
            attemps++;
            LOG_DEBUG(
                "Timeout waiting for ACK_ACK(%u), resending message (retry number %d)", state.send_seq_bit, attemps
            );
            continue;
        }

        if (ack_ack_frame.frame_type == ACK_ACK && ack_ack_frame.seq_bit == state.expected_seq_bit) {
            LOG_DEBUG("Received appropriate ACK_ACK(%u)", state.expected_seq_bit);
            ack_ack_received = 1;
        } else {
            LOG_DEBUG(
                "Received unknown package (type=%d, seq=%u) while waiting for ACK_ACK(%u)", ack_ack_frame.frame_type,
                ack_ack_frame.seq_bit, state.expected_seq_bit
            );
        }
    }

    if (!ack_ack_received) {
        LOG_WARN("ACK-ACK was not received. Even though token is being forwarded.");
    }

    state.expected_seq_bit ^= 1;

    return 0;
}


static int wait_for_packet(int socket, header_t* header, int timeout_us) {
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(socket, &rfds);

    struct timeval timeout = {.tv_sec = 0, .tv_usec = timeout_us};

    int ret = select(socket + 1, &rfds, NULL, NULL, &timeout);
    if (ret < 0) {
        return -1;
    }

    if (ret == 0) {
        return 0;
    }

    if (FD_ISSET(socket, &rfds)) {
        if (recvfrom(socket, header, sizeof(header_t), 0, NULL, NULL) < 0) {
            return -1;
        }
        return 1;
    }

    return 0;
}

static int drain_socket(int socket) {
    fd_set rfds;
    char dummy[MAX_FRAME_SIZE];
    int drained = 0;

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(socket, &rfds);

        struct timeval timeout = {.tv_sec = 0, .tv_usec = 0};

        int ret = select(socket + 1, &rfds, NULL, NULL, &timeout);
        if (ret <= 0) {
            break;
        }

        if (FD_ISSET(socket, &rfds)) {
            if (recvfrom(socket, dummy, sizeof(dummy), 0, NULL, NULL) < 0) {
                break;
            }
            drained++;
            continue;
        }
    }
    return drained;
}
