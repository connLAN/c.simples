/**
 * @file common.c
 * @brief Implementation of common utilities for distributed worker system
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include "common.h"

// Static variable to help generate unique IDs
static int next_id = 1;

long long get_timestamp_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec) * 1000 + (long long)(tv.tv_usec) / 1000;
}

char *get_timestamp_str(char *buffer, size_t size) {
    time_t now;
    struct tm *timeinfo;
    
    time(&now);
    timeinfo = localtime(&now);
    
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", timeinfo);
    return buffer;
}

const char *get_job_type_name(int job_type) {
    switch (job_type) {
        case JOB_TYPE_ECHO:
            return "Echo";
        case JOB_TYPE_REVERSE:
            return "Reverse";
        case JOB_TYPE_UPPERCASE:
            return "Uppercase";
        case JOB_TYPE_LOWERCASE:
            return "Lowercase";
        case JOB_TYPE_COUNT_CHARS:
            return "Count Characters";
        case JOB_TYPE_CUSTOM:
            return "Custom";
        default:
            return "Unknown";
    }
}

int generate_unique_id() {
    // Simple implementation that combines process ID and a counter
    // In a production system, you might want a more robust solution
    int id = (getpid() << 16) | (next_id++ & 0xFFFF);
    
    // Ensure ID is positive
    if (id <= 0) {
        id = next_id++;
    }
    
    return id;
}