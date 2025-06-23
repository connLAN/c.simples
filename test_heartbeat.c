#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <json-c/json.h>

// 声明我们要测试的函数
extern void init_heartbeat_history();
extern void add_heartbeat(const char* client_id, const char* timestamp);
extern char* get_history_json();
extern void* handle_client(void* client_socket_ptr);

// 测试设置函数
void setUp(void) {
    // 在每个测试前初始化心跳历史
    init_heartbeat_history();
}

// 测试清理函数
void tearDown(void) {
    // 清理资源（如果需要）
}

// 测试心跳数据添加功能
void test_add_heartbeat(void) {
    const char* client_id = "test_client";
    const char* timestamp = "2023-12-25 10:00:00";
    
    add_heartbeat(client_id, timestamp);
    
    // 获取历史记录并验证
    char* history = get_history_json();
    TEST_ASSERT_NOT_NULL(history);
    
    // 解析 JSON 并验证内容
    struct json_object* parsed_json = json_tokener_parse(history);
    TEST_ASSERT_NOT_NULL(parsed_json);
    
    // 验证是否是数组
    TEST_ASSERT(json_object_is_type(parsed_json, json_type_array));
    
    // 验证数组长度
    TEST_ASSERT_EQUAL_INT(1, json_object_array_length(parsed_json));
    
    // 获取第一个元素
    struct json_object* heartbeat = json_object_array_get_idx(parsed_json, 0);
    TEST_ASSERT_NOT_NULL(heartbeat);
    
    // 验证字段
    struct json_object* client_id_obj;
    struct json_object* timestamp_obj;
    
    TEST_ASSERT(json_object_object_get_ex(heartbeat, "client_id", &client_id_obj));
    TEST_ASSERT(json_object_object_get_ex(heartbeat, "timestamp", &timestamp_obj));
    
    TEST_ASSERT_EQUAL_STRING(client_id, json_object_get_string(client_id_obj));
    TEST_ASSERT_EQUAL_STRING(timestamp, json_object_get_string(timestamp_obj));
    
    // 清理
    json_object_put(parsed_json);
    free(history);
}

// 测试历史记录容量限制
void test_history_capacity(void) {
    const char* client_id = "test_client";
    char timestamp[32];
    
    // 添加超过最大容量的心跳记录
    for (int i = 0; i < 12; i++) {  // 假设最大容量是 10
        snprintf(timestamp, sizeof(timestamp), "2023-12-25 %02d:00:00", i);
        add_heartbeat(client_id, timestamp);
    }
    
    // 获取历史记录并验证
    char* history = get_history_json();
    TEST_ASSERT_NOT_NULL(history);
    
    // 解析 JSON
    struct json_object* parsed_json = json_tokener_parse(history);
    TEST_ASSERT_NOT_NULL(parsed_json);
    
    // 验证数组长度不超过最大容量
    TEST_ASSERT_EQUAL_INT(10, json_object_array_length(parsed_json));
    
    // 验证最新的记录在最前面
    struct json_object* latest = json_object_array_get_idx(parsed_json, 0);
    struct json_object* timestamp_obj;
    TEST_ASSERT(json_object_object_get_ex(latest, "timestamp", &timestamp_obj));
    TEST_ASSERT_EQUAL_STRING("2023-12-25 11:00:00", json_object_get_string(timestamp_obj));
    
    // 清理
    json_object_put(parsed_json);
    free(history);
}

// 测试空历史记录
void test_empty_history(void) {
    // 获取空历史记录
    char* history = get_history_json();
    TEST_ASSERT_NOT_NULL(history);
    
    // 解析 JSON
    struct json_object* parsed_json = json_tokener_parse(history);
    TEST_ASSERT_NOT_NULL(parsed_json);
    
    // 验证是空数组
    TEST_ASSERT(json_object_is_type(parsed_json, json_type_array));
    TEST_ASSERT_EQUAL_INT(0, json_object_array_length(parsed_json));
    
    // 清理
    json_object_put(parsed_json);
    free(history);
}

// 主函数
int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_add_heartbeat);
    RUN_TEST(test_history_capacity);
    RUN_TEST(test_empty_history);
    
    return UNITY_END();
}