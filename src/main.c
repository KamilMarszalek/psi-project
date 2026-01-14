#define TOKEN_DATA_SIZE 256

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "ring.h"

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;

    route_config_t config = {0};
    if (fill_config(&config) != 0) {
        exit(EXIT_FAILURE);
    }

    return 0;
}
