#define _GNU_SOURCE
#include "unicast_dispatch.h"

#include "logger.h"
#include "token.h"

#include <string.h>

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
    (void) unicast_socket;

    if (msg->type == UMSG_TOKEN) {
        handle_token_unicast(state, msg);
        return 0;
    }

    LOG_WARN("Unknown unicast message type: %u", msg->type);
    return 0;
}
