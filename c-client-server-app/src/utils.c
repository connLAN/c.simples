#include "utils.h"
#include "log.h"
#include "ipfilter.h"
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>

#define MAX_IPS 100
#define IP_LENGTH 16

struct ipfilter_ctx {
    char blacklist_ips[MAX_IPS][IP_LENGTH];
    char whitelist_ips[MAX_IPS][IP_LENGTH];
    size_t blacklist_count;
    size_t whitelist_count;
    pthread_mutex_t ip_mutex;
};

// Helper functions
static bool file_exists(const char *path) {
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

// Create new IP filter context
ipfilter_ctx* ipfilter_create() {
    ipfilter_ctx* ctx = malloc(sizeof(ipfilter_ctx));
    if (!ctx) return NULL;
    
    ctx->blacklist_count = 0;
    ctx->whitelist_count = 0;
    pthread_mutex_init(&ctx->ip_mutex, NULL);
    return ctx;
}

// Destroy IP filter context
void ipfilter_destroy(ipfilter_ctx* ctx) {
    if (!ctx) return;
    pthread_mutex_destroy(&ctx->ip_mutex);
    free(ctx);
}

// Initialize IP lists from files
int ipfilter_init(ipfilter_ctx* ctx, const char* blacklist_file, const char* whitelist_file) {
    if (!ctx) return -1;
    
    pthread_mutex_lock(&ctx->ip_mutex);
    load_ip_list(blacklist_file, ctx->blacklist_ips, &ctx->blacklist_count);
    load_ip_list(whitelist_file, ctx->whitelist_ips, &ctx->whitelist_count);
    pthread_mutex_unlock(&ctx->ip_mutex);
    return 0;
}

// Check if IP is blacklisted
bool ipfilter_is_blacklisted(ipfilter_ctx* ctx, const char* ip) {
    if (!ctx) return false;
    
    pthread_mutex_lock(&ctx->ip_mutex);
    for (size_t i = 0; i < ctx->blacklist_count; i++) {
        if (strcmp(ip, ctx->blacklist_ips[i]) == 0) {
            pthread_mutex_unlock(&ctx->ip_mutex);
            return true;
        }
    }
    pthread_mutex_unlock(&ctx->ip_mutex);
    return false;
}

// Check if IP is whitelisted
bool ipfilter_is_whitelisted(ipfilter_ctx* ctx, const char* ip) {
    if (!ctx) return false;
    
    pthread_mutex_lock(&ctx->ip_mutex);
    for (size_t i = 0; i < ctx->whitelist_count; i++) {
        if (strcmp(ip, ctx->whitelist_ips[i]) == 0) {
            pthread_mutex_unlock(&ctx->ip_mutex);
            return true;
        }
    }
    pthread_mutex_unlock(&ctx->ip_mutex);
    return false;
}

// Get blacklist count
size_t ipfilter_blacklist_count(ipfilter_ctx* ctx) {
    if (!ctx) return 0;
    return ctx->blacklist_count;
}

// Get whitelist count
size_t ipfilter_whitelist_count(ipfilter_ctx* ctx) {
    if (!ctx) return 0;
    return ctx->whitelist_count;
}
