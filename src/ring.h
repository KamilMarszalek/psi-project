#ifndef RING_H
#define RING_H

#include "route.h"

typedef struct Descriptors {
    int unicast_socket;
    int broadcast_socket;
    int cli_fd;
} descriptors_t;

typedef struct {
    route_config_t config;
    join_state_t join_state;

    token_t token_in;
    int have_token;

    token_t cli_pending;
    int have_cli_pending;

    struct timeval forward_at;
} ring_state_t;

int ring_initialize(route_config_t* config, descriptors_t* descriptors);
int ring_run(route_config_t config, descriptors_t descriptors);

#endif
