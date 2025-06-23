#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "unity.h"
#include "heartbeat_server.h"

// 注意：实际的函数实现在 heartbeat_server_test_impl.c 中

void setUp(void) {
    // 在每个测试前运行
    heartbeat_history = NULL;
    heartbeat_count = 0;
}

void tearDown(void) {
    // 在每个测试后运行
    // 清理历史数据链表
    HeartbeatNode *current = heartbeat_history;
    while (current != NULL) {
        HeartbeatNode *next = current->next;
        free(current);
        current = next;
    }
    heartbeat_history = NULL;
    heartbeat_count = 0;
}

// 测试 IP 地址验证
void test_validate_ip(void) {
    TEST_ASSERT_TRUE(validate_ip("192.168.1.1"));
    TEST_ASSERT_TRUE(validate_ip("10.0.0.1"));
    TEST_ASSERT_TRUE(validate_ip("172.16.254.1"));
    TEST_ASSERT_FALSE(validate_ip("256.1.2.3"));
    TEST_ASSERT_FALSE(validate_ip("1.2.3.256"));
    TEST_ASSERT_FALSE(validate_ip("192.168.1"));
    TEST_ASSERT_FALSE(validate_ip("192.168.1.1.1"));
    TEST_ASSERT_FALSE(validate_ip(""));
    TEST_ASSERT_FALSE(validate_ip("abc.def.ghi.jkl"));
}

// 测试百分比值验证
void test_validate_percentage(void) {
    TEST_ASSERT_TRUE(validate_percentage(0.0));
    TEST_ASSERT_TRUE(validate_percentage(50.0));
    TEST_ASSERT_TRUE(validate_percentage(100.0));
    TEST_ASSERT_FALSE(validate_percentage(-1.0));
    TEST_ASSERT_FALSE(validate_percentage(100.1));
    TEST_ASSERT_FALSE(validate_percentage(200.0));
}

// 测试延迟值验证
void test_validate_latency(void) {
    TEST_ASSERT_TRUE(validate_latency(0.0));
    TEST_ASSERT_TRUE(validate_latency(100.0));
    TEST_ASSERT_TRUE(validate_latency(1000.0));
    TEST_ASSERT_FALSE(validate_latency(-1.0));
}

// 测试 URL 解码
void test_url_decode(void) {
    char result[100];
    
    url_decode(result, "Hello+World", sizeof(result));
    TEST_ASSERT_EQUAL_STRING("Hello World", result);
    
    url_decode(result, "Hello%20World", sizeof(result));
    TEST_ASSERT_EQUAL_STRING("Hello World", result);
    
    url_decode(result, "Test%21%40%23%24", sizeof(result));
    TEST_ASSERT_EQUAL_STRING("Test!@#$", result);
    
    url_decode(result, "", sizeof(result));
    TEST_ASSERT_EQUAL_STRING("", result);
}

// 测试键值对解析
void test_parse_key_value(void) {
    char key[64];
    char value[128];
    
    TEST_ASSERT_TRUE(parse_key_value("name=John+Doe", key, sizeof(key), value, sizeof(value)));
    TEST_ASSERT_EQUAL_STRING("name", key);
    TEST_ASSERT_EQUAL_STRING("John Doe", value);
    
    TEST_ASSERT_TRUE(parse_key_value("key=value", key, sizeof(key), value, sizeof(value)));
    TEST_ASSERT_EQUAL_STRING("key", key);
    TEST_ASSERT_EQUAL_STRING("value", value);
    
    TEST_ASSERT_FALSE(parse_key_value("invalid", key, sizeof(key), value, sizeof(value)));
    TEST_ASSERT_FALSE(parse_key_value("=empty", key, sizeof(key), value, sizeof(value)));
    TEST_ASSERT_FALSE(parse_key_value("novalue=", key, sizeof(key), value, sizeof(value)));
}

// 测试 JSON 数据处理
void test_process_json_data(void) {
    HeartbeatData hb_data;
    const char *valid_json = "{"
        "\"local_ip\":\"192.168.1.100\","
        "\"public_ip\":\"203.0.113.10\","
        "\"cpu_usage\":25.5,"
        "\"memory_usage\":40.2,"
        "\"disk_usage\":65.8,"
        "\"availability\":99.9,"
        "\"latency\":15.3"
    "}";
    
    TEST_ASSERT_TRUE(process_json_data(valid_json, &hb_data));
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", hb_data.local_ip);
    TEST_ASSERT_EQUAL_STRING("203.0.113.10", hb_data.public_ip);
    TEST_ASSERT_EQUAL_FLOAT(25.5, hb_data.cpu_usage);
    TEST_ASSERT_EQUAL_FLOAT(40.2, hb_data.memory_usage);
    TEST_ASSERT_EQUAL_FLOAT(65.8, hb_data.disk_usage);
    TEST_ASSERT_EQUAL_FLOAT(99.9, hb_data.availability);
    TEST_ASSERT_EQUAL_FLOAT(15.3, hb_data.latency);
    
    // 测试无效 JSON
    const char *invalid_json = "{"
        "\"local_ip\":\"192.168.1.100\""  // 缺少必需字段
    "}";
    TEST_ASSERT_FALSE(process_json_data(invalid_json, &hb_data));
    
    // 测试无效值
    const char *invalid_values_json = "{"
        "\"local_ip\":\"256.256.256.256\","  // 无效 IP
        "\"public_ip\":\"203.0.113.10\","
        "\"cpu_usage\":125.5,"               // 无效百分比
        "\"memory_usage\":40.2,"
        "\"disk_usage\":65.8,"
        "\"availability\":99.9,"
        "\"latency\":15.3"
    "}";
    TEST_ASSERT_FALSE(process_json_data(invalid_values_json, &hb_data));
}

// 测试历史数据管理
void test_history_management(void) {
    HeartbeatData hb_data = {
        .local_ip = "192.168.1.100",
        .public_ip = "203.0.113.10",
        .cpu_usage = 25.5,
        .memory_usage = 40.2,
        .disk_usage = 65.8,
        .availability = 99.9,
        .latency = 15.3
    };
    
    // 测试添加数据
    add_to_history(&hb_data);
    TEST_ASSERT_NOT_NULL(heartbeat_history);
    TEST_ASSERT_EQUAL_INT(1, heartbeat_count);
    
    // 验证添加的数据
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", heartbeat_history->data.local_ip);
    TEST_ASSERT_EQUAL_STRING("203.0.113.10", heartbeat_history->data.public_ip);
    TEST_ASSERT_EQUAL_FLOAT(25.5, heartbeat_history->data.cpu_usage);
    
    // 测试历史数据限制
    for (int i = 0; i < MAX_HEARTBEATS + 10; i++) {
        add_to_history(&hb_data);
    }
    TEST_ASSERT_EQUAL_INT(MAX_HEARTBEATS, heartbeat_count);
    
    // 测试获取历史数据的 JSON
    char *history_json = get_history_json();
    TEST_ASSERT_NOT_NULL(history_json);
    TEST_ASSERT_TRUE(strstr(history_json, "192.168.1.100") != NULL);
    TEST_ASSERT_TRUE(strstr(history_json, "203.0.113.10") != NULL);
    free(history_json);
}

// 主测试函数
int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_validate_ip);
    RUN_TEST(test_validate_percentage);
    RUN_TEST(test_validate_latency);
    RUN_TEST(test_url_decode);
    RUN_TEST(test_parse_key_value);
    RUN_TEST(test_process_json_data);
    RUN_TEST(test_history_management);
    
    return UNITY_END();
}