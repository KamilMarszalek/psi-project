#include <stdio.h>
#include <string.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "consts.h"
#include "ring.h"
#include "route.h"
#include "token.h"


int main(int argc, char** argv) {
    (void) argc;
    (void) argv;

    route_config_t config = {0};
    sockets_t sockets = {0};

    if (ring_initalize(&config, &sockets) < 0) {
        exit(EXIT_FAILURE);
    }

    if (ring_run(&config, &sockets) < 0) {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
