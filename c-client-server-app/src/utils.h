#ifndef UTILS_H
#define UTILS_H

extern size_t blacklist_count;
extern size_t whitelist_count;

// Initialize IP lists (call this once at program start)
void init_ip_lists();

// Cleanup IP lists (called automatically at exit)
void cleanup_ip_lists();

// Check if IP is blacklisted
int is_ip_blacklisted(const char *ip);

// Check if IP is whitelisted
int is_ip_whitelisted(const char *ip);

#endif // UTILS_H