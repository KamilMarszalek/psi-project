#ifndef UNICAST_DISPATCH_H
#define UNICAST_DISPATCH_H

#include "ring.h"
#include "unicast_msg.h"

int unicast_dispatch_message(ring_state_t* state, const unicast_msg_t* msg, int unicast_socket);

#endif
