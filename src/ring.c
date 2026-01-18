#define _GNU_SOURCE
#include "ring.h"

#include "broadcast.h"
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

#define MAX3(x, y, z) (((x) > (y)) ? (((x) > (z)) ? (x) : (z)) : (((y) > (z)) ? (y) : (z)))

int fill_config_from_env(route_config_t* config, int joined);
int ring_on_token(ring_state_t* state, int unicast_socket);

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

static int join_request_tick(ring_state_t* st, int broadcast_socket) {
    if (st->joined)
        return 0;

    time_t now = time(NULL);
    if (st->join_request_last_sent != 0 && now - st->join_request_last_sent < JOIN_REQUEST_TIMEOUT_S) {
        return 0;
    }

    if (st->join_request_retries >= JOIN_REQUEST_RETRIES) {
        fprintf(stderr, "JOIN_REQUEST retries exhausted (id=%u)\n", st->join_request_id);
        return -1;
    }

    if (broadcast_send_join_request(
            broadcast_socket, st->config.current->broadcast_port, st->join_request_id, st->config.current->node_name,
            st->config.current->unicast_port
        ) < 0) {
        return -1;
    }

    st->join_request_last_sent = now;
    st->join_request_retries++;
    return 0;
}


int ring_run(route_config_t config, descriptors_t descriptors, int joined) {
    fd_set rfds;
    struct timeval timeout;
    ring_state_t state = {0};
    state.config = config;
    state.joined = joined;

    if (!state.joined) {
        srand((unsigned int) time(NULL) ^ getpid());
        state.join_request_id = (uint32_t) rand();
        state.join_request_last_sent = 0;
        state.join_request_retries = 0;
        if (join_request_tick(&state, descriptors.broadcast_socket) < 0) {
            return -1;
        }
    }
    state.join_state = (join_state_t){0};
    token_t from_unicast = {.is_empty = true};
    token_t from_cli = {.is_empty = true};
    token_pair_t tokens = {.from_unicast = &from_unicast, .from_cli = &from_cli};

    int maxfd = MAX3(descriptors.unicast_socket, descriptors.broadcast_socket, descriptors.cli_fd);

    LOG_INFO("Node initalized, waiting for UDP packets");

    if (getenv("SHOULD_START") && state.joined) {
        token_t token = {.is_empty = true};
        if (unicast_send(descriptors.unicast_socket, &token, config.next) < 0) {
            return -1;
        }
    }

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(descriptors.unicast_socket, &rfds);
        FD_SET(descriptors.broadcast_socket, &rfds);
        FD_SET(descriptors.cli_fd, &rfds);

        timeout.tv_sec = SELECT_TIMEOUT_S;
        timeout.tv_usec = 0;

        int ret = select(maxfd + 1, &rfds, NULL, NULL, &timeout);
        time_t now = time(NULL);
        prune_joins(&state.join_state, now, JOIN_PENDING_TTL_S);
        if (join_request_tick(&state, descriptors.broadcast_socket) < 0) {
            break;
        }
        if (join_inflight_tick(&state, descriptors.broadcast_socket) < 0) {
            break;
        }

        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOG_ERROR("Reading descriptors: %s", strerror(errno));
            return -1;
        }

        if (FD_ISSET(descriptors.broadcast_socket, &rfds)) {
            if (handle_broadcast(descriptors.broadcast_socket, &state) < 0) {
                break;
            }
        }

        if (FD_ISSET(descriptors.cli_fd, &rfds)) {
            if (cli_handle_read(descriptors.cli_fd, &state.cli_pending) < 0) {
                break;
            }
            state.have_cli_pending = 1;
        }

        if (FD_ISSET(descriptors.unicast_socket, &rfds)) {
            if (!state.joined) {
                token_t tmp;
                (void) unicast_recv(descriptors.unicast_socket, &tmp);
                continue;
            }
            if (unicast_recv(descriptors.unicast_socket, &state.token_in) < 0) {
                break;
            }
            state.have_token = 1;

            ring_on_token(&state, descriptors.unicast_socket);
        }
    }

    return 0;
}

static void start_join_inflight(ring_state_t* state, pending_join_t* pj) {
    state->join_inflight.active = 1;
    state->join_inflight.request_id = pj->request_id;
    state->join_inflight.joiner = *pj;

    strncpy(state->join_inflight.expected_prev_name, state->config.prev->node_name, MAX_NODE_NAME_SIZE - 1);
    state->join_inflight.expected_prev_name[MAX_NODE_NAME_SIZE - 1] = '\0';

    state->join_inflight.got_ack_prev = 0;
    state->join_inflight.got_ack_joiner = 0;

    state->join_inflight.last_sent = 0;
    state->join_inflight.retries = 0;
}

int ring_on_token(ring_state_t* state, int unicast_socket) {
    token_t out = state->token_in;
    if (!state->join_inflight.active && state->join_state.count > 0) {
        pending_join_t pj;
        if (pop_oldest_pending_join(&state->join_state, &pj) == 0) {
            start_join_inflight(state, &pj);
        }
    }
    if (state->join_inflight.active) {}
    if (!out.is_empty) {
        if (strcmp(out.receiver, state->config.current->node_name) == 0) {
            printf("Received token for me: %s\n", out.data);
            out.is_empty = true;
            memset(out.data, 0, MAX_DATA_SIZE);
            memset(out.sender, 0, MAX_NODE_NAME_SIZE);
            memset(out.receiver, 0, MAX_NODE_NAME_SIZE);
        } else {
            printf("Full token received for another node - forwarding\n");
        }
    }
    if (state->have_cli_pending && out.is_empty) {
        printf("Attaching CLI token: %s\n", state->cli_pending.data);
        out = state->cli_pending;
        state->have_cli_pending = 0;
        state->cli_pending.is_empty = true;
        memset(state->cli_pending.data, 0, MAX_DATA_SIZE);
        memset(state->cli_pending.sender, 0, MAX_NODE_NAME_SIZE);
        memset(state->cli_pending.receiver, 0, MAX_NODE_NAME_SIZE);
    }
    sleep(1);// simulate processing delay
    if (unicast_send(unicast_socket, &out, state->config.next) < 0) {
        return -1;
    }
    state->have_token = 0;
    return 0;
}

int fill_config_from_env(route_config_t* config, int joined) {
    char* node_name = getenv("NODE_NAME");
    char* uni_port = getenv("NODE_UNI_PORT");

    char* b_port = getenv("NODE_BROAD_PORT");

    if (!joined) {
        if (!node_name || !uni_port || !b_port) {
            fprintf(stderr, "Some env variables are missing for unjoined node\n");
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

    if (!node_name || !uni_port || !prev_node_name || !prev_node_port || !next_node_name || !next_node_port) {
        LOG_ERROR("Some env variables are missing");
        return -1;
    }

    strncpy(config->current->node_name, node_name, MAX_NODE_NAME_SIZE - 1);
    config->current->unicast_port = (uint16_t) atoi(uni_port);

    strncpy(config->prev->node_name, prev_node_name, MAX_NODE_NAME_SIZE - 1);
    config->prev->unicast_port = (uint16_t) atoi(prev_node_port);

    strncpy(config->next->node_name, next_node_name, MAX_NODE_NAME_SIZE - 1);
    config->next->unicast_port = (uint16_t) atoi(next_node_port);
    config->current->broadcast_port = (uint16_t) atoi(b_port);

    return 0;
}
