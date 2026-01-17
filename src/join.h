#ifndef JOIN_H
#define JOIN_H

#include "consts.h"
#include <stdint.h>

typedef struct {
    uint32_t magic;//0xAAAABBBB
    uint32_t request_id;
    char node_name[MAX_NODE_NAME_SIZE];
    uint16_t unicast_port;
} join_request_t;

#endif