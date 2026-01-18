#define _GNU_SOURCE
#include "ring.h"

#include "cli.h"
#include "consts.h"
#include "logger.h"
#include "route.h"
#include "token.h"
#include "unicast.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

int fill_config_from_env(route_config_t* config);

int ring_initalize(route_config_t* config, descriptors_t* descriptors) {
    if (fill_config_from_env(config) < 0) {
        return -1;
    }

    int unicast_socket = unicast_setup_socket(config->current);
    if (unicast_socket < 0) {
        return -1;
    }

    int cli_fd = cli_setup_reader(FIFO_FILE, FIFO_FILE_PERMISSIONS);
    if (cli_fd < 0) {
        return -1;
    }

    descriptors->unicast_socket = unicast_socket;
    descriptors->cli_fd = cli_fd;

    return 0;
}

int ring_run(route_config_t* config, descriptors_t* descriptors) {
    fd_set rfds;
    token_t from_unicast = {.is_empty = true};
    token_t from_cli = {.is_empty = true};
    token_pair_t tokens = {.from_unicast = &from_unicast, .from_cli = &from_cli};

    int maxfd = MAX(descriptors->unicast_socket, descriptors->cli_fd);

    LOG_INFO("Node initalized, waiting for UDP packets");

    if (getenv("SHOULD_START")) {
        if (unicast_forward_first_token(descriptors->unicast_socket, config->next) < 0) {
            return -1;
        }
    }

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(descriptors->unicast_socket, &rfds);
        FD_SET(descriptors->cli_fd, &rfds);

        int ret = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOG_ERROR("Reading descriptors: %s", strerror(errno));
            return -1;
        }

        // TODO: add broadcast handling

        if (FD_ISSET(descriptors->cli_fd, &rfds)) {
            if (cli_handle_read(descriptors->cli_fd, &from_cli) < 0) {
                return -1;
            }
        }

        if (FD_ISSET(descriptors->unicast_socket, &rfds)) {
            if (unicast_handle(descriptors->unicast_socket, &tokens, config) < 0) {
                return -1;
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
        LOG_ERROR("Some env variables are missing");
        return -1;
    }

    strncpy(config->current->node_name, node_name, MAX_NODE_NAME_SIZE - 1);
    config->current->port = (uint16_t) atoi(node_port);

    strncpy(config->prev->node_name, prev_node_name, MAX_NODE_NAME_SIZE - 1);
    config->prev->port = (uint16_t) atoi(prev_node_port);

    strncpy(config->next->node_name, next_node_name, MAX_NODE_NAME_SIZE - 1);
    config->next->port = (uint16_t) atoi(next_node_port);

    return 0;
}
