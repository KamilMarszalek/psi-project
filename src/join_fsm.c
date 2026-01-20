#define _GNU_SOURCE
#include "join_fsm.h"

#include "broadcast.h"
#include "consts.h"
#include "join.h"
#include "logger.h"
#include "route.h"
#include "unicast.h"
#include "unicast_msg.h"

#include <arpa/inet.h>
#include <string.h>
#include <time.h>

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

int join_fsm_request_tick(ring_state_t* state, int broadcast_socket) {
    if (state->joined) {
        return 0;
    }

    time_t now = time(NULL);
    if (state->join_request_last_sent != 0 && now - state->join_request_last_sent < JOIN_REQUEST_TIMEOUT_S) {
        return 0;
    }

    if (state->join_request_retries >= JOIN_REQUEST_RETRIES) {
        LOG_ERROR("JOIN_REQUEST retries exhausted (id=%u)\n", state->join_request_id);
        return -1;
    }

    if (broadcast_send_join_request(
            broadcast_socket, state->join_request_id, state->config.current->node_name,
            state->config.current->unicast_port
        ) < 0) {
        LOG_ERROR("Failed to send JOIN_REQUEST\n");
        return -1;
    }

    state->join_request_last_sent = now;
    state->join_request_retries++;
    return 0;
}

void join_fsm_start_inflight(ring_state_t* state, const pending_join_t* pending) {
    state->join_inflight.active = 1;
    state->join_inflight.request_id = pending->request_id;
    state->join_inflight.joiner = *pending;

    strncpy(state->join_inflight.expected_prev_name, state->config.prev->node_name, MAX_NODE_NAME_SIZE - 1);
    state->join_inflight.expected_prev_name[MAX_NODE_NAME_SIZE - 1] = '\0';

    state->join_inflight.got_confirm_prev = 0;
    state->join_inflight.got_confirm_joiner = 0;

    state->join_inflight.last_sent = 0;
    state->join_inflight.retries = 0;
}

int join_fsm_inflight_tick(ring_state_t* state, int unicast_socket) {
    LOG_DEBUG(
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
        LOG_WARN(
            "Join accept retries exhausted for request id %u, keeping token and retrying",
            state->join_inflight.request_id
        );
        state->join_inflight.retries = 0;
        state->join_inflight.last_sent = 0;
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

int join_fsm_handle_accept_unicast(ring_state_t* state, const join_accept_t* accept, int unicast_socket) {
    const char* me = state->config.current->node_name;
    int did_apply = 0;

    if (strncmp(me, accept->new_name, MAX_NODE_NAME_SIZE) == 0) {
        if (!state->joined && accept->header.request_id != state->join_request_id) {
            return 0;
        }
        if (state->joined) {
            if (accept->header.request_id != state->join_request_id) {
                return 0;
            }
            if (send_join_confirm(
                    unicast_socket, accept->before_name, accept->before_unicast_port, accept->header.request_id, me
                ) < 0) {
                LOG_WARN("Failed to send JOIN_CONFIRM_U for duplicate JOIN_ACCEPT_U");
            }
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

void join_fsm_handle_confirm_unicast(ring_state_t* state, const join_confirm_t* confirm) {
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

        remove_pending_join(&state->join_state, state->join_inflight.joiner.node_name);
        join_state_record_completed(&state->join_state, &state->join_inflight.joiner);

        state->join_inflight.active = 0;
        state->join_inflight.got_confirm_prev = 0;
        state->join_inflight.got_confirm_joiner = 0;
        state->join_inflight.retries = 0;

        LOG_INFO(
            "JOIN_INFLIGHT COMPLETE: new prev=%s topo=%u", state->config.prev->node_name, state->last_seen_topo_version
        );
    }
}
