#ifndef IPFILTER_H
#define IPFILTER_H

#include <stddef.h>
#include <stdbool.h>

// Global counters
extern size_t blacklist_count;
extern size_t whitelist_count;

// IP lists
extern char blacklist_ips[][16];  // Actual size defined in utils.c
extern char whitelist_ips[][16];  // Actual size defined in utils.c

// Function declarations
void init_ip_lists();
int is_ip_blacklisted(const char* ip);
int is_ip_whitelisted(const char* ip);

#endif // IPFILTER_H