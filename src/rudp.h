#ifndef RUDP_H
#define RUDP_H

#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_FRAME_DATA (1 << 10)

typedef enum FrameType {
    MESSAGE,
    ACK,
    ACK_ACK,
} frame_type_t;

typedef struct Header {
    frame_type_t frame_type;
    uint8_t seq_bit;
} header_t;

typedef struct Frame {
    header_t header;
    char data[];
} frame_t;

typedef struct RUDPState {
    uint8_t send_seq_bit;
    uint8_t expected_seq_bit;
} rudp_state_t;

int rudp_sendto(int socket, const void* buf, size_t len, const struct sockaddr_in* dest_addr, socklen_t addrlen);
int rudp_recvfrom(int socket, void* buf, size_t len, struct sockaddr_in* source_addr, socklen_t addrlen);

#endif
