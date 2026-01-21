#ifndef JOIN_H
#define JOIN_H
#include "consts.h"
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

typedef enum {
    JOIN_REQUEST = 1,
    JOIN_COMMIT = 6,
} join_message_type_t;

typedef struct {
    uint32_t request_id;
    char from_name[MAX_NODE_NAME_SIZE];
} join_ack_u_t;

typedef struct {
    uint32_t request_id;
    char to_name[MAX_NODE_NAME_SIZE];
} join_ack_ack_u_t;


typedef struct {
    uint32_t magic; //0xAAAABBBB
    uint16_t type;
    uint16_t reserved;
    uint32_t request_id;
} join_message_header_t;

typedef struct {
    join_message_header_t header;
    char new_name[MAX_NODE_NAME_SIZE];
    uint32_t topo_version;
} join_commit_t;

typedef struct {
    join_message_header_t header;
    char node_name[MAX_NODE_NAME_SIZE];
    uint16_t unicast_port;
} join_request_t;

typedef struct {
    join_message_header_t header;
    char new_name[MAX_NODE_NAME_SIZE];
    uint16_t new_unicast_port;

    char before_name[MAX_NODE_NAME_SIZE];
    uint16_t before_unicast_port;

    char prev_name[MAX_NODE_NAME_SIZE];
    uint16_t prev_unicast_port;
} join_accept_t;

typedef struct {
    join_message_header_t header;
    char from_name[MAX_NODE_NAME_SIZE];
} join_ack_t;

typedef struct {
    uint32_t request_id;
    char from_name[MAX_NODE_NAME_SIZE];
} join_confirm_t;

typedef struct {
    uint32_t request_id;
    char node_name[MAX_NODE_NAME_SIZE];
} completed_join_t;

typedef struct {
    int used;
    uint32_t request_id;
    char node_name[MAX_NODE_NAME_SIZE];
    uint16_t unicast_port;
    struct in_addr ip;
    time_t last_seen;
} pending_join_t;

typedef struct {
    int active;
    uint32_t request_id;

    pending_join_t joiner;

    char expected_prev_name[MAX_NODE_NAME_SIZE];
    int got_confirm_prev;
    int got_confirm_joiner;

    time_t last_sent;
    int retries;
} join_inflight_t;

typedef struct {
    pending_join_t joins[MAX_PENDING_JOINS];
    size_t count;
    completed_join_t completed[MAX_PENDING_JOINS];
    size_t completed_count;
} join_state_t;

int add_pending_join(join_state_t* state, const join_request_t* request, struct in_addr ip);
int remove_pending_join(join_state_t* state, const char* node_name);
int pop_oldest_pending_join(join_state_t* state, pending_join_t* out);
size_t drop_oldest_pending_joins(join_state_t* state, size_t max_remove);
int join_state_is_completed(const join_state_t* state, const join_request_t* request);
void join_state_record_completed(join_state_t* state, const pending_join_t* joiner);
void prune_joins(join_state_t* state, time_t now, int ttl_seconds);
void join_state_mark_completed_and_prune_pending(join_state_t* state, const char* node_name, uint32_t request_id);

#endif
