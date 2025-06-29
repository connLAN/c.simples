#ifndef UTILS_H
#define UTILS_H

// Initialize IP lists (call this once at program start)
void init_ip_lists();

// Check if IP is blacklisted
int is_ip_blacklisted(const char *ip);

// Check if IP is whitelisted
int is_ip_whitelisted(const char *ip);

#endif // UTILS_H