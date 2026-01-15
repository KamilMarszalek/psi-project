#ifndef ROUTE_H
#define ROUTE_H

#include "consts.h"
#include <stdint.h>

typedef struct Route {
    char node_name[MAX_NODE_NAME_SIZE];
    uint16_t port;
} route_t;

typedef struct RouteConfig {
    route_t* current;
    route_t* prev;
    route_t* next;
} route_config_t;

#endif
