#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "log.h"

// Basic utility functions can be added here as needed

// Simple helper for checking file existence
bool file_exists(const char *path) {
    return access(path, F_OK) == 0;
}

// Helper for creating directory if it doesn't exist
bool ensure_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        if (mkdir(path, 0755) != 0) {
            LOG_ERROR("Failed to create directory: %s", path);
            return false;
        }
        return true;
    }
    return S_ISDIR(st.st_mode);
}