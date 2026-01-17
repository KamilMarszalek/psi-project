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
            state->joins[i].request_id = request->request_id;
            state->joins[i].last_seen = time(NULL);
            state->joins[i].ip = ip;
            state->joins[i].unicast_port = request->unicast_port;
            return 0;
        }
        if (!state->joins[i].used) {
            state->joins[i].used = 1;
            state->joins[i].request_id = request->request_id;
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

void prune_joins(join_state_t* state, time_t now, int ttl_seconds) {
    for (size_t i = 0; i < MAX_PENDING_JOINS; i++) {
        if (state->joins[i].used && (now - state->joins[i].last_seen) > ttl_seconds) {
            state->joins[i].used = 0;
            state->count--;
        }
    }
}