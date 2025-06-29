#include "unity.h"
#include "utils.h"

void setUp(void) {}
void tearDown(void) {}

void test_ip_validation(void) {
    init_ip_lists();
    
    // Test known blocked IP
    TEST_ASSERT_EQUAL(1, is_ip_blacklisted("192.168.1.100"));
    
    // Test known allowed IP 
    TEST_ASSERT_EQUAL(1, is_ip_whitelisted("192.168.1.101"));
    
    // Test unknown IP
    TEST_ASSERT_EQUAL(0, is_ip_blacklisted("10.0.0.100"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ip_validation);
    return UNITY_END();
}
