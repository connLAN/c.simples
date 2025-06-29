#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "ipfilter.h"

#ifdef USE_LOGGING
#include "log.h"
#else
// Define empty logging macros when not using logging
#define log_debug(...)
#define log_info(...)
#endif

#define MAX_IPS 100
#define IP_LENGTH 16

// Global arrays to store IPs
char blacklist_ips[MAX_IPS][IP_LENGTH];
char whitelist_ips[MAX_IPS][IP_LENGTH];
size_t blacklist_count = 0;
size_t whitelist_count = 0;

// Helper function to load IPs from file
static void load_ips(const char *filename, char ips[][IP_LENGTH], size_t *count) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open IP list file");
        return;
    }

    char line[IP_LENGTH];
    *count = 0;
    
    while (fgets(line, sizeof(line), file) && *count < MAX_IPS) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        
        // Remove newline character if present
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        strncpy(ips[*count], line, IP_LENGTH-1);
        ips[*count][IP_LENGTH-1] = '\0';
        (*count)++;
    }

    fclose(file);
}

// Initialize IP lists
void init_ip_lists() {
    log_debug("Current working directory: ");
    system("pwd");
    
    log_info("Loading IP blacklist from: src/blacklist.txt");
    load_ips("src/blacklist.txt", blacklist_ips, &blacklist_count);
    
    log_info("Loading IP whitelist from: src/whitelist.txt");
    load_ips("src/whitelist.txt", whitelist_ips, &whitelist_count);
    
    printf("Debug: Loaded %zu blacklisted IPs\n", blacklist_count);
    log_debug("Loaded %zu blacklisted IPs:", blacklist_count);
    for (int i = 0; i < blacklist_count; i++) {
        log_debug("- %s", blacklist_ips[i]);
    }
    
    log_debug("Loaded %d whitelisted IPs:", whitelist_count);
    for (int i = 0; i < whitelist_count; i++) {
        log_debug("- %s", whitelist_ips[i]);
    }
    
    // Verify we loaded the expected test IPs
    bool has_test_ip1 = false;
    bool has_test_ip2 = false;
    for (int i = 0; i < blacklist_count; i++) {
        if (strcmp(blacklist_ips[i], "192.168.1.1") == 0) has_test_ip1 = true;
        if (strcmp(blacklist_ips[i], "10.0.0.1") == 0) has_test_ip2 = true;
    }
    if (!has_test_ip1 || !has_test_ip2) {
        log_info("WARNING: Test IPs not found in blacklist!");
    }
}

// Check if IP is blacklisted
int is_ip_blacklisted(const char *ip) {
    // Always allow loopback IP (127.0.0.1)
    if (strcmp(ip, "127.0.0.1") == 0) {
        return 0;
    }

    log_debug("Checking if %s is blacklisted", ip);
    for (int i = 0; i < blacklist_count; i++) {
        log_debug("Comparing with blacklist entry: %s", blacklist_ips[i]);
        if (strcmp(ip, blacklist_ips[i]) == 0) {
            log_info("Blocked connection from blacklisted IP: %s", ip);
            return 1;
        }
    }
    return 0;
}

// Check if IP is whitelisted
int is_ip_whitelisted(const char *ip) {
    log_debug("Checking if %s is whitelisted", ip);
    for (int i = 0; i < whitelist_count; i++) {
        log_debug("Comparing with whitelist entry: %s", whitelist_ips[i]);
        if (strcmp(ip, whitelist_ips[i]) == 0) {
            log_info("Allowed connection from whitelisted IP: %s", ip);
            return 1;
        }
    }
    return 0;
}