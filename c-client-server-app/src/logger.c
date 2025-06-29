#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "logger.h"

int current_log_level = LOG_LEVEL_INFO;

void log_print(const char *level, const char *format, va_list args) {
    time_t now;
    time(&now);
    struct tm *tm_info = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    printf("[%s][%s] ", timestamp, level);
    vprintf(format, args);
    printf("\n");
}

void log_debug(const char *format, ...) {
    if (current_log_level <= LOG_LEVEL_DEBUG) {
        va_list args;
        va_start(args, format);
        log_print("DEBUG", format, args);
        va_end(args);
    }
}

void log_info(const char *format, ...) {
    if (current_log_level <= LOG_LEVEL_INFO) {
        va_list args;
        va_start(args, format);
        log_print("INFO", format, args);
        va_end(args);
    }
}

void log_error(const char *format, ...) {
    if (current_log_level <= LOG_LEVEL_ERROR) {
        va_list args;
        va_start(args, format);
        log_print("ERROR", format, args);
        va_end(args);
        va_start(args, format);
        log_print("ERROR", format, args);
        va_end(args);
    }
}
