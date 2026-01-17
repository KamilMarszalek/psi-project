#ifndef LOGGER_H
#define LOGGER_H

#define LOG_LEVEL_DEBUG 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_WARN 3
#define LOG_LEVEL_ERROR 4

#define LOG_LEVEL LOG_LEVEL_INFO // this can be used to set log level

void logger(int level, const char* tag, const char* message, ...);

#define LOG_DEBUG(fmt, ...) logger(LOG_LEVEL_DEBUG, "DEBUG", fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) logger(LOG_LEVEL_INFO, "INFO", fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) logger(LOG_LEVEL_WARN, "WARN", fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) logger(LOG_LEVEL_ERROR, "ERROR", fmt, ##__VA_ARGS__)

#endif
