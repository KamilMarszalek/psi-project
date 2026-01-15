#include <stdio.h>
#include <string.h>
#define TOKEN_DATA_SIZE 256

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "ring.h"
#include "route.h"

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;

    route_config_t config = {0};
    if (fill_config(&config) < 0) {
        exit(EXIT_FAILURE);
    }

    int unicast_socket = setup_unicast(&config.current);
    if (unicast_socket < 0) {
        exit(EXIT_FAILURE);
    }

    if (getenv("SHOULD_START")) {
        sleep(10);// make sure every other node is already initalized, maybe something like 'READY' should be sent
        token_t token = {0};
        strncpy(token.data, "very important message", MAX_TOKEN_SIZE);
        strncpy(token.sender, config.current.node_name, MAX_NODE_NAME_SIZE);
        strncpy(token.reciever, config.next.node_name, MAX_NODE_NAME_SIZE);
        token.is_empty = false;
        if (forward_token(&config.next, unicast_socket, &token) < 0) {
            perror("forwarding first token");
            exit(EXIT_FAILURE);
        }
    }

    sockets_t socks = {.unicast_socket = unicast_socket};
    if (event_loop(&config, &socks) < 0) {
        exit(EXIT_FAILURE);
    }

    return 0;
}
