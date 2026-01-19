#ifndef TOKEN_H
#define TOKEN_H

#include "consts.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct Token {
    char data[MAX_DATA_SIZE];
    char sender[MAX_NODE_NAME_SIZE];
    char receiver[MAX_NODE_NAME_SIZE];
    uint32_t topo_version;
    bool is_empty;
} token_t;

#endif
