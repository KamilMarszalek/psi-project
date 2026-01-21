#define _GNU_SOURCE
#include "unicast.h"

#include "logger.h"
#include "route.h"
#include "rudp.h"
#include "unicast_msg.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


int unicast_setup_socket(route_t* current) {
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd == -1) {
        LOG_ERROR("Opening unicast socket: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_in host_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(current->unicast_port),
    };
    socklen_t length = sizeof(host_addr);

    if (bind(sock_fd, (struct sockaddr*) &host_addr, length) < 0) {
        LOG_ERROR("Binding unicast socket: %s", strerror(errno));
        return -1;
    }

    return sock_fd;
}

int unicast_recv(int unicast_sock, unicast_msg_t* msg) {
    struct sockaddr_in prev_node_addr;
    socklen_t length = sizeof(prev_node_addr);
    unicast_msg_t wire = {0};
    int rc = rudp_recvfrom(unicast_sock, &wire, sizeof(wire), &prev_node_addr, length);
    if (rc != 0) {
        return rc;
    }
    msg->type = ntohs(wire.type);
    msg->payload_len = ntohs(wire.payload_len);
    if (msg->payload_len > MAX_UNICAST_PAYLOAD) {
        LOG_ERROR("Invalid unicast payload length: %u", msg->payload_len);
        return -1;
    }
    memcpy(msg->payload, wire.payload, msg->payload_len);
    return 0;
}

static int resolve_next_addr(const route_t* next, struct sockaddr_in* out, socklen_t* out_len) {
    *out = (struct sockaddr_in){
        .sin_family = AF_INET,
        .sin_port = htons(next->unicast_port),
    };
    *out_len = sizeof(*out);

    int res = inet_pton(AF_INET, next->node_name, &out->sin_addr);
    if (res < 0) {
        LOG_ERROR("Converting address to binary number: %s", strerror(errno));
        return -1;
    }

    if (res == 0) {
        struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM};
        struct addrinfo* addr_res;

        int err = getaddrinfo(next->node_name, NULL, &hints, &addr_res);
        if (err != 0) {
            LOG_ERROR("DNS lookup failed for: %s: %s", next->node_name, gai_strerror(err));
            return -1;
        }

        struct sockaddr_in* resolved_addr = (struct sockaddr_in*) addr_res->ai_addr;
        out->sin_addr = resolved_addr->sin_addr;
        freeaddrinfo(addr_res);
    }
    return 0;
}

static int build_unicast_wire(const unicast_msg_t* msg, unicast_msg_t* wire_out) {
    if (msg->payload_len > MAX_UNICAST_PAYLOAD) {
        LOG_ERROR("Invalid unicast payload length: %u", msg->payload_len);
        return -1;
    }

    *wire_out = (unicast_msg_t){0};
    wire_out->type = htons(msg->type);
    wire_out->payload_len = htons(msg->payload_len);
    memcpy(wire_out->payload, msg->payload, msg->payload_len);
    return 0;
}

int unicast_send(int unicast_sock, const unicast_msg_t* msg, const route_t* next) {
    struct sockaddr_in next_node_addr = {0};
    socklen_t length = 0;
    if (resolve_next_addr(next, &next_node_addr, &length) < 0) {
        return -1;
    }

    unicast_msg_t wire = {0};
    if (build_unicast_wire(msg, &wire) < 0) {
        return -1;
    }

    return rudp_sendto(unicast_sock, &wire, sizeof(wire), &next_node_addr, length);
}

int unicast_send_limited(
    int unicast_sock, const unicast_msg_t* msg, const route_t* next, int ack_timeout_us, int max_attempts
) {
    struct sockaddr_in next_node_addr = {0};
    socklen_t length = 0;
    if (resolve_next_addr(next, &next_node_addr, &length) < 0) {
        return -1;
    }

    unicast_msg_t wire = {0};
    if (build_unicast_wire(msg, &wire) < 0) {
        return -1;
    }

    return rudp_sendto_with_limits(
        unicast_sock, &wire, sizeof(wire), &next_node_addr, length, ack_timeout_us, max_attempts
    );
}
