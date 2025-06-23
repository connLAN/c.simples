#ifndef TEST_HEARTBEAT_H
#define TEST_HEARTBEAT_H

#define MAX_HISTORY 100  // Maximum number of heartbeat records to keep in history

#include <unity.h>
#include "heartbeat_server.h"

// Test function declarations
void setUp(void);
void tearDown(void);

// Core functionality tests
void test_validate_ip(void);
void test_validate_percentage(void);
void test_validate_latency(void);
void test_url_decode(void);
void test_parse_key_value(void);

// JSON processing tests
void test_process_json_data_valid(void);
void test_process_json_data_invalid(void);
void test_process_json_data_missing_fields(void);

// History management tests
void test_add_to_history(void);
void test_history_capacity(void);
void test_get_history_json(void);
void test_empty_history(void);
void test_concurrent_history_access(void);

#endif /* TEST_HEARTBEAT_H */