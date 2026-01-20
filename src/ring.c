#define _GNU_SOURCE
#include "ring.h"

#include "broadcast.h"
#include "cli.h"
#include "consts.h"
#include "join_fsm.h"
#include "logger.h"
#include "route.h"
#include "rudp.h"
#include "token.h"
#include "unicast.h"
#include "unicast_dispatch.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#define MAX3(x, y, z) (((x) > (y)) ? (((x) > (z)) ? (x) : (z)) : (((y) > (z)) ? (y) : (z)))

static int fill_config_from_env(route_config_t* config, int joined);
static int ring_on_token(ring_state_t* state, int unicast_socket);
static int set_select_timeout(const ring_state_t* state, struct timeval* timeout);
static void init_ring_state(ring_state_t* state, route_config_t config, int joined);
static int init_join_request_if_needed(ring_state_t* state, int broadcast_socket);
static int maybe_send_initial_token(const ring_state_t* state, int unicast_socket);
static int handle_broadcast_if_ready(int broadcast_socket, ring_state_t* state, const fd_set* rfds);
static int handle_cli_if_ready(int cli_fd, ring_state_t* state, const fd_set* rfds);
static int handle_unicast_if_ready(int unicast_socket, ring_state_t* state, const fd_set* rfds);
static int handle_token_if_ready(ring_state_t* state, int unicast_socket);


int ring_initialize(route_config_t* config, descriptors_t* descriptors, int joined) {
    if (fill_config_from_env(config, joined) < 0) {
        return -1;
    }

    int unicast_socket = unicast_setup_socket(config->current);
    if (unicast_socket < 0) {
        return -1;
    }

    int broadcast_socket = broadcast_setup_socket(config->current);
    if (broadcast_socket < 0) {
        return -1;
    }


    int cli_fd = cli_setup_reader(FIFO_FILE, FIFO_FILE_PERMISSIONS);
    if (cli_fd < 0) {
        return -1;
    }

    descriptors->unicast_socket = unicast_socket;
    descriptors->broadcast_socket = broadcast_socket;
    descriptors->cli_fd = cli_fd;

    return 0;
}

int ring_run(route_config_t config, descriptors_t descriptors, int joined) {
    fd_set rfds;
    struct timeval timeout;

    ring_state_t state = {0};
    init_ring_state(&state, config, joined);

    if (init_join_request_if_needed(&state, descriptors.broadcast_socket) < 0) {
        return -1;
    }
    int maxfd = MAX3(descriptors.unicast_socket, descriptors.broadcast_socket, descriptors.cli_fd);
    LOG_INFO("Node initalized, waiting for UDP packets");

    if (maybe_send_initial_token(&state, descriptors.unicast_socket) < 0) {
        return -1;
    }

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(descriptors.unicast_socket, &rfds);
        FD_SET(descriptors.broadcast_socket, &rfds);
        FD_SET(descriptors.cli_fd, &rfds);

        int use_timeout = set_select_timeout(&state, &timeout);
        int ret = select(maxfd + 1, &rfds, NULL, NULL, use_timeout ? &timeout : NULL);

        time_t now = time(NULL);
        prune_joins(&state.join_state, now, JOIN_PENDING_TTL_S);

        if (join_fsm_request_tick(&state, descriptors.broadcast_socket) < 0) {
            break;
        }

        if (join_fsm_inflight_tick(&state, descriptors.unicast_socket) < 0) {
            break;
        }

        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOG_ERROR("Reading descriptors: %s", strerror(errno));
            return -1;
        }

        if (handle_broadcast_if_ready(descriptors.broadcast_socket, &state, &rfds) < 0) {
            break;
        }

        if (handle_cli_if_ready(descriptors.cli_fd, &state, &rfds) < 0) {
            break;
        }

        if (handle_unicast_if_ready(descriptors.unicast_socket, &state, &rfds) < 0) {
            break;
        }

        if (handle_token_if_ready(&state, descriptors.unicast_socket) < 0) {
            break;
        }
    }

    return 0;
}

static int ring_on_token(ring_state_t* state, int unicast_socket) {
    token_t out = state->token_in;

    if (!out.is_empty) {
        if (strcmp(out.receiver, state->config.current->node_name) == 0) {
            LOG_INFO("Received token for me: %s\n", out.data);
            out.is_empty = true;
            memset(out.data, 0, MAX_DATA_SIZE);
            memset(out.sender, 0, MAX_NODE_NAME_SIZE);
            memset(out.receiver, 0, MAX_NODE_NAME_SIZE);
        } else {
            LOG_INFO("Full token received for another node - forwarding\n");
        }
    }

    if (state->have_cli_pending && out.is_empty) {
        LOG_INFO("Attaching CLI token: %s\n", state->cli_pending.data);
        out = state->cli_pending;
        out.topo_version = state->last_seen_topo_version;

        state->have_cli_pending = 0;
        state->cli_pending.is_empty = true;
        memset(state->cli_pending.data, 0, MAX_DATA_SIZE);
        memset(state->cli_pending.sender, 0, MAX_NODE_NAME_SIZE);
        memset(state->cli_pending.receiver, 0, MAX_NODE_NAME_SIZE);
    }

    sleep(1); // simulate processing delay
    unicast_msg_t msg = {.type = UMSG_TOKEN, .payload_len = sizeof(token_t)};
    memcpy(msg.payload, &out, sizeof(out));
    if (unicast_send(unicast_socket, &msg, state->config.next) < 0) {
        LOG_WARN("Token send failed, will retry");
        return 0;
    }

    state->have_token = 0;
    memset(&state->token_in, 0, sizeof(state->token_in));
    return 0;
}

static int fill_config_from_env(route_config_t* config, int joined) {
    char* node_name = getenv("NODE_NAME");
    char* uni_port = getenv("NODE_UNI_PORT");
    char* b_port = getenv("NODE_BROAD_PORT");

    if (!joined) {
        if (!node_name || !uni_port || !b_port) {
            LOG_ERROR("Some env variables are missing for unjoined node\n");
            return -1;
        }
        strncpy(config->current->node_name, node_name, MAX_NODE_NAME_SIZE - 1);
        config->current->node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
        config->current->broadcast_port = (uint16_t) atoi(b_port);
        config->current->unicast_port = (uint16_t) atoi(uni_port);
        return 0;
    }

    char* prev_node_name = getenv("PREV_NODE_NAME");
    char* prev_node_port = getenv("PREV_NODE_UNI_PORT");
    char* next_node_name = getenv("NEXT_NODE_NAME");
    char* next_node_port = getenv("NEXT_NODE_UNI_PORT");

    if (!node_name || !uni_port || !prev_node_name || !prev_node_port || !next_node_name || !next_node_port ||
        !b_port) {
        LOG_ERROR("Some env variables are missing");
        return -1;
    }

    strncpy(config->current->node_name, node_name, MAX_NODE_NAME_SIZE - 1);
    config->current->node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    config->current->unicast_port = (uint16_t) atoi(uni_port);

    strncpy(config->prev->node_name, prev_node_name, MAX_NODE_NAME_SIZE - 1);
    config->prev->node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    config->prev->unicast_port = (uint16_t) atoi(prev_node_port);

    strncpy(config->next->node_name, next_node_name, MAX_NODE_NAME_SIZE - 1);
    config->next->node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    config->next->unicast_port = (uint16_t) atoi(next_node_port);

    config->current->broadcast_port = (uint16_t) atoi(b_port);
    return 0;
}

static void init_ring_state(ring_state_t* state, route_config_t config, int joined) {
    *state = (ring_state_t){0};
    state->config = config;
    state->joined = joined;
    state->join_state = (join_state_t){0};
    state->last_seen_topo_version = 0;
}

static int init_join_request_if_needed(ring_state_t* state, int broadcast_socket) {
    if (state->joined) {
        return 0;
    }

    srand((unsigned int) time(NULL) ^ getpid());
    state->join_request_id = (uint32_t) rand();
    state->join_request_last_sent = 0;
    state->join_request_retries = 0;

    if (join_fsm_request_tick(state, broadcast_socket) < 0) {
        return -1;
    }

    return 0;
}

static int maybe_send_initial_token(const ring_state_t* state, int unicast_socket) {
    if (!getenv("SHOULD_START") || !state->joined) {
        return 0;
    }

    token_t token = {.is_empty = true, .topo_version = state->last_seen_topo_version};
    unicast_msg_t msg = {.type = UMSG_TOKEN, .payload_len = sizeof(token_t)};
    memcpy(msg.payload, &token, sizeof(token));
    if (unicast_send(unicast_socket, &msg, state->config.next) < 0) {
        return -1;
    }

    return 0;
}

static int handle_broadcast_if_ready(int broadcast_socket, ring_state_t* state, const fd_set* rfds) {
    if (!FD_ISSET(broadcast_socket, rfds)) {
        return 0;
    }
    if (handle_broadcast(broadcast_socket, state) < 0) {
        return -1;
    }
    return 0;
}

static int handle_cli_if_ready(int cli_fd, ring_state_t* state, const fd_set* rfds) {
    if (!FD_ISSET(cli_fd, rfds)) {
        return 0;
    }
    if (cli_handle_read(cli_fd, &state->cli_pending) < 0) {
        return -1;
    }
    state->have_cli_pending = 1;
    return 0;
}

static int handle_unicast_if_ready(int unicast_socket, ring_state_t* state, const fd_set* rfds) {
    int have_pending = rudp_has_pending();
    if (!have_pending && !FD_ISSET(unicast_socket, rfds)) {
        return 0;
    }

    int unicast_error = 0;
    do {
        unicast_msg_t msg = {0};
        int rc = unicast_recv(unicast_socket, &msg);
        if (rc > 0) {
            break;
        }
        if (rc < 0) {
            unicast_error = 1;
            break;
        }
        if (unicast_dispatch_message(state, &msg, unicast_socket) < 0) {
            unicast_error = 1;
            break;
        }
    } while (rudp_has_pending());

    if (unicast_error) {
        return -1;
    }
    return 0;
}

static int handle_token_if_ready(ring_state_t* state, int unicast_socket) {
    if (!state->joined || !state->have_token) {
        return 0;
    }

    LOG_INFO("TOKEN: have_token=1 pending=%zu inflight=%d", state->join_state.count, state->join_inflight.active);
    if (state->join_inflight.active) {
        return 0;
    }

    if (state->join_state.count > 0) {
        pending_join_t pj;
        if (pop_oldest_pending_join(&state->join_state, &pj) == 0) {
            LOG_INFO(
                "JOIN_INFLIGHT START: joiner=%s req=%u (prev=%s curr=%s)", pj.node_name, pj.request_id,
                state->config.prev->node_name, state->config.current->node_name
            );
            join_fsm_start_inflight(state, &pj);
            if (join_fsm_inflight_tick(state, unicast_socket) < 0) {
                return -1;
            }
        }
        return 0;
    }

    if (ring_on_token(state, unicast_socket) < 0) {
        return -1;
    }
    return 0;
}

static int set_select_timeout(const ring_state_t* state, struct timeval* timeout) {
    if (rudp_has_pending()) {
        timeout->tv_sec = 0;
        timeout->tv_usec = 0;
        return 1;
    }

    if (state->have_token) {
        timeout->tv_sec = 0;
        timeout->tv_usec = 0;
        return 1;
    }

    time_t now = time(NULL);
    time_t next_deadline = 0;

    if (!state->joined) {
        time_t deadline =
            state->join_request_last_sent == 0 ? now : state->join_request_last_sent + JOIN_REQUEST_TIMEOUT_S;
        next_deadline = deadline;
    }

    if (state->join_inflight.active &&
        !(state->join_inflight.got_confirm_prev && state->join_inflight.got_confirm_joiner)) {
        time_t deadline =
            state->join_inflight.last_sent == 0 ? now : state->join_inflight.last_sent + JOIN_ACCEPT_TIMEOUT_S;
        if (next_deadline == 0 || deadline < next_deadline) {
            next_deadline = deadline;
        }
    }

    if (state->join_state.count > 0) {
        time_t deadline = now + 1;
        if (next_deadline == 0 || deadline < next_deadline) {
            next_deadline = deadline;
        }
    }

    if (next_deadline == 0) {
        return 0;
    }

    if (next_deadline <= now) {
        timeout->tv_sec = 0;
        timeout->tv_usec = 0;
        return 1;
    }

    timeout->tv_sec = (int) (next_deadline - now);
    timeout->tv_usec = 0;
    return 1;
}
