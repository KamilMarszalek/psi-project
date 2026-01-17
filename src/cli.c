#include "cli.h"

#include "logger.h"
#include "token.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


int cli_setup_reader(const char* filename, int permissions) {
    if (mkfifo(filename, permissions) < 0 && errno != EEXIST) {
        LOG_ERROR("Creating fifo: %s", strerror(errno));
        return -1;
    }

    int cli_fd = open(filename, O_RDWR);
    if (cli_fd < 0) {
        LOG_ERROR("Opening fifo: %s", strerror(errno));
        return -1;
    }

    return cli_fd;
}

int cli_setup_writer(const char* filename, int permissions) {
    if (mkfifo(filename, permissions) < 0 && errno != EEXIST) {
        LOG_ERROR("Creating fifo: %s", strerror(errno));
        return -1;
    }

    int cli_fd = open(filename, O_WRONLY);
    if (cli_fd < 0) {
        LOG_ERROR("Opening fifo: %s", strerror(errno));
        return -1;
    }

    return cli_fd;
}

int cli_handle_read(int cli_fd, token_t* token) {
    if (read(cli_fd, token, sizeof(*token)) < 0) {
        LOG_ERROR("Reading token from fifo: %s", strerror(errno));
        return -1;
    }

    return 0;
}
