#include "join.h"
#include <arpa/inet.h>
#include <string.h>
#include <time.h>

int add_pending_join(join_state_t* state, const join_request_t* request, struct in_addr ip) {
    if (state->count >= MAX_PENDING_JOINS) {
        return -1;
    }

    for (size_t i = 0; i < MAX_PENDING_JOINS; i++) {
        if (state->joins[i].used && strncmp(state->joins[i].node_name, request->node_name, MAX_NODE_NAME_SIZE) == 0) {
            state->joins[i].request_id = request->header.request_id;
            state->joins[i].last_seen = time(NULL);
            state->joins[i].ip = ip;
            state->joins[i].unicast_port = request->unicast_port;
            return 0;
        }
        if (!state->joins[i].used) {
            state->joins[i].used = 1;
            state->joins[i].request_id = request->header.request_id;
            strncpy(state->joins[i].node_name, request->node_name, MAX_NODE_NAME_SIZE - 1);
            state->joins[i].node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
            state->joins[i].unicast_port = request->unicast_port;
            state->joins[i].ip = ip;
            state->joins[i].last_seen = time(NULL);
            state->count++;
            return 0;
        }
    }

    return -1;
}

int remove_pending_join(join_state_t* state, const char* node_name) {
    for (size_t i = 0; i < MAX_PENDING_JOINS; i++) {
        if (state->joins[i].used && strncmp(state->joins[i].node_name, node_name, MAX_NODE_NAME_SIZE) == 0) {
            state->joins[i].used = 0;
            state->count--;
            return 0;
        }
    }
    return -1;
}

int pop_oldest_pending_join(join_state_t* state, pending_join_t* out) {
    int oldest_index = -1;
    time_t oldest_time = 0;

    for (size_t i = 0; i < MAX_PENDING_JOINS; i++) {
        if (!state->joins[i].used)
            continue;
        if (oldest_index == -1 || state->joins[i].last_seen < oldest_time) {
            oldest_time = state->joins[i].last_seen;
            oldest_index = (int) i;
        }
    }


    if (oldest_index != -1) {
        *out = state->joins[oldest_index];
        state->joins[oldest_index].used = 0;
        state->count--;
        return 0;
    }

    return -1;
}

size_t drop_oldest_pending_joins(join_state_t* state, size_t max_remove) {
    size_t removed = 0;
    pending_join_t ignored;

    while (removed < max_remove) {
        if (pop_oldest_pending_join(state, &ignored) != 0) {
            break;
        }
        removed++;
    }

    return removed;
}

int join_state_is_completed(const join_state_t* state, const join_request_t* request) {
    for (size_t i = 0; i < state->completed_count; i++) {
        if (strncmp(state->completed[i].node_name, request->node_name, MAX_NODE_NAME_SIZE) == 0) {
            return 1;
        }
    }
    return 0;
}

void join_state_record_completed(join_state_t* state, const pending_join_t* joiner) {
    for (size_t i = 0; i < state->completed_count; i++) {
        if (strncmp(state->completed[i].node_name, joiner->node_name, MAX_NODE_NAME_SIZE) == 0) {
            state->completed[i].request_id = joiner->request_id;
            return;
        }
    }

    size_t idx = state->completed_count;
    if (idx >= MAX_PENDING_JOINS) {
        for (size_t i = 1; i < state->completed_count; i++) {
            state->completed[i - 1] = state->completed[i];
        }
        idx = state->completed_count - 1;
    } else {
        state->completed_count++;
    }

    state->completed[idx].request_id = joiner->request_id;
    strncpy(state->completed[idx].node_name, joiner->node_name, MAX_NODE_NAME_SIZE - 1);
    state->completed[idx].node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
}

void prune_joins(join_state_t* state, time_t now, int ttl_seconds) {
    for (size_t i = 0; i < MAX_PENDING_JOINS; i++) {
        if (state->joins[i].used && (now - state->joins[i].last_seen) > ttl_seconds) {
            state->joins[i].used = 0;
            state->count--;
        }
    }
}

void join_state_mark_completed_and_prune_pending(join_state_t* state, const char* node_name, uint32_t request_id) {
    pending_join_t tmp = {0};
    tmp.request_id = request_id;
    strncpy(tmp.node_name, node_name, MAX_NODE_NAME_SIZE - 1);
    tmp.node_name[MAX_NODE_NAME_SIZE - 1] = '\0';
    tmp.unicast_port = 0;

    join_state_record_completed(state, &tmp);

    remove_pending_join(state, node_name);
}
