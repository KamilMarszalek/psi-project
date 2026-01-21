#ifndef UNICAST_MSG_H
#define UNICAST_MSG_H

#include "consts.h"
#include <stdint.h>

typedef enum {
    UMSG_TOKEN = 1,
    UMSG_JOIN_ACCEPT_U = 2,
    UMSG_JOIN_ACK_U = 4,
    UMSG_JOIN_ACK_ACK_U = 5,
} unicast_msg_type_t;


typedef struct {
    uint16_t type;
    uint16_t payload_len;
    uint8_t payload[MAX_UNICAST_PAYLOAD];
} unicast_msg_t;

#endif
