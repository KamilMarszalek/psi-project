#include "ring.h"
#include <stdio.h>
#include <stdlib.h>

int fill_config(route_config_t* config) {
    char* node_name = getenv("NODE_NAME");
    char* node_port = getenv("NODE_PORT");

    char* prev_node_name = getenv("PREV_NODE_NAME");
    char* prev_node_port = getenv("PREV_NODE_PORT");

    char* next_node_name = getenv("NEXT_NODE_NAME");
    char* next_node_port = getenv("NEXT_NODE_PORT");

    if (!node_name || !node_port || !prev_node_name || !prev_node_port || !next_node_name || !next_node_port) {
        fprintf(stderr, "Missing env variables\n");
        fprintf(stderr,
                "Required: NODE_NAME, NODE_PORT, PREV_NODE_NAME, PREV_NODE_PORT, NEXT_NODE_NAME, NEXT_NODE_PORT\n");
        return 1;
    }

    config->current.node_name = node_name;
    config->current.port = (uint16_t) atoi(node_port);

    config->prev.node_name = prev_node_name;
    config->prev.port = (uint16_t) atoi(prev_node_port);

    config->next.node_name = next_node_name;
    config->next.port = (uint16_t) atoi(next_node_port);

    return 0;
}

int setup_ring(route_config_t* config) {
}
