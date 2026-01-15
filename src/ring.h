#ifndef RING_H
#define RING_H

#include "route.h"
#include <stdbool.h>

#define MAX_TOKEN_SIZE 256

typedef struct Token {
    char data[MAX_TOKEN_SIZE];
    char sender[MAX_NODE_NAME_SIZE];
    char reciever[MAX_NODE_NAME_SIZE];
    bool is_empty;
} token_t;


typedef struct Sockets {
    int unicast_socket;
    // int broadcast_socket;
    // int cli_socket;
} sockets_t;


int setup_unicast(route_t* config);
int setup_broadcast_socket();
int setup_cli_socket();


int forward_token(route_t* config, int unicast_sock, token_t* token);

int event_loop(route_config_t* config, sockets_t* sockets);

#endif
