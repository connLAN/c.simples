/**
 * logger.c - Logging System Implementation
 */

#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

/* Maximum length of a log message */
#define MAX_LOG_MSG_LEN 1024

/* Logger state */
static struct {
    FILE *log_file;             /* Log file handle */
    char *app_name;             /* Application name */
    log_level_t level;          /* Current log level */
    pthread_mutex_t mutex;      /* Mutex for thread safety */
    int initialized;            /* Flag indicating if logger is initialized */
} logger = {NULL, NULL, LOG_INFO, PTHREAD_MUTEX_INITIALIZER, 0};

/* Log level names */
static const char *level_names[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "FATAL"
};

/* Log level colors (ANSI escape codes) */
static const char *level_colors[] = {
    "\x1B[36m",  /* Cyan for DEBUG */
    "\x1B[32m",  /* Green for INFO */
    "\x1B[33m",  /* Yellow for WARNING */
    "\x1B[31m",  /* Red for ERROR */
    "\x1B[35m"   /* Magenta for FATAL */
};

/* ANSI color reset */
static const char *color_reset = "\x1B[0m";

/* Initialize the logger */
int logger_init(const char *app_name, const char *log_file, log_level_t level) {
    pthread_mutex_lock(&logger.mutex);

    /* Check if already initialized */
    if (logger.initialized) {
        pthread_mutex_unlock(&logger.mutex);
        return -1;
    }

    /* Set application name */
    if (app_name != NULL) {
        logger.app_name = strdup(app_name);
        if (logger.app_name == NULL) {
            pthread_mutex_unlock(&logger.mutex);
            return -1;
        }
    } else {
        logger.app_name = strdup("App");
    }

    /* Open log file if specified */
    if (log_file != NULL) {
        logger.log_file = fopen(log_file, "a");
        if (logger.log_file == NULL) {
            free(logger.app_name);
            logger.app_name = NULL;
            pthread_mutex_unlock(&logger.mutex);
            return -1;
        }
    }

    /* Set log level */
    logger.level = level;

    logger.initialized = 1;

    pthread_mutex_unlock(&logger.mutex);

    /* Log initialization message */
    LOG_INFO("Logger initialized");

    return 0;
}

/* Close the logger and free resources */
void logger_close(void) {
    pthread_mutex_lock(&logger.mutex);

    if (!logger.initialized) {
        pthread_mutex_unlock(&logger.mutex);
        return;
    }

    /* Log closing message */
    if (logger.initialized) {
        logger_log(LOG_INFO, __FILE__, __LINE__, __func__, "Logger closed");
    }

    /* Close log file */
    if (logger.log_file != NULL) {
        fclose(logger.log_file);
        logger.log_file = NULL;
    }

    /* Free application name */
    if (logger.app_name != NULL) {
        free(logger.app_name);
        logger.app_name = NULL;
    }

    logger.initialized = 0;

    pthread_mutex_unlock(&logger.mutex);
}

/* Set the minimum log level */
void logger_set_level(log_level_t level) {
    pthread_mutex_lock(&logger.mutex);
    logger.level = level;
    pthread_mutex_unlock(&logger.mutex);
}

/* Get current time as formatted string */
static void get_time_str(char *buffer, size_t size) {
    time_t now;
    struct tm *timeinfo;

    time(&now);
    timeinfo = localtime(&now);

    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", timeinfo);
}

/* Log a message at the specified level */
void logger_log(log_level_t level, const char *file, int line,
               const char *func, const char *format, ...) {
    va_list args;
    char time_str[20];
    char message[MAX_LOG_MSG_LEN];
    char full_message[MAX_LOG_MSG_LEN];
    int console_output = 1;  /* Always output to console */

    /* Check if message should be logged based on level */
    if (level < logger.level) {
        return;
    }

    /* Format the message */
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    /* Get current time */
    get_time_str(time_str, sizeof(time_str));

    /* Create full message with metadata */
    snprintf(full_message, sizeof(full_message),
            "[%s] [%s] [%s:%d] [%s] %s\n",
            time_str, level_names[level], file, line, func, message);

    pthread_mutex_lock(&logger.mutex);

    /* Write to console with colors */
    if (console_output) {
        fprintf(stdout, "%s%s%s", level_colors[level], full_message, color_reset);
        fflush(stdout);
    }

    /* Write to log file without colors */
    if (logger.log_file != NULL) {
        fprintf(logger.log_file, "%s", full_message);
        fflush(logger.log_file);
    }

    pthread_mutex_unlock(&logger.mutex);
}