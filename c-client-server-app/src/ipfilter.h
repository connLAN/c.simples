#ifndef IPFILTER_H
#define IPFILTER_H

#include <stdbool.h>
#include <stddef.h>

// Opaque handle type
typedef struct ipfilter_ctx ipfilter_ctx;

// Create/destroy IP filter context
ipfilter_ctx* ipfilter_create();
void ipfilter_destroy(ipfilter_ctx* ctx);

// Initialize IP lists (load from files)
int ipfilter_init(ipfilter_ctx* ctx, const char* blacklist_file, const char* whitelist_file);

// Check IP status
bool ipfilter_is_blacklisted(ipfilter_ctx* ctx, const char* ip);
bool ipfilter_is_whitelisted(ipfilter_ctx* ctx, const char* ip);

// Get counts (optional)
size_t ipfilter_blacklist_count(ipfilter_ctx* ctx);
size_t ipfilter_whitelist_count(ipfilter_ctx* ctx);

#endif // IPFILTER_H
