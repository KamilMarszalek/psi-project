#include <stdio.h>
#include <stdlib.h>

#include "ring.h"
#include "route.h"


int main(int argc, char** argv) {
    (void) argc;
    (void) argv;

    route_config_t config = {0};
    sockets_t sockets = {0};

    if (ring_initalize(&config, &sockets) < 0) {
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

    if (ring_run(&config, &sockets) < 0) {
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
