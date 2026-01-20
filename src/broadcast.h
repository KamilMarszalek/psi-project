#ifndef BROADCAST_H
#define BROADCAST_H

#include "join.h"
#include "ring.h"
#include "route.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int broadcast_setup_socket(const route_t* current);
int handle_broadcast(int broadcast_socket, ring_state_t* ring_state);

int broadcast_send_join_request(
    int broadcast_socket, uint32_t request_id, const char* node_name, uint16_t unicast_port
);

int broadcast_send_join_accept(int broadcast_socket, const join_accept_t* accept_host);
int broadcast_send_join_ack(int broadcast_socket, uint32_t request_id, const char* from_name);

#endif
