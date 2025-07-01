#include "unity.h"
#include "../src/error.h"

void setUp(void) {
    error_clear();
}

void tearDown(void) {
    error_clear();
}

void test_error_set_and_get(void) {
    const char* test_message = "Test error message";
    error_set(ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "%s", test_message);
    
    const app_error_t* err = error_get_last();
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_INT(ERR_INVALID_PARAM, err->code);
    TEST_ASSERT_EQUAL_STRING(test_message, err->message);
}

void test_error_code_str(void) {
    TEST_ASSERT_EQUAL_STRING("No error", error_code_str(ERR_NONE));
    TEST_ASSERT_EQUAL_STRING("Invalid parameter", error_code_str(ERR_INVALID_PARAM));
    TEST_ASSERT_EQUAL_STRING("Memory allocation failed", error_code_str(ERR_MEMORY));
    TEST_ASSERT_EQUAL_STRING("I/O operation failed", error_code_str(ERR_IO));
    TEST_ASSERT_EQUAL_STRING("Network operation failed", error_code_str(ERR_NETWORK));
    TEST_ASSERT_EQUAL_STRING("Unknown error", error_code_str(ERR_UNKNOWN));
}

void test_error_clear(void) {
    error_set(ERR_INVALID_PARAM, __FILE__, __LINE__, __func__, "Test error");
    error_clear();
    const app_error_t* err = error_get_last();
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_INT(ERR_NONE, err->code);
    TEST_ASSERT_EQUAL_STRING("", err->message);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_error_set_and_get);
    RUN_TEST(test_error_code_str);
    RUN_TEST(test_error_clear);
    return UNITY_END();
}