#ifndef TOKEN_H
#define TOKEN_H

#include "consts.h"
#include <stdbool.h>

typedef struct Token {
    char data[MAX_DATA_SIZE];
    char sender[MAX_NODE_NAME_SIZE];
    char receiver[MAX_NODE_NAME_SIZE];
    bool is_empty;
} token_t;

#endif
