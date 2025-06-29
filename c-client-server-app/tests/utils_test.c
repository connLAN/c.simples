#define TEST_PRINTF printf
#include "unity/unity.h"
#include "../src/utils.h"
#include <stdio.h>

void setUp(void) {
    TEST_PRINTF("Initializing test environment...\n");
    TEST_PRINTF("Calling init_ip_lists()...\n");
    init_ip_lists();
    TEST_PRINTF("init_ip_lists() called\n");
    
    // 添加临时测试代码验证IP列表加载
    TEST_PRINTF("Verifying blacklist...\n");
    TEST_ASSERT_EQUAL(1, is_ip_blacklisted("192.168.1.1"));
    TEST_PRINTF("Blacklist verification complete\n");
}

void tearDown(void) {
    printf("Cleaning up test environment...\n");
}

void test_ip_list_checks(void) {
    printf("Running IP list checks...\n");
    printf("\n--- Testing IP list checks ---\n");
    
    // 测试黑名单功能
    printf("\nTesting blacklist:\n");
    printf("Checking 192.168.1.1 (should be blacklisted): ");
    int black_result1 = is_ip_blacklisted("192.168.1.1");
    printf("%d\n", black_result1);
    TEST_ASSERT_EQUAL(1, black_result1);
    
    printf("Checking 10.0.0.1 (should NOT be blacklisted): ");
    int black_result2 = is_ip_blacklisted("10.0.0.1");
    printf("%d\n", black_result2);
    TEST_ASSERT_EQUAL(0, black_result2);
    
    // 测试白名单功能
    printf("\nTesting whitelist:\n");
    printf("Checking 10.0.0.1 (should be whitelisted): ");
    int white_result1 = is_ip_whitelisted("10.0.0.1");
    printf("%d\n", white_result1);
    TEST_ASSERT_EQUAL(1, white_result1);
    
    printf("Checking 192.168.1.1 (should NOT be whitelisted): ");
    int white_result2 = is_ip_whitelisted("192.168.1.1");
    printf("%d\n", white_result2);
    TEST_ASSERT_EQUAL(0, white_result2);
}

int main(void) {
    UnityBegin("utils_test.c");
    RUN_TEST(test_ip_list_checks);
    UnityEnd();
    return 0;
}