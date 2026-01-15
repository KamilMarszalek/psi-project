#ifndef UNICAST_H
#define UNICAST_H

#include "route.h"
#include "token.h"

int unicast_setup(route_t* current);
int unicast_forward_first_token(int unicast_socket, route_t* next);
int unicast_handle(int unicast_socket, token_t* token, route_t* next);

#endif
