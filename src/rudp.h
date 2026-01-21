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


int rudp_sendto(int socket, const void* buf, size_t len, const struct sockaddr_in* dest_addr, socklen_t addrlen);
int rudp_sendto_with_limits(
    int socket, const void* buf, size_t len, const struct sockaddr_in* dest_addr, socklen_t addrlen,
    int ack_timeout_us, int max_attempts
);
int rudp_recvfrom(int socket, void* buf, size_t len, struct sockaddr_in* source_addr, socklen_t addrlen);
int rudp_has_pending(void);

#endif
