#ifndef JOIN_H
#define JOIN_H
#include "consts.h"
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

typedef struct {
    uint32_t magic;//0xAAAABBBB
    uint32_t request_id;
    char node_name[MAX_NODE_NAME_SIZE];
    uint16_t unicast_port;
} join_request_t;

typedef struct {
    int used;
    uint32_t request_id;
    char node_name[MAX_NODE_NAME_SIZE];
    uint16_t unicast_port;
    struct in_addr ip;
    time_t last_seen;
} pending_join_t;

typedef struct {
    pending_join_t joins[MAX_PENDING_JOINS];
    size_t count;
} join_state_t;

int add_pending_join(join_state_t* state, const join_request_t* request, struct in_addr ip);
int remove_pending_join(join_state_t* state, const char* node_name);
int pop_oldest_pending_join(join_state_t* state, pending_join_t* out);
void prune_joins(join_state_t* state, time_t now, int ttl_seconds);
#endif