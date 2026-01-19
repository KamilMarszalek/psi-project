#ifndef UNICAST_H
#define UNICAST_H

#include "route.h"
#include "token.h"
#include "unicast_msg.h"

typedef struct TokenPair {
    token_t* from_unicast;
    token_t* from_cli;
} token_pair_t;


int unicast_setup_socket(route_t* current);
int unicast_recv(int unicast_socket, unicast_msg_t* msg);
int unicast_send(int unicast_socket, const unicast_msg_t* msg, const route_t* next);

#endif
