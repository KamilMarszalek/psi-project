#ifndef BROADCAST_H
#define BROADCAST_H

#include "route.h"


int broadcast_setup_socket(const route_t* current);
int handle_broadcast(int broadcast_socket);

#endif