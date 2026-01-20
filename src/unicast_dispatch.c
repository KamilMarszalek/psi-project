#define _GNU_SOURCE
#include "unicast_dispatch.h"

#include "join_fsm.h"
#include "logger.h"
#include "token.h"

#include <arpa/inet.h>
#include <string.h>

static void handle_token_unicast(ring_state_t* state, const unicast_msg_t* msg) {
    if (msg->payload_len != sizeof(token_t)) {
        LOG_ERROR("Invalid token payload length: %u", msg->payload_len);
        return;
    }

    memcpy(&state->token_in, msg->payload, sizeof(token_t));
    if (state->joined) {
        if (state->token_in.topo_version > state->last_seen_topo_version) {
            state->last_seen_topo_version = state->token_in.topo_version;
            state->join_state = (join_state_t){0};
            LOG_INFO("Topo version advanced to %u, cleared pending joins", state->last_seen_topo_version);
        }
        state->have_token = 1;
        LOG_INFO("Received token via unicast\n");
    }
}

static int handle_join_accept_message(ring_state_t* state, const unicast_msg_t* msg, int unicast_socket) {
    if (msg->payload_len != sizeof(join_accept_t)) {
        LOG_ERROR("Invalid JOIN_ACCEPT payload length: %u", msg->payload_len);
        return 0;
    }

    join_accept_t accept_wire;
    memcpy(&accept_wire, msg->payload, sizeof(accept_wire));
    if (ntohl(accept_wire.header.magic) != JOIN_MAGIC || ntohs(accept_wire.header.type) != JOIN_ACCEPT) {
        LOG_ERROR("Invalid JOIN_ACCEPT magic");
        return 0;
    }

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

    return join_fsm_handle_accept_unicast(state, &accept, unicast_socket);
}

static void handle_join_confirm_message(ring_state_t* state, const unicast_msg_t* msg) {
    if (msg->payload_len != sizeof(join_confirm_t)) {
        LOG_ERROR("Invalid JOIN_CONFIRM payload length: %u", msg->payload_len);
        return;
    }

    join_confirm_t confirm_wire;
    memcpy(&confirm_wire, msg->payload, sizeof(confirm_wire));
    join_confirm_t confirm = {0};
    confirm.request_id = ntohl(confirm_wire.request_id);
    strncpy(confirm.from_name, confirm_wire.from_name, MAX_NODE_NAME_SIZE - 1);
    confirm.from_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    join_fsm_handle_confirm_unicast(state, &confirm);
}

int unicast_dispatch_message(ring_state_t* state, const unicast_msg_t* msg, int unicast_socket) {
    if (msg->type == UMSG_TOKEN) {
        handle_token_unicast(state, msg);
        return 0;
    }

    if (msg->type == UMSG_JOIN_ACCEPT) {
        return handle_join_accept_message(state, msg, unicast_socket);
    }

    if (msg->type == UMSG_JOIN_CONFIRM) {
        handle_join_confirm_message(state, msg);
        return 0;
    }

    LOG_WARN("Unknown unicast message type: %u", msg->type);
    return 0;
}
