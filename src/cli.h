#ifndef CLI_H
#define CLI_H

#include "token.h"

int cli_setup_reader(const char* filename, int permissions);
int cli_setup_writer(const char* filename, int permissions);
int cli_handle_read(int cli_fd, token_t* token);

#endif
