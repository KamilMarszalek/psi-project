#include "cli.h"

#include "token.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


int cli_setup_reader(const char* filename, int permissions) {
    if (mkfifo(filename, permissions) < 0 && errno != EEXIST) {
        perror("creating fifo");
        return -1;
    }

    int cli_fd = open(filename, O_RDWR);
    if (cli_fd < 0) {
        perror("opening fifo");
        return -1;
    }

    return cli_fd;
}

int cli_setup_writer(const char* filename, int permissions) {
    if (mkfifo(filename, permissions) < 0 && errno != EEXIST) {
        perror("creating fifo");
        return -1;
    }

    int cli_fd = open(filename, O_WRONLY);
    if (cli_fd < 0) {
        perror("opening fifo");
        return -1;
    }

    return cli_fd;
}

int cli_handle_read(int cli_fd, token_t* token) {
    if (read(cli_fd, token, sizeof(*token)) < 0) {
        perror("reading token from fifo");
        return -1;
    }

    return 0;
}
