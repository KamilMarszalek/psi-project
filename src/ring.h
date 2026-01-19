#ifndef RING_H
#define RING_H

#include "join.h"
#include "route.h"
#include "token.h"
#include <sys/time.h>

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
    join_inflight_t join_inflight;

    int joined;

    uint32_t join_request_id;
    time_t join_request_last_sent;
    int join_request_retries;

    uint32_t last_seen_topo_version;
} ring_state_t;

int ring_initialize(route_config_t* config, descriptors_t* descriptors, int joined);
int ring_run(route_config_t config, descriptors_t descriptors, int joined);

#endif
