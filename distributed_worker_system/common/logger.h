/**
 * logger.h - Logging System
 * 
 * This file provides functions for logging messages at different levels.
 */

#ifndef LOGGER_H
#define LOGGER_H

/* Log levels */
typedef enum {
    LOG_DEBUG = 0,  /* Debug messages (most verbose) */
    LOG_INFO,       /* Informational messages */
    LOG_WARNING,    /* Warning messages */
    LOG_ERROR,      /* Error messages */
    LOG_FATAL       /* Fatal error messages (least verbose) */
} log_level_t;

/**
 * Initialize the logger
 * 
 * @param app_name Name of the application
 * @param log_file Path to the log file (NULL for console only)
 * @param level Minimum log level to record
 * @return 0 on success, -1 on error
 */
int logger_init(const char *app_name, const char *log_file, log_level_t level);

/**
 * Close the logger and free resources
 */
void logger_close(void);

/**
 * Set the minimum log level
 * 
 * @param level New minimum log level
 */
void logger_set_level(log_level_t level);

/**
 * Log a message at the specified level
 * 
 * @param level Log level
 * @param file Source file name
 * @param line Source line number
 * @param func Function name
 * @param format Format string (printf style)
 * @param ... Additional arguments for format string
 */
void logger_log(log_level_t level, const char *file, int line, 
               const char *func, const char *format, ...);

/* Convenience macros for logging */
#define LOG_DEBUG(format, ...) \
    logger_log(LOG_DEBUG, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

#define LOG_INFO(format, ...) \
    logger_log(LOG_INFO, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

#define LOG_WARNING(format, ...) \
    logger_log(LOG_WARNING, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

#define LOG_ERROR(format, ...) \
    logger_log(LOG_ERROR, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

#define LOG_FATAL(format, ...) \
    logger_log(LOG_FATAL, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

#endif /* LOGGER_H */