#define _GNU_SOURCE
#include "rudp.h"

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define TIMEOUT_USEC 200000
#define ACK_ACK_DELAY_USEC 100000
#define ACK_ACK_RETRIES 5

rudp_state_t state = {.send_seq_bit = 0, .expected_seq_bit = 0};

int rudp_sendto(int socket, const void* buf, size_t len, const struct sockaddr_in* dest_addr, socklen_t addrlen) {
    fd_set rfds;
    struct timeval timeout;

    size_t total_size = sizeof(header_t) + len;
    char buffer[total_size];
    frame_t* frame = (frame_t*) buffer;

    frame->header.frame_type = MESSAGE;
    frame->header.seq_bit = state.send_seq_bit;
    memcpy(frame->data, buf, len);

    int ack_received = 0;
    while (!ack_received) {
        if (sendto(socket, frame, total_size, 0, (struct sockaddr*) dest_addr, addrlen) < 0) {
            perror("sending rudp frame");
            return -1;
        }

        FD_ZERO(&rfds);
        FD_SET(socket, &rfds);

        timeout.tv_sec = 0;
        timeout.tv_usec = TIMEOUT_USEC;

        int ret = select(socket + 1, &rfds, NULL, NULL, &timeout);
        if (ret < 0) {
            perror("waiting for ACK");
            return -1;
        }

        if (ret == 0) {
            printf("Timeout reached while waiting for ACK(%u) - resending message\n", state.send_seq_bit);
            continue;
        }

        if (FD_ISSET(socket, &rfds)) {
            header_t hdr;
            // TODO: handle case when ack is sent by someone else
            if (recvfrom(socket, &hdr, sizeof(header_t), 0, NULL, NULL) < 0) {
                perror("receiving ACK");
                return -1;
            }

            if (hdr.frame_type == ACK && hdr.seq_bit == state.send_seq_bit) {
                ack_received = 1;
                printf("Received correct ACK(%u)\n", hdr.seq_bit);
            }
        }
    }

    header_t ack_ack_header = {.frame_type = ACK_ACK, .seq_bit = state.send_seq_bit};
    for (int i = 0; i < ACK_ACK_RETRIES; i++) {
        if (sendto(socket, &ack_ack_header, sizeof(header_t), 0, (struct sockaddr*) dest_addr, addrlen) < 0) {
            perror("sending ACK_ACK");
            return -1;
        }
        usleep(ACK_ACK_DELAY_USEC);
    }

    state.send_seq_bit ^= 1;

    return 0;
}

int rudp_recvfrom(int socket, void* buf, size_t len, struct sockaddr_in* source_addr, socklen_t addrlen) {
    fd_set rfds;
    struct timeval timeout;

    size_t total_size = sizeof(header_t) + len;
    char buffer[total_size];

    int message_received = 0;
    while (!message_received) {
        if (recvfrom(socket, buffer, total_size, 0, (struct sockaddr*) source_addr, &addrlen) < 0) {
            perror("receiving rudp frame");
            return -1;
        }

        frame_t* frame = (frame_t*) buffer;

        if (frame->header.frame_type == MESSAGE && frame->header.seq_bit == state.expected_seq_bit) {
            message_received = 1;
            memcpy(buf, frame->data, len);
        }
    }

    int ack_ack_received = 0;
    while (!ack_ack_received) {
        header_t ack = {.frame_type = ACK, .seq_bit = state.expected_seq_bit};
        if (sendto(socket, &ack, sizeof(header_t), 0, (struct sockaddr*) source_addr, addrlen) < 0) {
            perror("sending ACK");
            return -1;
        }
        FD_ZERO(&rfds);
        FD_SET(socket, &rfds);

        timeout.tv_sec = 0;
        timeout.tv_usec = TIMEOUT_USEC;

        int ret = select(socket + 1, &rfds, NULL, NULL, &timeout);
        if (ret < 0) {
            perror("waiting for ACK ACK");
            return -1;
        }

        if (ret == 0) {
            continue;
        }

        if (FD_ISSET(socket, &rfds)) {
            header_t hdr;
            if (recvfrom(socket, &hdr, sizeof(header_t), 0, NULL, NULL) < 0) {
                perror("receiving ACK ACK");
                return -1;
            }

            if (hdr.frame_type == ACK_ACK && hdr.seq_bit == state.expected_seq_bit) {
                ack_ack_received = 1;
                printf("Received correct ACK_ACK(%u)\n", state.expected_seq_bit);
                break;
            }
        }
    }

    state.expected_seq_bit ^= 1;

    return 0;
}
