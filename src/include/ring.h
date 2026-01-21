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
    int active;
    uint32_t request_id;
    char coord_name[MAX_NODE_NAME_SIZE];
    uint16_t coord_unicast_port;

    time_t last_sent;
    int retries;
    int got_ack_ack;
} join_ack_sender_t;

typedef struct {
    route_config_t config;
    int joined;
    uint32_t last_seen_topo_version;
    int broadcast_socket;

    join_state_t join_state;
    join_inflight_t join_inflight;
    join_ack_sender_t ack_sender;
    uint32_t join_request_id;
    time_t join_request_last_sent;
    int join_request_retries;

    token_t token_in;
    int have_token;

    token_t cli_pending;
    int have_cli_pending;

    struct timeval forward_at;
} ring_state_t;

int ring_initialize(route_config_t* config, descriptors_t* descriptors, int joined);
int ring_run(route_config_t config, descriptors_t descriptors, int joined);

#endif
