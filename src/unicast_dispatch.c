#define _GNU_SOURCE
#include "unicast_dispatch.h"

#include "join.h"
#include "join_fsm.h"
#include "logger.h"
#include "token.h"
#include "unicast.h"

#include <arpa/inet.h>
#include <string.h>

static void update_topology_on_token(ring_state_t* state);
static void handle_token_unicast(ring_state_t* state, const unicast_msg_t* msg);

static int handle_join_ack_u(ring_state_t* st, const unicast_msg_t* msg, int unicast_socket);
static void handle_join_ack_ack_u(ring_state_t* st, const unicast_msg_t* msg);

static void update_topology_on_token(ring_state_t* state) {
    if (state->token_in.topo_version <= state->last_seen_topo_version) {
        return;
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
}

static void handle_token_unicast(ring_state_t* state, const unicast_msg_t* msg) {
    if (msg->payload_len != sizeof(token_t)) {
        LOG_ERROR("Invalid token payload length: %u", msg->payload_len);
        return;
    }

    memcpy(&state->token_in, msg->payload, sizeof(token_t));
    if (state->joined) {
        update_topology_on_token(state);
        state->have_token = 1;
        LOG_INFO("Received token via unicast");
    }
}

int unicast_dispatch_message(ring_state_t* state, const unicast_msg_t* msg, int unicast_socket) {
    if (msg->type == UMSG_TOKEN) {
        handle_token_unicast(state, msg);
        return 0;
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
        LOG_INFO(
            "RECV JOIN_ACK_U req=%u from=%s prev=%d joiner=%d", req, wire.from_name, matched_prev,
            matched_joiner
        );
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
        (void) unicast_send(unicast_socket, &out, &dst);
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
