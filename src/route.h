#ifndef ROUTE_H
#define ROUTE_H

#include <stdint.h>

#define MAX_NODE_NAME_SIZE 32

typedef struct Route {
    char node_name[MAX_NODE_NAME_SIZE];
    uint16_t port;
} route_t;


typedef struct RouteConfig {
    route_t current;
    route_t prev;
    route_t next;
} route_config_t;


int fill_config(route_config_t* config);

#endif
