#ifndef BROADCAST_H
#define BROADCAST_H

#include "join.h"
#include "ring.h"
#include "route.h"

int broadcast_setup_socket(const route_t* current);
int handle_broadcast(int broadcast_socket, ring_state_t* ring_state);
int join_inflight_tick(ring_state_t* ring_state, int broadcast_socket);

#endif