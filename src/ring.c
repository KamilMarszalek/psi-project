#define _GNU_SOURCE

#include "ring.h"
#include "consts.h"
#include "route.h"
#include "unicast.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/select.h>

int fill_config_from_env(route_config_t* config);

int ring_initalize(route_config_t* config, sockets_t* sockets) {
    if (fill_config_from_env(config) < 0) {
        return -1;
    }

    int unicast_socket = unicast_setup(&config->current);
    if (unicast_socket < 0) {
        return -1;
    }

    sockets->unicast_socket = unicast_socket;
    return 0;
}

int ring_run(route_config_t* config, sockets_t* sockets) {
    fd_set rfds;
    struct timeval timeout;
    token_t token;

    printf("Node initalized, waiting for UDP packets.\n");

    if (getenv("SHOULD_START")) {
        printf("Sending initial empty token.\n");
        if (unicast_forward_first_token(sockets->unicast_socket, &config->next) < 0) {
            return -1;
        }
    }

    while (1) {
        fflush(stdout);

        FD_ZERO(&rfds);
        FD_SET(sockets->unicast_socket, &rfds);

        timeout.tv_sec = SELECT_TIMEOUT_S;
        timeout.tv_usec = 0;

        int ret = select(sockets->unicast_socket + 1, &rfds, NULL, NULL, &timeout);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("reading descriptors (select)");
            break;
        }

        if (ret == 0) {
            printf("Timeout waiting for token\n");
            continue;
        }

        if (FD_ISSET(sockets->unicast_socket, &rfds)) {
            if (unicast_handle(sockets->unicast_socket, &token, &config->next) < 0) {
                break;
            }
        }
    }

    return 0;
}

int fill_config_from_env(route_config_t* config) {
    char* node_name = getenv("NODE_NAME");
    char* node_port = getenv("NODE_PORT");

    char* prev_node_name = getenv("PREV_NODE_NAME");
    char* prev_node_port = getenv("PREV_NODE_PORT");

    char* next_node_name = getenv("NEXT_NODE_NAME");
    char* next_node_port = getenv("NEXT_NODE_PORT");

    if (!node_name || !node_port || !prev_node_name || !prev_node_port || !next_node_name || !next_node_port) {
        fprintf(stderr, "Some env variables are missing\n");
        return -1;
    }

    strncpy(config->current.node_name, node_name, MAX_NODE_NAME_SIZE);
    config->current.port = (uint16_t) atoi(node_port);

    strncpy(config->prev.node_name, prev_node_name, MAX_NODE_NAME_SIZE);
    config->prev.port = (uint16_t) atoi(prev_node_port);

    strncpy(config->next.node_name, next_node_name, MAX_NODE_NAME_SIZE);
    config->next.port = (uint16_t) atoi(next_node_port);

    return 0;
}
