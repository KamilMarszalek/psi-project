#ifndef RING_H
#define RING_H

#include "route.h"

typedef struct Descriptors {
    int unicast_socket;
    // int broadcast_socket;
    int cli_fd;
} descriptors_t;

int ring_initalize(route_config_t* config, descriptors_t* descriptors);
int ring_run(route_config_t* config, descriptors_t* descriptors);

#endif
