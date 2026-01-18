#include "logger.h"
#include "ring.h"
#include "route.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        LOG_ERROR("Usage: %s <joined: 0|1>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int joined = atoi(argv[1]);

    route_t current = {0};
    route_t prev = {0};
    route_t next = {0};

    route_config_t config = {.current = &current, .prev = &prev, .next = &next};
    descriptors_t descriptors = {0};

    if (ring_initialize(&config, &descriptors, joined) < 0) {
        exit(EXIT_FAILURE);
    }

    if (ring_run(config, descriptors, joined) < 0) {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
