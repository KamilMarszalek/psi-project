#define _GNU_SOURCE
#include "join_fsm.h"

#include "broadcast.h"
#include "consts.h"
#include "join.h"
#include "logger.h"
#include "unicast.h"
#include "unicast_msg.h"

#include <string.h>
#include <time.h>

static void build_join_accept_host(const ring_state_t* state, join_accept_t* accept) {
    *accept = (join_accept_t){0};

    accept->header.request_id = state->join_inflight.request_id;
    accept->header.type = JOIN_ACCEPT;
    accept->header.magic = JOIN_MAGIC;

    strncpy(accept->new_name, state->join_inflight.joiner.node_name, MAX_NODE_NAME_SIZE - 1);
    accept->new_unicast_port = state->join_inflight.joiner.unicast_port;

    strncpy(accept->before_name, state->config.current->node_name, MAX_NODE_NAME_SIZE - 1);
    accept->before_unicast_port = state->config.current->unicast_port;

    strncpy(accept->prev_name, state->config.prev->node_name, MAX_NODE_NAME_SIZE - 1);
    accept->prev_unicast_port = state->config.prev->unicast_port;

    accept->new_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    accept->before_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    accept->prev_name[MAX_NODE_NAME_SIZE - 1] = '\0';
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
        LOG_ERROR("JOIN_REQUEST retries exhausted (id=%u)", state->join_request_id);
        return -1;
    }

    if (broadcast_send_join_request(
            broadcast_socket, state->join_request_id, state->config.current->node_name,
            state->config.current->unicast_port
        ) < 0) {
        LOG_ERROR("Failed to send JOIN_REQUEST");
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

static void join_fsm_try_complete(ring_state_t* state, int broadcast_socket) {
    if (!state->join_inflight.active) {
        return;
    }
    if (!(state->join_inflight.got_confirm_prev && state->join_inflight.got_confirm_joiner)) {
        return;
    }

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

    broadcast_send_join_commit(
        broadcast_socket, state->join_inflight.request_id, state->join_inflight.joiner.node_name,
        state->last_seen_topo_version
    );


    LOG_INFO(
        "JOIN_INFLIGHT COMPLETE: new prev=%s topo=%u", state->config.prev->node_name, state->last_seen_topo_version
    );
}

void join_fsm_maybe_complete(ring_state_t* state, int broadcast_socket) {
    join_fsm_try_complete(state, broadcast_socket);
}

int join_fsm_inflight_tick(ring_state_t* state, int unicast_socket, int broadcast_socket) {
    if (!state->join_inflight.active) {
        return 0;
    }

    if (state->join_inflight.got_confirm_prev && state->join_inflight.got_confirm_joiner) {
        join_fsm_maybe_complete(state, broadcast_socket);
        return 0;
    }

    time_t now = time(NULL);
    if (state->join_inflight.last_sent != 0 && now - state->join_inflight.last_sent < JOIN_ACCEPT_TIMEOUT_S) {
        return 0;
    }

    if (state->join_inflight.retries >= JOIN_ACCEPT_RETRIES) {
        LOG_WARN(
            "Join accept retries exhausted for req=%u, keeping token and retrying", state->join_inflight.request_id
        );
        state->join_inflight.retries = 0;
        state->join_inflight.last_sent = 0;
    }

    join_accept_t accept;
    build_join_accept_host(state, &accept);

    join_accept_t wire = {0};
    wire.header.magic = htonl(JOIN_MAGIC);
    wire.header.type = htons(JOIN_ACCEPT);
    wire.header.request_id = htonl(accept.header.request_id);
    strncpy(wire.new_name, accept.new_name, MAX_NODE_NAME_SIZE - 1);
    strncpy(wire.before_name, accept.before_name, MAX_NODE_NAME_SIZE - 1);
    strncpy(wire.prev_name, accept.prev_name, MAX_NODE_NAME_SIZE - 1);
    wire.new_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    wire.before_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    wire.prev_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    wire.new_unicast_port = htons(accept.new_unicast_port);
    wire.before_unicast_port = htons(accept.before_unicast_port);
    wire.prev_unicast_port = htons(accept.prev_unicast_port);

    unicast_msg_t msg = {0};
    msg.type = UMSG_JOIN_ACCEPT_U;
    msg.payload_len = sizeof(wire);
    memcpy(msg.payload, &wire, sizeof(wire));

    LOG_INFO(
        "SENDING JOIN_ACCEPT_U: new=%s before=%s prev=%s", accept.new_name, accept.before_name, accept.prev_name
    );

    int send_error = 0;
    if (!state->join_inflight.got_confirm_joiner) {
        route_t joiner = {0};
        strncpy(joiner.node_name, accept.new_name, MAX_NODE_NAME_SIZE - 1);
        joiner.node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
        joiner.unicast_port = accept.new_unicast_port;
        if (unicast_send(unicast_socket, &msg, &joiner) < 0) {
            LOG_WARN("JOIN_ACCEPT_U to joiner failed, will retry");
            send_error = 1;
        }
    }

    if (!state->join_inflight.got_confirm_prev) {
        if (unicast_send(unicast_socket, &msg, state->config.prev) < 0) {
            LOG_WARN("JOIN_ACCEPT_U to prev failed, will retry");
            send_error = 1;
        }
    }

    state->join_inflight.last_sent = now;
    state->join_inflight.retries++;
    if (send_error) {
        return 0;
    }
    return 0;
}


void join_fsm_handle_ack_broadcast(ring_state_t* state, const join_ack_t* ack, int broadcast_socket) {
    if (!state->join_inflight.active) {
        return;
    }
    if (ack->header.request_id != state->join_inflight.request_id) {
        return;
    }

    if (strncmp(ack->from_name, state->join_inflight.expected_prev_name, MAX_NODE_NAME_SIZE) == 0) {
        state->join_inflight.got_confirm_prev = 1;
    }
    if (strncmp(ack->from_name, state->join_inflight.joiner.node_name, MAX_NODE_NAME_SIZE) == 0) {
        state->join_inflight.got_confirm_joiner = 1;
    }

    LOG_INFO(
        "RECV JOIN_ACK (broadcast): req=%u from=%s got_prev=%d got_joiner=%d", ack->header.request_id, ack->from_name,
        state->join_inflight.got_confirm_prev, state->join_inflight.got_confirm_joiner
    );

    join_fsm_maybe_complete(state, broadcast_socket);
}
