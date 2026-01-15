#define _GNU_SOURCE

#include "ring.h"

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#define TIMEOUT_S 5

int max3(int num1, int num2, int num3) {
    int max = num1;
    if (num2 > max) {
        max = num2;
    }
    if (num3 > max) {
        max = num3;
    }
    return max;
}

int setup_unicast(route_t* config) {
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

int receive_token(route_t* config, int unicast_sock, token_t* token) {
    struct sockaddr_in prev_node_addr;
    socklen_t length = sizeof(prev_node_addr);
    ssize_t n_recv = recvfrom(unicast_sock, token, sizeof(*token), 0, (struct sockaddr*) &prev_node_addr, &length);
    if (n_recv < 0) {
        perror("receiving token");
        return -1;
    }
    return 0;
}

int forward_token(route_t* config, int unicast_sock, token_t* token) {
    struct sockaddr_in next_node_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(config->port),
    };
    socklen_t length = sizeof(next_node_addr);

    int res = inet_pton(AF_INET, config->node_name, &next_node_addr.sin_addr);
    if (res < 0) {
        perror("converting address to binary number");
        return -1;
    }

    if (res == 0) {
        struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_DGRAM};
        struct addrinfo* addr_res;

        int err = getaddrinfo(config->node_name, NULL, &hints, &addr_res);
        if (err != 0) {
            fprintf(stderr, "DNS lookup failed for: %s: %s\n", config->node_name, gai_strerror(err));
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

int event_loop(route_config_t* config, sockets_t* sockets) {
    fd_set rfds;
    struct timeval timeout;
    token_t token;

    // int maxfd = max3(sockets->unicast_socket, sockets->broadcast_socket, sockets->cli_socket);

    printf("Node %s entering event loop, waiting for token...\n", config->current.node_name);

    while (1) {
        fflush(stdout);

        FD_ZERO(&rfds);
        FD_SET(sockets->unicast_socket, &rfds);

        timeout.tv_sec = TIMEOUT_S;
        timeout.tv_usec = 0;

        int ret = select(sockets->unicast_socket + 1, &rfds, NULL, NULL, &timeout);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("reading descriptors (select)");
            return -1;
        }

        if (ret == 0) {
            printf("Timeout waiting for token\n");
            continue;
        }

        if (FD_ISSET(sockets->unicast_socket, &rfds)) {
            if (receive_token(&config->prev, sockets->unicast_socket, &token) < 0) {
                return -1;
            };
            // TODO: Consume if user is the receiver, eventually fill token if it is empty
            printf("Receieved token with message: %s that was sent by %s\n", token.data, token.sender);
            sleep(1);//simulate delay
            if (forward_token(&config->next, sockets->unicast_socket, &token) < 0) {
                return -1;
            };
        }
    }

    return 0;
}
