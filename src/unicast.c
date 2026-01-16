#define _GNU_SOURCE
#include "unicast.h"
#include "route.h"
#include "token.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int receive_token(int unicast_socket, token_t* token);
int forward_token(int unicast_socket, token_t* token, route_t* next);

int unicast_setup_socket(route_t* current) {
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd == -1) {
        perror("opening socket");
        return -1;
    }

    struct sockaddr_in host_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(current->port),
    };
    socklen_t length = sizeof(host_addr);

    if (bind(sock_fd, (struct sockaddr*) &host_addr, length) < 0) {
        perror("binding socket");
        return -1;
    }

    return sock_fd;
}

int unicast_forward_first_token(int unicast_socket, route_t* next) {
    token_t token = {0};
    token.is_empty = true;

    if (forward_token(unicast_socket, &token, next) < 0) {
        return -1;
    }

    return 0;
}


int unicast_handle(int unicast_socket, token_pair_t tokens, route_config_t config) {
    if (receive_token(unicast_socket, tokens.from_unicast) < 0) {
        return -1;
    };

    token_t token_to_send = *tokens.from_unicast;

    if (!tokens.from_unicast->is_empty) {
        if (strcmp(tokens.from_unicast->reciever, config.current->node_name) == 0) {
            printf("Received message from %s: %s\n", tokens.from_unicast->sender, tokens.from_unicast->data);
            memset(&token_to_send, 0, sizeof(token_t));
            token_to_send.is_empty = true;
        } else {
            printf("Forwarding already filled token\n");
        }

    } else if (tokens.from_unicast->is_empty) {
        printf("Received empty token\n");
        if (!tokens.from_cli->is_empty) {
            token_to_send = *tokens.from_cli;
            memset(tokens.from_cli, 0, sizeof(token_t));
            tokens.from_cli->is_empty = true;
            printf("Filled token with data: receiver=%s; data=%s\n", token_to_send.reciever, token_to_send.data);
        }
    }

    sleep(1);//simulate delay
    if (forward_token(unicast_socket, &token_to_send, config.next) < 0) {
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
