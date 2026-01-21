#define _GNU_SOURCE
#include "unicast_dispatch.h"

#include "join.h"
#include "join_fsm.h"
#include "logger.h"
#include "token.h"
#include "unicast.h"

#include <arpa/inet.h>
#include <string.h>
#include <time.h>

static int update_topology_on_token(ring_state_t* state);
static void clear_pending_joins_on_token(ring_state_t* state);
static void handle_token_unicast(ring_state_t* state, const unicast_msg_t* msg);

static int handle_join_accept_u(ring_state_t* st, const unicast_msg_t* msg, int unicast_socket);
static int handle_join_ack_u(ring_state_t* st, const unicast_msg_t* msg, int unicast_socket);
static void handle_join_ack_ack_u(ring_state_t* st, const unicast_msg_t* msg);

static int apply_accept_if_relevant(ring_state_t* ring_state, const join_accept_t* accept);

static int update_topology_on_token(ring_state_t* state) {
    if (state->token_in.topo_version <= state->last_seen_topo_version) {
        return 0;
    }

    uint32_t old_topo = state->last_seen_topo_version;
    uint32_t new_topo = state->token_in.topo_version;
    uint32_t delta = new_topo - old_topo;

    size_t pending_before = state->join_state.count;
    size_t removed = drop_oldest_pending_joins(&state->join_state, pending_before);

    state->last_seen_topo_version = new_topo;
    LOG_INFO(
        "Topo version advanced to %u (delta=%u), cleared pending joins: removed=%zu", state->last_seen_topo_version,
        delta, removed
    );
    return 1;
}

static void clear_pending_joins_on_token(ring_state_t* state) {
    size_t pending_before = state->join_state.count;
    if (pending_before == 0) {
        return;
    }

    size_t removed = drop_oldest_pending_joins(&state->join_state, pending_before);
    LOG_INFO("Cleared pending joins on token receive: removed=%zu", removed);
}

static void handle_token_unicast(ring_state_t* state, const unicast_msg_t* msg) {
    if (msg->payload_len != sizeof(token_t)) {
        LOG_ERROR("Invalid token payload length: %u", msg->payload_len);
        return;
    }

    memcpy(&state->token_in, msg->payload, sizeof(token_t));
    if (state->joined) {
        if (update_topology_on_token(state)) {
            clear_pending_joins_on_token(state);
        }
        state->have_token = 1;
        LOG_INFO("Received token via unicast");
    }
}

int unicast_dispatch_message(ring_state_t* state, const unicast_msg_t* msg, int unicast_socket) {
    if (msg->type == UMSG_TOKEN) {
        handle_token_unicast(state, msg);
        return 0;
    }

    if (msg->type == UMSG_JOIN_ACCEPT_U) {
        return handle_join_accept_u(state, msg, unicast_socket);
    }

    if (msg->type == UMSG_JOIN_ACK_U) {
        return handle_join_ack_u(state, msg, unicast_socket);
    }

    if (msg->type == UMSG_JOIN_ACK_ACK_U) {
        handle_join_ack_ack_u(state, msg);
        return 0;
    }

    LOG_WARN("Unknown unicast message type: %u", msg->type);
    return 0;
}

static int handle_join_accept_u(ring_state_t* st, const unicast_msg_t* msg, int unicast_socket) {
    if (msg->payload_len != sizeof(join_accept_t)) {
        return 0;
    }

    join_accept_t wire;
    memcpy(&wire, msg->payload, sizeof(wire));
    if (ntohl(wire.header.magic) != JOIN_MAGIC || ntohs(wire.header.type) != JOIN_ACCEPT) {
        return 0;
    }

    join_accept_t accept = {0};
    accept.header.request_id = ntohl(wire.header.request_id);
    accept.header.type = JOIN_ACCEPT;
    accept.header.magic = JOIN_MAGIC;

    strncpy(accept.new_name, wire.new_name, MAX_NODE_NAME_SIZE - 1);
    strncpy(accept.before_name, wire.before_name, MAX_NODE_NAME_SIZE - 1);
    strncpy(accept.prev_name, wire.prev_name, MAX_NODE_NAME_SIZE - 1);
    accept.new_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    accept.before_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    accept.prev_name[MAX_NODE_NAME_SIZE - 1] = '\0';

    accept.new_unicast_port = ntohs(wire.new_unicast_port);
    accept.before_unicast_port = ntohs(wire.before_unicast_port);
    accept.prev_unicast_port = ntohs(wire.prev_unicast_port);

    join_state_mark_completed_and_prune_pending(&st->join_state, accept.new_name, accept.header.request_id);

    LOG_INFO(
        "RECV JOIN_ACCEPT_U: req=%u new=%s before=%s prev=%s", accept.header.request_id, accept.new_name,
        accept.before_name, accept.prev_name
    );

    int did_apply = apply_accept_if_relevant(st, &accept);
    LOG_INFO("APPLY JOIN_ACCEPT_U: did_apply=%d", did_apply);

    if (did_apply) {
        st->ack_sender.active = 1;
        st->ack_sender.request_id = accept.header.request_id;

        strncpy(st->ack_sender.coord_name, accept.before_name, MAX_NODE_NAME_SIZE - 1);
        st->ack_sender.coord_name[MAX_NODE_NAME_SIZE - 1] = '\0';
        st->ack_sender.coord_unicast_port = accept.before_unicast_port;

        st->ack_sender.last_sent = 0;
        st->ack_sender.retries = 0;
        st->ack_sender.got_ack_ack = 0;

        join_ack_u_t ack = {0};
        ack.request_id = htonl(accept.header.request_id);
        strncpy(ack.from_name, st->config.current->node_name, MAX_NODE_NAME_SIZE - 1);
        ack.from_name[MAX_NODE_NAME_SIZE - 1] = '\0';

        unicast_msg_t ack_msg = {0};
        ack_msg.type = UMSG_JOIN_ACK_U;
        ack_msg.payload_len = sizeof(ack);
        memcpy(ack_msg.payload, &ack, sizeof(ack));

        route_t coord = {0};
        strncpy(coord.node_name, accept.before_name, MAX_NODE_NAME_SIZE - 1);
        coord.node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
        coord.unicast_port = accept.before_unicast_port;

        LOG_INFO("SEND JOIN_ACK_U req=%u to=%s:%u", accept.header.request_id, coord.node_name, coord.unicast_port);
        if (unicast_send_limited(
                unicast_socket, &ack_msg, &coord, JOIN_ACCEPT_ACK_TIMEOUT_USEC, JOIN_ACCEPT_ACK_MAX_ATTEMPTS
            ) == 0) {
            st->ack_sender.last_sent = time(NULL);
            st->ack_sender.retries = 1;
        }
    }

    return 0;
}

static int handle_join_ack_u(ring_state_t* st, const unicast_msg_t* msg, int unicast_socket) {
    if (msg->payload_len != sizeof(join_ack_u_t)) {
        return 0;
    }

    join_ack_u_t wire;
    memcpy(&wire, msg->payload, sizeof(wire));

    uint32_t req = ntohl(wire.request_id);
    wire.from_name[MAX_NODE_NAME_SIZE - 1] = '\0';

    if (st->join_inflight.active && st->join_inflight.request_id == req) {
        int matched_prev = 0;
        int matched_joiner = 0;

        if (strncmp(wire.from_name, st->join_inflight.expected_prev_name, MAX_NODE_NAME_SIZE) == 0 ||
            strncmp(wire.from_name, st->config.prev->node_name, MAX_NODE_NAME_SIZE) == 0) {
            st->join_inflight.got_confirm_prev = 1;
            matched_prev = 1;
        }
        if (strncmp(wire.from_name, st->join_inflight.joiner.node_name, MAX_NODE_NAME_SIZE) == 0) {
            st->join_inflight.got_confirm_joiner = 1;
            matched_joiner = 1;
        }
        LOG_INFO("RECV JOIN_ACK_U req=%u from=%s prev=%d joiner=%d", req, wire.from_name, matched_prev, matched_joiner);
        join_fsm_maybe_complete(st, st->broadcast_socket);
    }

    join_ack_ack_u_t ackack = {0};
    ackack.request_id = htonl(req);
    strncpy(ackack.to_name, wire.from_name, MAX_NODE_NAME_SIZE - 1);
    ackack.to_name[MAX_NODE_NAME_SIZE - 1] = '\0';

    unicast_msg_t out = {0};
    out.type = UMSG_JOIN_ACK_ACK_U;
    out.payload_len = sizeof(ackack);
    memcpy(out.payload, &ackack, sizeof(ackack));

    route_t dst = {0};
    strncpy(dst.node_name, wire.from_name, MAX_NODE_NAME_SIZE - 1);
    dst.node_name[MAX_NODE_NAME_SIZE - 1] = '\0';

    if (st->join_inflight.active &&
        strncmp(wire.from_name, st->join_inflight.joiner.node_name, MAX_NODE_NAME_SIZE) == 0) {
        dst.unicast_port = st->join_inflight.joiner.unicast_port;
    } else if (strncmp(wire.from_name, st->config.prev->node_name, MAX_NODE_NAME_SIZE) == 0) {
        dst.unicast_port = st->config.prev->unicast_port;
    } else {
        return 0;
    }

    for (int i = 0; i < JOIN_ACK_ACK_SEND_COUNT; i++) {
        (void) unicast_send_limited(unicast_socket, &out, &dst, JOIN_ACCEPT_ACK_TIMEOUT_USEC, 1);
    }

    return 0;
}

static void handle_join_ack_ack_u(ring_state_t* st, const unicast_msg_t* msg) {
    if (msg->payload_len != sizeof(join_ack_ack_u_t)) {
        return;
    }

    join_ack_ack_u_t wire;
    memcpy(&wire, msg->payload, sizeof(wire));

    uint32_t req = ntohl(wire.request_id);
    wire.to_name[MAX_NODE_NAME_SIZE - 1] = '\0';

    if (strncmp(wire.to_name, st->config.current->node_name, MAX_NODE_NAME_SIZE) != 0) {
        return;
    }

    if (st->ack_sender.active && st->ack_sender.request_id == req) {
        st->ack_sender.got_ack_ack = 1;
        LOG_INFO("RECV JOIN_ACK_ACK_U req=%u => stop ACK retries", req);
    }
}

static int apply_accept_if_relevant(ring_state_t* ring_state, const join_accept_t* accept) {
    const char* me = ring_state->config.current->node_name;

    if (strncmp(me, accept->new_name, MAX_NODE_NAME_SIZE) == 0) {
        if (ring_state->joined) {
            return 0;
        }
        if (!ring_state->joined && accept->header.request_id != ring_state->join_request_id) {
            return 0;
        }
        strncpy(ring_state->config.prev->node_name, accept->prev_name, MAX_NODE_NAME_SIZE - 1);
        ring_state->config.prev->node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
        ring_state->config.prev->unicast_port = accept->prev_unicast_port;

        strncpy(ring_state->config.next->node_name, accept->before_name, MAX_NODE_NAME_SIZE - 1);
        ring_state->config.next->node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
        ring_state->config.next->unicast_port = accept->before_unicast_port;
        ring_state->joined = 1;
        ring_state->join_request_last_sent = 0;
        ring_state->join_request_retries = 0;
        return 1;
    }

    if (strncmp(me, accept->prev_name, MAX_NODE_NAME_SIZE) == 0) {
        if (strncmp(ring_state->config.next->node_name, accept->before_name, MAX_NODE_NAME_SIZE) != 0) {
            return 0;
        }
        strncpy(ring_state->config.next->node_name, accept->new_name, MAX_NODE_NAME_SIZE - 1);
        ring_state->config.next->node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
        ring_state->config.next->unicast_port = accept->new_unicast_port;
        ring_state->joined = 1;
        return 1;
    }

    return 0;
}
