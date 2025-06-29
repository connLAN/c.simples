#include "unity/unity.h"
#include <stdio.h>

void test_addition(void) {
    TEST_ASSERT_EQUAL(4, 2+2);
}

void test_subtraction(void) {
    TEST_ASSERT_EQUAL(1, 3-2);
}

int main(void) {
    UnityBegin("example_test.c");
    
    RUN_TEST(test_addition);
    RUN_TEST(test_subtraction);
    
    UnityEnd();
    return failCount != 0;
}