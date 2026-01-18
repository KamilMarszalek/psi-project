#ifndef UNICAST_H
#define UNICAST_H

#include "route.h"
#include "token.h"

typedef struct TokenPair {
    token_t* from_unicast;
    token_t* from_cli;
} token_pair_t;

int unicast_setup_socket(route_t* current);
int unicast_forward_first_token(int unicast_socket, route_t* next);
int unicast_handle(int unicast_socket, token_pair_t* tokens, route_config_t* config);

#endif
