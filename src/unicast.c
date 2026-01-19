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

int unicast_send(int unicast_sock, const unicast_msg_t* msg, const route_t* next) {
    struct sockaddr_in next_node_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(next->unicast_port),
    };
    socklen_t length = sizeof(next_node_addr);

    int res = inet_pton(AF_INET, next->node_name, &next_node_addr.sin_addr);
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
        next_node_addr.sin_addr = resolved_addr->sin_addr;
        freeaddrinfo(addr_res);
    }

    if (msg->payload_len > MAX_UNICAST_PAYLOAD) {
        LOG_ERROR("Invalid unicast payload length: %u", msg->payload_len);
        return -1;
    }

    unicast_msg_t wire = {0};
    wire.type = htons(msg->type);
    wire.payload_len = htons(msg->payload_len);
    memcpy(wire.payload, msg->payload, msg->payload_len);

    return rudp_sendto(unicast_sock, &wire, sizeof(wire), &next_node_addr, length);
}
