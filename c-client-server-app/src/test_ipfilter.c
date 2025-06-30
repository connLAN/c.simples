#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

void test_ip(const char *ip) {
    printf("Testing IP: %s\n", ip);
    printf("Blacklisted: %s\n", is_ip_blacklisted(ip) ? "YES" : "NO");
    printf("Whitelisted: %s\n", is_ip_whitelisted(ip) ? "YES" : "NO");
    printf("---\n");
}

int main() {
    // Initialize IP lists
    init_ip_lists();
    
    // Test known blacklisted IPs
    test_ip("192.168.1.1");
    test_ip("10.0.0.1");
    
    // Test special case
    test_ip("127.0.0.1");
    
    // Test unknown IP
    test_ip("8.8.8.8");
    
    // Test invalid IP
    test_ip("invalid.ip.address");
    
    // Cleanup happens automatically via atexit
    
    return 0;
}