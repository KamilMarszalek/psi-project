#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>


void logger(int level, const char* tag, const char* message, ...) {
    if (level < LOG_LEVEL) {
        return;
    }

    time_t now = time(NULL);
    struct tm* date = localtime(&now);
    char buffer[32];

    strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", date);

    fprintf(stderr, "[%s] %s: ", tag, buffer);
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);
}
