#define _GNU_SOURCE

#include "route.h"
#include "token.h"

#include <netdb.h>
#include <stdio.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

int receive_token(int unicast_socket, token_t* token);
int forward_token(int unicast_socket, token_t* token, route_t* next);

int unicast_setup(route_t* config) {
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd == -1) {
        perror("opening socket");
        return -1;
    }

    struct sockaddr_in host_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(config->port),
    };
    socklen_t length = sizeof(host_addr);

    if (bind(sock_fd, (struct sockaddr*) &host_addr, length) < 0) {
        perror("binding socket");
        return -1;
    }

    return sock_fd;
}

int unicast_send_first_token(int unicast_socket, route_t* next) {
    token_t token = {0};
    token.is_empty = true;

    if (forward_token(unicast_socket, &token, next) < 0) {
        return -1;
    }

    return 0;
}


int unicast_handle(int unicast_socket, token_t* token, route_t* next) {
    if (receive_token(unicast_socket, token) < 0) {
        return -1;
    };
    // TODO: Consume if user is the receiver, eventually fill token if it is empty
    if (token->is_empty) {
        printf("Received empty token\n");
    } else {
        printf("Receieved token with message: %s that was sent by %s\n", token->data, token->sender);
    }
    sleep(1);//simulate delay
    if (forward_token(unicast_socket, token, next) < 0) {
        return -1;
    };

    return 0;
}

int receive_token(int unicast_sock, token_t* token) {
    struct sockaddr_in prev_node_addr;
    socklen_t length = sizeof(prev_node_addr);
    ssize_t n_recv = recvfrom(unicast_sock, token, sizeof(*token), 0, (struct sockaddr*) &prev_node_addr, &length);
    if (n_recv < 0) {
        perror("receiving token");
        return -1;
    }
    return 0;
}

int forward_token(int unicast_sock, token_t* token, route_t* next) {
    struct sockaddr_in next_node_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(next->port),
    };
    socklen_t length = sizeof(next_node_addr);

    int res = inet_pton(AF_INET, next->node_name, &next_node_addr.sin_addr);
    if (res < 0) {
        perror("converting address to binary number");
        return -1;
    }

    if (res == 0) {
        struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM};
        struct addrinfo* addr_res;

        int err = getaddrinfo(next->node_name, NULL, &hints, &addr_res);
        if (err != 0) {
            fprintf(stderr, "DNS lookup failed for: %s: %s\n", next->node_name, gai_strerror(err));
            return -1;
        }

        struct sockaddr_in* resolved_addr = (struct sockaddr_in*) addr_res->ai_addr;
        next_node_addr.sin_addr = resolved_addr->sin_addr;
        freeaddrinfo(addr_res);
    }

    // TODO: Reliable UDP is needed here
    if (sendto(unicast_sock, token, sizeof(*token), 0, (struct sockaddr*) &next_node_addr, length) < 0) {
        perror("sending token");
        return -1;
    }

    return 0;
}
