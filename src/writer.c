#include "cli.h"
#include "consts.h"
#include "token.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Args missing. Usage: %s <data> <reciever_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (strlen(argv[1]) > MAX_DATA_SIZE - 1) {
        fprintf(stderr, "Max data size (%d) exceeded\n", MAX_DATA_SIZE);
        exit(EXIT_FAILURE);
    }

    if (strlen(argv[2]) > MAX_NODE_NAME_SIZE - 1) {
        fprintf(stderr, "Max node name size (%d) exceeded\n", MAX_NODE_NAME_SIZE);
        exit(EXIT_FAILURE);
    }

    char* sender_name_env = getenv("NODE_NAME");
    if (!sender_name_env) {
        fprintf(stderr, "Sender name env variable is missing\n");
        exit(EXIT_FAILURE);
    }

    token_t token = {.is_empty = false};
    strncpy(token.data, argv[1], MAX_DATA_SIZE - 1);
    strncpy(token.reciever, argv[2], MAX_NODE_NAME_SIZE - 1);
    strncpy(token.sender, sender_name_env, MAX_NODE_NAME_SIZE - 1);

    int cli_fd = cli_setup_writer(FIFO_FILE, FIOF_FILE_PERMISSIONS);
    if (cli_fd < 0) {
        exit(EXIT_FAILURE);
    }

    if (write(cli_fd, &token, sizeof(token)) < 0) {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
