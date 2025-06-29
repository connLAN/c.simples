#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "logger.h"

void test_ip(const char* ip) {
    printf("\nTesting IP: %s\n", ip ? ip : "NULL");
    printf("Whitelisted: %s\n", is_ip_whitelisted(ip) ? "YES" : "NO");
    printf("Blacklisted: %s\n", is_ip_blacklisted(ip) ? "YES" : "NO");
}

int main() {
    // Initialize logger
    init_logger("test_lib.log");
    
    // Initialize IP lists
    init_ip_lists();

    // Test various IP cases
    test_ip("192.168.1.1");    // Common private IP
    test_ip("8.8.8.8");        // Google DNS
    test_ip("10.0.0.1");       // Another private IP
    test_ip("invalid.ip");     // Invalid format
    
    // Edge cases
    test_ip("");               // Empty string
    test_ip(NULL);             // NULL pointer
    
    // Clean up
    close_logger();
    
    printf("\nAll tests completed\n");
    return 0;
}