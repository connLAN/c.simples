#include "net_traffic_analyzer.h"
#include <stdio.h>
#include <time.h>
#include <unistd.h> // 用于sleep函数

int main() {
    // 添加一个测试连接记录
    time_t future_time = 1751328000; // 2025-06-30 12:00:00 UTC
    add_connection("192.168.1.1", future_time, 1500);

    // 生成每日报告
    size_t report_count;
    TrafficReport* reports = generate_daily_report(future_time, &report_count);
    if (!reports) {
        printf("Failed to generate report\n");
        return 1;
    }

    // 导出为CSV
    export_csv(reports, report_count, "report.csv");

    // 释放内存
    free_report(reports, report_count);

    printf("\n--- 测试可疑IP检测功能 ---\n\n");

    // 重置IP统计信息，开始新的测试
    reset_ip_stats();
    
    // 添加正常IP的连接记录
    time_t current_time = time(NULL);
    printf("添加正常IP (10.0.0.1) 的连接记录...\n");
    for (int i = 0; i < 10; i++) {
        add_connection("10.0.0.1", current_time + i, 1000 + i);
    }
    
    // 添加可疑IP的连接记录（高频请求）
    printf("添加可疑IP (192.168.0.100) 的连接记录（高频请求）...\n");
    for (int i = 0; i < SUSPICIOUS_REQUESTS_THRESHOLD + 10; i++) {
        add_connection("192.168.0.100", current_time + i % 5, 500 + i);
    }
    
    // 添加另一个可疑IP
    printf("添加另一个可疑IP (172.16.0.50) 的连接记录...\n");
    for (int i = 0; i < SUSPICIOUS_REQUESTS_THRESHOLD + 5; i++) {
        add_connection("172.16.0.50", current_time + i % 3, 300 + i);
    }
    
    // 导出可疑IP报告
    printf("导出可疑IP报告...\n");
    export_suspicious_ips("suspicious_ips.csv");
    
    printf("\n可疑IP报告已导出到 suspicious_ips.csv\n");
    printf("可以使用以下命令查看报告：\n");
    printf("cat suspicious_ips.csv\n");
    
    return 0;
}