#include "ring.h"
#include "route.h"

#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    (void) argc;
    (void) argv;

    route_t current = {0};
    route_t prev = {0};
    route_t next = {0};

    route_config_t config = {.current = &current, .prev = &prev, .next = &next};
    descriptors_t descriptors = {0};

    if (ring_initalize(&config, &descriptors) < 0) {
        exit(EXIT_FAILURE);
    }

    if (ring_run(config, descriptors) < 0) {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
