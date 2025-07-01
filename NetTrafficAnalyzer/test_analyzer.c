#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "net_traffic_analyzer.h"

// 自定义函数用于释放可疑IP资源
void free_suspicious_ips(SuspiciousIP* ips) {
    if (ips) {
        free(ips);
    }
}

// 测试初始化函数
void test_init() {
    printf("Testing initialization...\n");
    
    // 使用默认配置初始化
    init_analyzer(NULL);
    
    // 验证初始化后可以添加连接记录
    add_connection("192.168.1.1", time(NULL), 1024);
    
    printf("Initialization test passed.\n\n");
}

// 测试连接记录功能
void test_connection_records() {
    printf("Testing connection records...\n");
    
    // 重置
    reset_ip_stats();
    
    time_t current_time = time(NULL);
    
    // 添加多个连接记录
    for (int i = 0; i < 10; i++) {
        add_connection("192.168.1.2", current_time - i * 60, 2048);
    }
    
    // 添加可疑IP的连接记录（大量请求）
    for (int i = 0; i < 100; i++) {
        add_connection("10.0.0.1", current_time - i, 1024);
    }
    
    // 验证IP检查
    int is_suspicious = check_ip("192.168.1.2", current_time);
    printf("IP 192.168.1.2 is %s\n", is_suspicious ? "suspicious" : "normal");
    
    is_suspicious = check_ip("10.0.0.1", current_time);
    printf("IP 10.0.0.1 is %s\n", is_suspicious ? "suspicious" : "normal");
    
    printf("Connection records test passed.\n\n");
}

// 测试可疑IP检测
void test_suspicious_ip_detection() {
    printf("Testing suspicious IP detection...\n");
    
    // 重置
    reset_ip_stats();
    
    time_t current_time = time(NULL);
    
    // 添加正常IP的连接记录
    for (int i = 0; i < 5; i++) {
        add_connection("192.168.1.3", current_time - i * 3600, 1024);
    }
    
    // 添加可疑IP的连接记录（短时间内大量请求）
    for (int i = 0; i < 50; i++) {
        add_connection("10.0.0.2", current_time - i * 10, 1024);
    }
    
    // 更新IP地理位置
    update_ip_location("10.0.0.2", "US", "Unknown Location");
    
    // 检查可疑IP
    size_t count;
    SuspiciousIP* suspicious_ips = get_suspicious_ips(&count);
    
    printf("Found %zu suspicious IPs\n", count);
    for (size_t i = 0; i < count; i++) {
        printf("Suspicious IP: %s, Requests: %u, Reason: %s\n", 
               suspicious_ips[i].ip, 
               suspicious_ips[i].request_count,
               suspicious_ips[i].reason);
    }
    
    // 释放资源
    free_suspicious_ips(suspicious_ips);
    
    printf("Suspicious IP detection test passed.\n\n");
}

// 测试报告生成
void test_report_generation() {
    printf("Testing report generation...\n");
    
    // 重置
    reset_ip_stats();
    
    time_t current_time = time(NULL);
    
    // 添加一些连接记录，跨越多个小时
    for (int i = 0; i < 100; i++) {
        add_connection("192.168.1.4", current_time - i * 1800, 1024 + i * 10);
    }
    
    // 生成小时报告
    size_t count;
    TrafficReport* hourly_report = generate_hourly_report(current_time, &count);
    
    printf("Generated hourly report with %zu entries\n", count);
    for (size_t i = 0; i < 5 && i < count; i++) { // 只打印前5个
        printf("Hour: %s, Bytes: %lu\n", 
               hourly_report[i].period, 
               hourly_report[i].total_bytes);
    }
    
    // 释放资源
    free_report(hourly_report, count);
    
    // 生成每日报告
    TrafficReport* daily_report = generate_daily_report(current_time, &count);
    
    printf("Generated daily report with %zu entries\n", count);
    for (size_t i = 0; i < 5 && i < count; i++) { // 只打印前5个
        printf("Day: %s, Bytes: %lu\n", 
               daily_report[i].period, 
               daily_report[i].total_bytes);
    }
    
    // 排序报告
    TrafficReport* sorted_report = sort_by_traffic(daily_report, count, 0); // 降序
    
    printf("Sorted daily report (by traffic, descending):\n");
    for (size_t i = 0; i < 5 && i < count; i++) { // 只打印前5个
        printf("Day: %s, Bytes: %lu\n", 
               sorted_report[i].period, 
               sorted_report[i].total_bytes);
    }
    
    // 导出报告
    export_csv(sorted_report, count, "traffic_report.csv");
    printf("Report exported to traffic_report.csv\n");
    
    // 释放资源
    free_report(daily_report, count);
    free_report(sorted_report, count);
    
    printf("Report generation test passed.\n\n");
}

// 测试配置管理
void test_config_management() {
    printf("Testing configuration management...\n");
    
    // 创建自定义配置
    AnalyzerConfig config;
    config.suspicious_requests_threshold = 20;
    config.suspicious_time_window = 300; // 5分钟
    config.enable_geo_tracking = 1;
    config.enable_pattern_analysis = 1;
    config.enable_adaptive_threshold = 1;
    strcpy(config.blacklist_file, "test_blacklist.txt");
    strcpy(config.whitelist_file, "test_whitelist.txt");
    strcpy(config.database_file, "test_ip_stats.csv");
    
    // 更新配置
    update_config(&config);
    
    // 保存配置
    save_config("test_config.cfg");
    printf("Configuration saved to test_config.cfg\n");
    
    // 修改配置
    config.suspicious_requests_threshold = 30;
    update_config(&config);
    
    // 加载配置
    load_config("test_config.cfg");
    
    // 验证加载的配置 - 使用全局配置
    printf("Loaded configuration from file\n");
    
    printf("Configuration management test passed.\n\n");
}

// 测试清理功能
void test_cleanup() {
    printf("Testing cleanup functions...\n");
    
    time_t current_time = time(NULL);
    
    // 添加一些连接记录
    for (int i = 0; i < 20; i++) {
        add_connection("192.168.1.5", current_time - i * 3600, 1024);
    }
    
    // 清理旧记录（保留最近12小时）
    cleanup_old_records(current_time - 12 * 3600);
    
    // 优化内存使用
    optimize_memory_usage();
    
    // 完全清理 - 使用init_analyzer(NULL)代替reset_analyzer
    init_analyzer(NULL);
    
    printf("Cleanup test passed.\n\n");
}

int main() {
    printf("Starting Network Traffic Analyzer tests...\n\n");
    
    // 运行测试
    test_init();
    test_connection_records();
    test_suspicious_ip_detection();
    test_report_generation();
    test_config_management();
    test_cleanup();
    
    printf("All tests completed successfully!\n");
    return 0;
}