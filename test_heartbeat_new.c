#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <json-c/json.h>
#include "unity.h"
#include "test_heartbeat.h"
#include "heartbeat_server.h"

// Test setup function
void setUp(void) {
    // Initialize history through proper API
    clear_heartbeat_history();
}

// Test cleanup function
void tearDown(void) {
    // Clean up through proper API
    clear_heartbeat_history();
}

// Test IP validation
void test_validate_ip(void) {
    TEST_ASSERT_TRUE(validate_ip("192.168.1.1"));
    TEST_ASSERT_TRUE(validate_ip("10.0.0.1"));
    TEST_ASSERT_TRUE(validate_ip("172.16.0.1"));
    TEST_ASSERT_TRUE(validate_ip("8.8.8.8"));
    
    TEST_ASSERT_FALSE(validate_ip("256.0.0.1"));
    TEST_ASSERT_FALSE(validate_ip("192.168.1"));
    TEST_ASSERT_FALSE(validate_ip("192.168.1.1.1"));
    TEST_ASSERT_FALSE(validate_ip("192.168.1.a"));
    TEST_ASSERT_FALSE(validate_ip(""));
    TEST_ASSERT_FALSE(validate_ip(NULL));
}

// Test percentage validation
void test_validate_percentage(void) {
    TEST_ASSERT_TRUE(validate_percentage(0.0));
    TEST_ASSERT_TRUE(validate_percentage(50.0));
    TEST_ASSERT_TRUE(validate_percentage(100.0));
    
    TEST_ASSERT_FALSE(validate_percentage(-1.0));
    TEST_ASSERT_FALSE(validate_percentage(101.0));
}

// Test latency validation
void test_validate_latency(void) {
    TEST_ASSERT_TRUE(validate_latency(0.0));
    TEST_ASSERT_TRUE(validate_latency(100.0));
    TEST_ASSERT_TRUE(validate_latency(1000.0));
    
    TEST_ASSERT_FALSE(validate_latency(-1.0));
    TEST_ASSERT_FALSE(validate_latency(10001.0));
}

// Test URL decoding
void test_url_decode(void) {
    char result[100];
    
    url_decode(result, "Hello%20World", sizeof(result));
    TEST_ASSERT_EQUAL_STRING("Hello World", result);
    
    url_decode(result, "Test%2Bplus%26ampersand", sizeof(result));
    TEST_ASSERT_EQUAL_STRING("Test+plus&ampersand", result);
    
    url_decode(result, "%E4%B8%AD%E6%96%87", sizeof(result));
    TEST_ASSERT_EQUAL_STRING("\xE4\xB8\xAD\xE6\x96\x87", result);
    
    url_decode(result, "", sizeof(result));
    TEST_ASSERT_EQUAL_STRING("", result);
}

// Test key-value parsing
void test_parse_key_value(void) {
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
    
    TEST_ASSERT_TRUE(parse_key_value("name=John", key, sizeof(key), value, sizeof(value)));
    TEST_ASSERT_EQUAL_STRING("name", key);
    TEST_ASSERT_EQUAL_STRING("John", value);
    
    TEST_ASSERT_TRUE(parse_key_value("age=30", key, sizeof(key), value, sizeof(value)));
    TEST_ASSERT_EQUAL_STRING("age", key);
    TEST_ASSERT_EQUAL_STRING("30", value);
    
    TEST_ASSERT_FALSE(parse_key_value("invalid", key, sizeof(key), value, sizeof(value)));
    TEST_ASSERT_FALSE(parse_key_value("=empty", key, sizeof(key), value, sizeof(value)));
    TEST_ASSERT_FALSE(parse_key_value("novalue=", key, sizeof(key), value, sizeof(value)));
}

// Test processing valid JSON data
void test_process_json_data_valid(void) {
    HeartbeatData data;
    memset(&data, 0, sizeof(data)); // Initialize to known state
    
    const char *valid_json = "{"
        "\"local_ip\": \"192.168.1.1\","
        "\"public_ip\": \"8.8.8.8\","
        "\"cpu_usage\": 45.5,"
        "\"memory_usage\": 60.2,"
        "\"disk_usage\": 75.0,"
        "\"availability\": 99.9,"
        "\"latency\": 120.5"
    "}";
    
    // Test with valid JSON
    TEST_ASSERT_TRUE(process_json_data(valid_json, &data));
    
    // Verify all fields were parsed correctly
    TEST_ASSERT_NOT_NULL(data.local_ip);
    TEST_ASSERT_NOT_NULL(data.public_ip);
    if (data.local_ip) TEST_ASSERT_EQUAL_STRING("192.168.1.1", data.local_ip);
    if (data.public_ip) TEST_ASSERT_EQUAL_STRING("8.8.8.8", data.public_ip);
    TEST_ASSERT_EQUAL_FLOAT(45.5, data.cpu_usage);
    TEST_ASSERT_EQUAL_FLOAT(60.2, data.memory_usage);
    TEST_ASSERT_EQUAL_FLOAT(75.0, data.disk_usage);
    TEST_ASSERT_EQUAL_FLOAT(99.9, data.availability);
    TEST_ASSERT_EQUAL_FLOAT(120.5, data.latency);
    
    // Test with NULL input
    TEST_ASSERT_FALSE(process_json_data(NULL, &data));
    
    // Test with NULL data pointer
    TEST_ASSERT_FALSE(process_json_data(valid_json, NULL));
}

// Test processing invalid JSON data
void test_process_json_data_invalid(void) {
    HeartbeatData data;
    
    // Invalid JSON format
    const char *invalid_json = "{\"local_ip\": \"192.168.1.1\", invalid}";
    TEST_ASSERT_FALSE(process_json_data(invalid_json, &data));
    
    // Invalid IP address
    const char *invalid_ip_json = "{"
        "\"local_ip\": \"999.999.999.999\","
        "\"public_ip\": \"8.8.8.8\","
        "\"cpu_usage\": 45.5,"
        "\"memory_usage\": 60.2,"
        "\"disk_usage\": 75.0,"
        "\"availability\": 99.9,"
        "\"latency\": 120.5"
    "}";
    TEST_ASSERT_FALSE(process_json_data(invalid_ip_json, &data));
    
    // Invalid percentage value
    const char *invalid_percentage_json = "{"
        "\"local_ip\": \"192.168.1.1\","
        "\"public_ip\": \"8.8.8.8\","
        "\"cpu_usage\": 145.5,"
        "\"memory_usage\": 60.2,"
        "\"disk_usage\": 75.0,"
        "\"availability\": 99.9,"
        "\"latency\": 120.5"
    "}";
    TEST_ASSERT_FALSE(process_json_data(invalid_percentage_json, &data));
}

// Test processing JSON with missing fields
void test_process_json_data_missing_fields(void) {
    HeartbeatData data;
    
    // Missing required fields
    const char *missing_fields_json = "{"
        "\"local_ip\": \"192.168.1.1\","
        "\"public_ip\": \"8.8.8.8\""
    "}";
    TEST_ASSERT_FALSE(process_json_data(missing_fields_json, &data));
}

// Test adding to history
void test_add_to_history(void) {
    HeartbeatData data = {
        .local_ip = "192.168.1.1",
        .public_ip = "8.8.8.8",
        .cpu_usage = 45.5,
        .memory_usage = 60.2,
        .disk_usage = 75.0,
        .availability = 99.9,
        .latency = 120.5
    };
    
    pthread_mutex_lock(&history_mutex);
    int old_count = heartbeat_count;
    add_to_history(&data);
    pthread_mutex_unlock(&history_mutex);
    
    TEST_ASSERT_EQUAL_INT(old_count + 1, heartbeat_count);
    TEST_ASSERT_NOT_NULL(heartbeat_history);
    
    pthread_mutex_lock(&history_mutex);
    TEST_ASSERT_EQUAL_STRING("192.168.1.1", heartbeat_history->data.local_ip);
    TEST_ASSERT_EQUAL_STRING("8.8.8.8", heartbeat_history->data.public_ip);
    pthread_mutex_unlock(&history_mutex);
}

// Test history capacity limit
void test_history_capacity(void) {
    HeartbeatData data = {
        .local_ip = "192.168.1.1",
        .public_ip = "8.8.8.8",
        .cpu_usage = 50.0,
        .memory_usage = 60.0,
        .disk_usage = 70.0,
        .availability = 99.0,
        .latency = 100.0
    };
    
    // Add more than capacity
    for (int i = 0; i < MAX_HISTORY + 2; i++) {
        pthread_mutex_lock(&history_mutex);
        add_to_history(&data);
        pthread_mutex_unlock(&history_mutex);
    }
    
    pthread_mutex_lock(&history_mutex);
    TEST_ASSERT_EQUAL_INT(MAX_HISTORY, heartbeat_count);
    pthread_mutex_unlock(&history_mutex);
}

// Test getting history as JSON
void test_get_history_json(void) {
    HeartbeatData data;
    memset(&data, 0, sizeof(data));
    strncpy(data.local_ip, "192.168.1.1", sizeof(data.local_ip)-1);
    strncpy(data.public_ip, "8.8.8.8", sizeof(data.public_ip)-1);
    data.cpu_usage = 50.0;
    data.memory_usage = 60.0;
    data.disk_usage = 70.0;
    data.availability = 99.0;
    data.latency = 100.0;

    // Add test data with mutex protection
    if (pthread_mutex_lock(&history_mutex) != 0) {
        TEST_FAIL_MESSAGE("Failed to lock mutex");
        return;
    }
    add_to_history(&data);
    if (pthread_mutex_unlock(&history_mutex) != 0) {
        TEST_FAIL_MESSAGE("Failed to unlock mutex");
    }

    // Get JSON and verify
    char *json = get_history_json();
    TEST_ASSERT_NOT_NULL(json);
    if (!json) {
        return;
    }

    // Parse JSON with error checking
    struct json_object *parsed_json = json_tokener_parse(json);
    if (!parsed_json) {
        TEST_FAIL_MESSAGE("Failed to parse JSON");
        free(json);
        return;
    }

    // Verify JSON structure
    if (!json_object_is_type(parsed_json, json_type_array)) {
        TEST_FAIL_MESSAGE("Expected JSON array");
        json_object_put(parsed_json);
        free(json);
        return;
    }

    // Verify array contents
    int array_length = json_object_array_length(parsed_json);
    TEST_ASSERT_EQUAL_INT(1, array_length);

    if (array_length > 0) {
        struct json_object *heartbeat = json_object_array_get_idx(parsed_json, 0);
        TEST_ASSERT_NOT_NULL(heartbeat);
        
        if (heartbeat) {
            struct json_object *field;
            
            // Verify required fields
            TEST_ASSERT_TRUE(json_object_object_get_ex(heartbeat, "local_ip", &field));
            if (field) {
                TEST_ASSERT_EQUAL_STRING("192.168.1.1", json_object_get_string(field));
            }
            
            TEST_ASSERT_TRUE(json_object_object_get_ex(heartbeat, "public_ip", &field));
            if (field) {
                TEST_ASSERT_EQUAL_STRING("8.8.8.8", json_object_get_string(field));
            }
        }
    }

    // Clean up JSON objects
    json_object_put(parsed_json);
    free(json);

    // Clean test data with mutex protection
    if (pthread_mutex_lock(&history_mutex) != 0) {
        TEST_FAIL_MESSAGE("Failed to lock mutex for cleanup");
        return;
    }
    
    HeartbeatNode *current = heartbeat_history;
    while (current != NULL) {
        HeartbeatNode *next = current->next;
        free(current);
        current = next;
    }
    heartbeat_history = NULL;
    heartbeat_count = 0;
    
    if (pthread_mutex_unlock(&history_mutex) != 0) {
        TEST_FAIL_MESSAGE("Failed to unlock mutex after cleanup");
    }
}

// Test empty history
void test_empty_history(void) {
    char *json = get_history_json();
    TEST_ASSERT_NOT_NULL(json);
    
    if (json) {
        struct json_object *parsed_json = json_tokener_parse(json);
        TEST_ASSERT_NOT_NULL(parsed_json);
        
        if (parsed_json) {
            TEST_ASSERT_TRUE(json_object_is_type(parsed_json, json_type_array));
            TEST_ASSERT_EQUAL_INT(0, json_object_array_length(parsed_json));
            json_object_put(parsed_json);
        }
        free(json);
    }
}

#define NUM_TEST_THREADS 5
#define HEARTBEATS_PER_THREAD 20

// Thread worker function for concurrent testing
void *concurrent_add_worker(void *arg) {
    HeartbeatData data = {
        .local_ip = "192.168.1.1",
        .public_ip = "8.8.8.8",
        .cpu_usage = 45.5,
        .memory_usage = 60.2,
        .disk_usage = 75.0,
        .availability = 99.9,
        .latency = 120.5
    };
    
    for (int i = 0; i < HEARTBEATS_PER_THREAD; i++) {
        pthread_mutex_lock(&history_mutex);
        add_to_history(&data);
        pthread_mutex_unlock(&history_mutex);
    }
    return NULL;
}

// Test concurrent history access
void test_concurrent_history_access(void) {
    pthread_t threads[NUM_TEST_THREADS];
    
    // Create threads
    for (int i = 0; i < NUM_TEST_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, concurrent_add_worker, NULL) != 0) {
            TEST_FAIL_MESSAGE("Thread creation failed");
            return;
        }
    }
    
    // Wait for threads to complete
    for (int i = 0; i < NUM_TEST_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Verify results
    pthread_mutex_lock(&history_mutex);
    TEST_ASSERT_EQUAL_INT(NUM_TEST_THREADS * HEARTBEATS_PER_THREAD, heartbeat_count);
    
    // Verify data integrity
    int node_count = 0;
    HeartbeatNode *current = heartbeat_history;
    while (current != NULL) {
        TEST_ASSERT_EQUAL_STRING("192.168.1.1", current->data.local_ip);
        TEST_ASSERT_EQUAL_STRING("8.8.8.8", current->data.public_ip);
        current = current->next;
        node_count++;
    }
    TEST_ASSERT_EQUAL_INT(heartbeat_count, node_count);
    pthread_mutex_unlock(&history_mutex);
}

// Main test runner
int main(void) {
    UNITY_BEGIN();
    
    // Core functionality tests
    RUN_TEST(test_validate_ip);
    RUN_TEST(test_validate_percentage);
    RUN_TEST(test_validate_latency);
    RUN_TEST(test_url_decode);
    RUN_TEST(test_parse_key_value);
    
    // JSON processing tests
    RUN_TEST(test_process_json_data_valid);
    RUN_TEST(test_process_json_data_invalid);
    RUN_TEST(test_process_json_data_missing_fields);
    
    // History management tests
    RUN_TEST(test_add_to_history);
    RUN_TEST(test_history_capacity);
    RUN_TEST(test_get_history_json);
    RUN_TEST(test_empty_history);
    RUN_TEST(test_concurrent_history_access);
    
    return UNITY_END();
}