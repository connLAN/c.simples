#ifndef LOGGER_H
#define LOGGER_H

#define LOG_LEVEL_DEBUG 1
#define LOG_LEVEL_INFO  2
#define LOG_LEVEL_ERROR 3

extern int current_log_level;

void log_debug(const char *format, ...);
void log_info(const char *format, ...);
void log_error(const char *format, ...);

#endif
