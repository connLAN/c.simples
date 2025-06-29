#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "log.h"

static const char *level_names[] = {
    "DEBUG", "INFO", "WARN", "ERROR"
};

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
LogLevel log_level = LOG_INFO;

void log_set_level(LogLevel level) {
    log_level = level;
}

 void log_message(LogLevel level, const char *format, ...) {
    if (level < log_level) {
        return;
    }

    time_t now;
    time(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    pthread_mutex_lock(&log_mutex);
    
    fprintf(stderr, "%s %-5s ", timestamp, level_names[level]);
    
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    
    fprintf(stderr, "\n");
    fflush(stderr);
    
    pthread_mutex_unlock(&log_mutex);
}