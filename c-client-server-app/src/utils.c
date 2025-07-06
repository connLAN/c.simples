#include "utils.h"
#include "log.h"
#include "ipfilter.h"
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/stat.h>

#define MAX_IPS 100
#define IP_LENGTH 16

char blacklist_ips[MAX_IPS][IP_LENGTH];
char whitelist_ips[MAX_IPS][IP_LENGTH];
size_t blacklist_count = 0;
size_t whitelist_count = 0;
static pthread_mutex_t ip_mutex = PTHREAD_MUTEX_INITIALIZER;

bool file_exists(const char *path) {
    FILE *file = fopen(path, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

void ensure_directory(const char *path) {
    if (!file_exists(path)) {
        mkdir(path, 0777);
    }
}

static void load_ip_list(const char *filename, char list[][IP_LENGTH], size_t *count) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        LOG_ERROR("Failed to open IP list: %s", filename);
        return;
    }

    char line[IP_LENGTH];
    while (*count < MAX_IPS && fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0'; // Remove newline
        if (strlen(line) > 0) {
            strncpy(list[*count], line, IP_LENGTH-1);
            list[*count][IP_LENGTH-1] = '\0';
            (*count)++;
        }
    }
    fclose(file);
}

void init_ip_lists() {
    pthread_mutex_lock(&ip_mutex);
    
    load_ip_list("blacklist.txt", blacklist_ips, &blacklist_count);
    load_ip_list("whitelist.txt", whitelist_ips, &whitelist_count);
    
    pthread_mutex_unlock(&ip_mutex);
}

int is_ip_blacklisted(const char *ip) {
    pthread_mutex_lock(&ip_mutex);
    
    for (int i = 0; i < blacklist_count; i++) {
        if (strcmp(ip, blacklist_ips[i]) == 0) {
            pthread_mutex_unlock(&ip_mutex);
            return 1;
        }
    }
    
    pthread_mutex_unlock(&ip_mutex);
    return 0;
}

int is_ip_whitelisted(const char *ip) {
    pthread_mutex_lock(&ip_mutex);
    
    for (int i = 0; i < whitelist_count; i++) {
        if (strcmp(ip, whitelist_ips[i]) == 0) {
            pthread_mutex_unlock(&ip_mutex);
            return 1;
        }
    }
    
    pthread_mutex_unlock(&ip_mutex);
    return 0;
}

void cleanup_ip_lists() {
    pthread_mutex_destroy(&ip_mutex);
}
