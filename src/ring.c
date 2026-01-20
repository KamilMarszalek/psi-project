#define _GNU_SOURCE
#include "ring.h"

#include "broadcast.h"
#include "cli.h"
#include "consts.h"
#include "logger.h"
#include "route.h"
#include "rudp.h"
#include "token.h"
#include "unicast.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#define MAX3(x, y, z) (((x) > (y)) ? (((x) > (z)) ? (x) : (z)) : (((y) > (z)) ? (y) : (z)))

int fill_config_from_env(route_config_t* config, int joined);
int ring_on_token(ring_state_t* state, int unicast_socket);
static void start_join_inflight(ring_state_t* state, pending_join_t* pj);
static int join_inflight_tick(ring_state_t* state, int unicast_socket);
static int handle_join_accept_unicast(ring_state_t* state, const join_accept_t* accept, int unicast_socket);
static void handle_join_confirm_unicast(ring_state_t* state, const join_confirm_t* confirm);
static int set_select_timeout(const ring_state_t* state, struct timeval* out_timeout);


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
        LOG_ERROR("JOIN_REQUEST retries exhausted (id=%u)\n", st->join_request_id);
        return -1;
    }

    if (broadcast_send_join_request(
            broadcast_socket, st->join_request_id, st->config.current->node_name, st->config.current->unicast_port
        ) < 0) {
        LOG_ERROR("Failed to send JOIN_REQUEST\n");
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
    state.join_state = (join_state_t){0};
    state.last_seen_topo_version = 0;

    if (!state.joined) {
        srand((unsigned int) time(NULL) ^ getpid());
        state.join_request_id = (uint32_t) rand();
        state.join_request_last_sent = 0;
        state.join_request_retries = 0;

        if (join_request_tick(&state, descriptors.broadcast_socket) < 0) {
            return -1;
        }
    }
    int maxfd = MAX3(descriptors.unicast_socket, descriptors.broadcast_socket, descriptors.cli_fd);
    LOG_INFO("Node initalized, waiting for UDP packets");

    if (getenv("SHOULD_START") && state.joined) {
        token_t token = {.is_empty = true, .topo_version = state.last_seen_topo_version};
        unicast_msg_t msg = {.type = UMSG_TOKEN, .payload_len = sizeof(token_t)};
        memcpy(msg.payload, &token, sizeof(token));
        if (unicast_send(descriptors.unicast_socket, &msg, config.next) < 0) {
            return -1;
        }
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

        if (join_request_tick(&state, descriptors.broadcast_socket) < 0) {
            break;
        }

        if (join_inflight_tick(&state, descriptors.unicast_socket) < 0) {
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

        int have_pending = rudp_has_pending();
        if (have_pending || FD_ISSET(descriptors.unicast_socket, &rfds)) {
            int unicast_error = 0;
            do {
                unicast_msg_t msg = {0};
                int rc = unicast_recv(descriptors.unicast_socket, &msg);
                if (rc > 0) {
                    break;
                }
                if (rc < 0) {
                    unicast_error = 1;
                    break;
                }
                if (msg.type == UMSG_TOKEN) {
                    if (msg.payload_len != sizeof(token_t)) {
                        LOG_ERROR("Invalid token payload length: %u", msg.payload_len);
                    } else {
                        memcpy(&state.token_in, msg.payload, sizeof(token_t));
                        if (state.joined) {
                            if (state.token_in.topo_version > state.last_seen_topo_version) {
                                state.last_seen_topo_version = state.token_in.topo_version;
                                state.join_state = (join_state_t){0};
                                LOG_INFO(
                                    "Topo version advanced to %u, cleared pending joins", state.last_seen_topo_version
                                );
                            }
                            state.have_token = 1;
                            LOG_INFO("Received token via unicast\n");
                        }
                    }
                } else if (msg.type == UMSG_JOIN_ACCEPT) {
                    if (msg.payload_len != sizeof(join_accept_t)) {
                        LOG_ERROR("Invalid JOIN_ACCEPT payload length: %u", msg.payload_len);
                    } else {
                        join_accept_t accept_wire;
                        memcpy(&accept_wire, msg.payload, sizeof(accept_wire));
                        if (ntohl(accept_wire.header.magic) != JOIN_MAGIC ||
                            ntohs(accept_wire.header.type) != JOIN_ACCEPT) {
                            LOG_ERROR("Invalid JOIN_ACCEPT magic");
                        } else {
                            join_accept_t accept = {0};
                            accept.header.request_id = ntohl(accept_wire.header.request_id);
                            accept.header.type = JOIN_ACCEPT;
                            accept.header.magic = JOIN_MAGIC;
                            strncpy(accept.new_name, accept_wire.new_name, MAX_NODE_NAME_SIZE - 1);
                            strncpy(accept.before_name, accept_wire.before_name, MAX_NODE_NAME_SIZE - 1);
                            strncpy(accept.prev_name, accept_wire.prev_name, MAX_NODE_NAME_SIZE - 1);
                            accept.new_unicast_port = ntohs(accept_wire.new_unicast_port);
                            accept.before_unicast_port = ntohs(accept_wire.before_unicast_port);
                            accept.prev_unicast_port = ntohs(accept_wire.prev_unicast_port);
                            accept.new_name[MAX_NODE_NAME_SIZE - 1] = '\0';
                            accept.before_name[MAX_NODE_NAME_SIZE - 1] = '\0';
                            accept.prev_name[MAX_NODE_NAME_SIZE - 1] = '\0';

                            if (handle_join_accept_unicast(&state, &accept, descriptors.unicast_socket) < 0) {
                                break;
                            }
                        }
                    }
                } else if (msg.type == UMSG_JOIN_CONFIRM) {
                    if (msg.payload_len != sizeof(join_confirm_t)) {
                        LOG_ERROR("Invalid JOIN_CONFIRM payload length: %u", msg.payload_len);
                    } else {
                        join_confirm_t confirm_wire;
                        memcpy(&confirm_wire, msg.payload, sizeof(confirm_wire));
                        join_confirm_t confirm = {0};
                        confirm.request_id = ntohl(confirm_wire.request_id);
                        strncpy(confirm.from_name, confirm_wire.from_name, MAX_NODE_NAME_SIZE - 1);
                        confirm.from_name[MAX_NODE_NAME_SIZE - 1] = '\0';
                        handle_join_confirm_unicast(&state, &confirm);
                    }
                } else {
                    LOG_WARN("Unknown unicast message type: %u", msg.type);
                }
            } while (rudp_has_pending());
            if (unicast_error) {
                break;
            }
        }

        if (state.joined && state.have_token) {
            LOG_INFO("TOKEN: have_token=1 pending=%zu inflight=%d", state.join_state.count, state.join_inflight.active);
            if (!state.join_inflight.active && state.join_state.count > 0) {
                pending_join_t pj;
                if (pop_oldest_pending_join(&state.join_state, &pj) == 0) {
                    LOG_INFO(
                        "JOIN_INFLIGHT START: joiner=%s req=%u (prev=%s curr=%s)", pj.node_name, pj.request_id,
                        state.config.prev->node_name, state.config.current->node_name
                    );
                    start_join_inflight(&state, &pj);
                    if (join_inflight_tick(&state, descriptors.unicast_socket) < 0) {
                        break;
                    }
                }
            }

            if (!state.join_inflight.active) {
                if (ring_on_token(&state, descriptors.unicast_socket) < 0) {
                    break;
                }
            }
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

    state->join_inflight.got_confirm_prev = 0;
    state->join_inflight.got_confirm_joiner = 0;

    state->join_inflight.last_sent = 0;
    state->join_inflight.retries = 0;
}

int ring_on_token(ring_state_t* state, int unicast_socket) {
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

int fill_config_from_env(route_config_t* config, int joined) {
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

static int send_join_confirm(
    int unicast_socket, const char* coord_name, uint16_t coord_port, uint32_t request_id, const char* from_name
) {
    join_confirm_t confirm = {0};
    confirm.request_id = htonl(request_id);
    strncpy(confirm.from_name, from_name, MAX_NODE_NAME_SIZE - 1);
    confirm.from_name[MAX_NODE_NAME_SIZE - 1] = '\0';

    unicast_msg_t msg = {.type = UMSG_JOIN_CONFIRM, .payload_len = sizeof(confirm)};
    memcpy(msg.payload, &confirm, sizeof(confirm));

    route_t coord = {0};
    strncpy(coord.node_name, coord_name, MAX_NODE_NAME_SIZE - 1);
    coord.node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    coord.unicast_port = coord_port;

    LOG_INFO("SENDING JOIN_CONFIRM_U: req=%u from=%s to=%s:%u", request_id, from_name, coord.node_name, coord_port);
    return unicast_send(unicast_socket, &msg, &coord);
}

static int handle_join_accept_unicast(ring_state_t* state, const join_accept_t* accept, int unicast_socket) {
    const char* me = state->config.current->node_name;
    int did_apply = 0;

    if (strncmp(me, accept->new_name, MAX_NODE_NAME_SIZE) == 0) {
        if (!state->joined && accept->header.request_id != state->join_request_id) {
            return 0;
        }
        strncpy(state->config.prev->node_name, accept->prev_name, MAX_NODE_NAME_SIZE - 1);
        state->config.prev->node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
        state->config.prev->unicast_port = accept->prev_unicast_port;

        strncpy(state->config.next->node_name, accept->before_name, MAX_NODE_NAME_SIZE - 1);
        state->config.next->node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
        state->config.next->unicast_port = accept->before_unicast_port;

        state->joined = 1;
        state->join_request_last_sent = 0;
        state->join_request_retries = 0;
        did_apply = 1;
    } else if (strncmp(me, accept->prev_name, MAX_NODE_NAME_SIZE) == 0) {
        if (strncmp(state->config.next->node_name, accept->before_name, MAX_NODE_NAME_SIZE) != 0 &&
            strncmp(state->config.next->node_name, accept->new_name, MAX_NODE_NAME_SIZE) != 0) {
            return 0;
        }
        strncpy(state->config.next->node_name, accept->new_name, MAX_NODE_NAME_SIZE - 1);
        state->config.next->node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
        state->config.next->unicast_port = accept->new_unicast_port;
        state->joined = 1;
        did_apply = 1;
    }

    if (!did_apply) {
        return 0;
    }

    LOG_INFO(
        "APPLY JOIN_ACCEPT_U: req=%u new=%s before=%s prev=%s", accept->header.request_id, accept->new_name,
        accept->before_name, accept->prev_name
    );

    if (send_join_confirm(
            unicast_socket, accept->before_name, accept->before_unicast_port, accept->header.request_id, me
        ) < 0) {
        LOG_WARN("Failed to send JOIN_CONFIRM_U, will retry on next accept");
        return 0;
    }
    return 0;
}

static void handle_join_confirm_unicast(ring_state_t* state, const join_confirm_t* confirm) {
    if (!state->join_inflight.active) {
        return;
    }

    if (confirm->request_id != state->join_inflight.request_id) {
        return;
    }

    if (strncmp(confirm->from_name, state->join_inflight.expected_prev_name, MAX_NODE_NAME_SIZE) == 0) {
        state->join_inflight.got_confirm_prev = 1;
    }

    if (strncmp(confirm->from_name, state->join_inflight.joiner.node_name, MAX_NODE_NAME_SIZE) == 0) {
        state->join_inflight.got_confirm_joiner = 1;
    }

    LOG_INFO(
        "RECV JOIN_CONFIRM_U: req=%u from=%s got_prev=%d got_joiner=%d", confirm->request_id, confirm->from_name,
        state->join_inflight.got_confirm_prev, state->join_inflight.got_confirm_joiner
    );

    if (state->join_inflight.got_confirm_prev && state->join_inflight.got_confirm_joiner) {
        strncpy(state->config.prev->node_name, state->join_inflight.joiner.node_name, MAX_NODE_NAME_SIZE - 1);
        state->config.prev->node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
        state->config.prev->unicast_port = state->join_inflight.joiner.unicast_port;

        state->last_seen_topo_version++;
        state->token_in.topo_version = state->last_seen_topo_version;
        state->join_state = (join_state_t){0};

        state->join_inflight.active = 0;
        state->join_inflight.got_confirm_prev = 0;
        state->join_inflight.got_confirm_joiner = 0;
        state->join_inflight.retries = 0;

        LOG_INFO(
            "JOIN_INFLIGHT COMPLETE: new prev=%s topo=%u", state->config.prev->node_name, state->last_seen_topo_version
        );
    }
}

static int join_inflight_tick(ring_state_t* state, int unicast_socket) {
    LOG_INFO(
        "JOIN_ACCEPT_U TICK: active=%d req=%u retries=%d", state->join_inflight.active, state->join_inflight.request_id,
        state->join_inflight.retries
    );

    if (!state->join_inflight.active) {
        return 0;
    }

    if (state->join_inflight.got_confirm_prev && state->join_inflight.got_confirm_joiner) {
        return 0;
    }

    time_t now = time(NULL);
    if (now - state->join_inflight.last_sent < JOIN_ACCEPT_TIMEOUT_S && state->join_inflight.last_sent != 0) {
        return 0;
    }

    if (state->join_inflight.retries >= JOIN_ACCEPT_RETRIES) {
        LOG_ERROR("Join accept retries exhausted for request id %u", state->join_inflight.request_id);
        state->join_inflight.active = 0;
        return 0;
    }

    join_accept_t accept = {0};
    accept.header.request_id = state->join_inflight.request_id;
    strncpy(accept.new_name, state->join_inflight.joiner.node_name, MAX_NODE_NAME_SIZE - 1);
    accept.new_unicast_port = state->join_inflight.joiner.unicast_port;

    strncpy(accept.before_name, state->config.current->node_name, MAX_NODE_NAME_SIZE - 1);
    accept.before_unicast_port = state->config.current->unicast_port;

    strncpy(accept.prev_name, state->config.prev->node_name, MAX_NODE_NAME_SIZE - 1);
    accept.prev_unicast_port = state->config.prev->unicast_port;
    accept.new_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    accept.before_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    accept.prev_name[MAX_NODE_NAME_SIZE - 1] = '\0';

    join_accept_t accept_wire = {0};
    accept_wire.header.magic = htonl(JOIN_MAGIC);
    accept_wire.header.type = htons(JOIN_ACCEPT);
    accept_wire.header.request_id = htonl(accept.header.request_id);
    strncpy(accept_wire.new_name, accept.new_name, MAX_NODE_NAME_SIZE - 1);
    strncpy(accept_wire.before_name, accept.before_name, MAX_NODE_NAME_SIZE - 1);
    strncpy(accept_wire.prev_name, accept.prev_name, MAX_NODE_NAME_SIZE - 1);
    accept_wire.new_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    accept_wire.before_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    accept_wire.prev_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    accept_wire.new_unicast_port = htons(accept.new_unicast_port);
    accept_wire.before_unicast_port = htons(accept.before_unicast_port);
    accept_wire.prev_unicast_port = htons(accept.prev_unicast_port);

    unicast_msg_t msg = {.type = UMSG_JOIN_ACCEPT, .payload_len = sizeof(accept_wire)};
    memcpy(msg.payload, &accept_wire, sizeof(accept_wire));

    LOG_INFO("SENDING JOIN_ACCEPT_U: new=%s before=%s prev=%s", accept.new_name, accept.before_name, accept.prev_name);

    if (!state->join_inflight.got_confirm_joiner) {
        route_t joiner_route = {0};
        strncpy(joiner_route.node_name, state->join_inflight.joiner.node_name, MAX_NODE_NAME_SIZE - 1);
        joiner_route.node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
        joiner_route.unicast_port = state->join_inflight.joiner.unicast_port;
        if (unicast_send(unicast_socket, &msg, &joiner_route) < 0) {
            return -1;
        }
    }

    if (!state->join_inflight.got_confirm_prev) {
        if (unicast_send(unicast_socket, &msg, state->config.prev) < 0) {
            return -1;
        }
    }

    state->join_inflight.last_sent = now;
    state->join_inflight.retries++;
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
