#ifndef RING_H
#define RING_H

#include "route.h"

typedef struct Sockets {
    int unicast_socket;
    // int broadcast_socket;
    // int cli_socket;
} sockets_t;

int ring_initalize(route_config_t* config, sockets_t* sockets);
int ring_run(route_config_t* config, sockets_t* sockets);

#endif
