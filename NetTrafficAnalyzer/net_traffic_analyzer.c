#include "net_traffic_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// 全局变量
ConnectionRecord* connections = NULL;
size_t connection_count = 0;
IPStats* ip_stats = NULL;
size_t ip_stats_count = 0;
static char* blacklist[MAX_BLACKLIST_SIZE] = {NULL};
static char* whitelist[MAX_WHITELIST_SIZE] = {NULL};
static size_t blacklist_count = 0;
static size_t whitelist_count = 0;
static AnalyzerConfig current_config = {
    .suspicious_requests_threshold = DEFAULT_SUSPICIOUS_REQUESTS_THRESHOLD,
    .suspicious_time_window = DEFAULT_SUSPICIOUS_TIME_WINDOW,
    .enable_geo_tracking = 1,
    .enable_pattern_analysis = 1,
    .enable_adaptive_threshold = 1
};

// 生成每日报告
TrafficReport* generate_daily_report(time_t ref_ts, size_t* count) {
    struct tm* tm_info = localtime(&ref_ts);
    time_t start_day = ref_ts - (tm_info->tm_hour * 3600 +
                                tm_info->tm_min * 60 +
                                tm_info->tm_sec);

    // 创建30天的报告
    TrafficReport* reports = malloc(30 * sizeof(TrafficReport));
    if (!reports) {
        *count = 0;
        return NULL;
    }

    // 初始化报告
    for (int i = 0; i < 30; i++) {
        time_t day_ts = start_day - i * 86400; // 86400 = 24 * 3600
        struct tm* day_tm = localtime(&day_ts);

        memset(&reports[i], 0, sizeof(TrafficReport));  // 首先将整个结构体清零
        snprintf(reports[i].period, sizeof(reports[i].period),
                "%04d-%02d-%02d",
                day_tm->tm_year + 1900,
                day_tm->tm_mon + 1,
                day_tm->tm_mday);
    }

    // 统计每天的流量数据
    for (size_t i = 0; i < connection_count; i++) {
        struct tm* conn_tm = localtime(&connections[i].timestamp);
        time_t conn_day = connections[i].timestamp -
                         (conn_tm->tm_hour * 3600 +
                          conn_tm->tm_min * 60 +
                          conn_tm->tm_sec);

        int day_diff = (int)((start_day - conn_day) / 86400);
        if (day_diff >= 0 && day_diff < 30) {
            reports[day_diff].total_bytes += connections[i].bytes;
            reports[day_diff].total_connections++;

            // 统计唯一IP
            int is_unique = 1;
            for (size_t j = 0; j < i; j++) {
                struct tm* prev_tm = localtime(&connections[j].timestamp);
                time_t prev_day = connections[j].timestamp -
                                (prev_tm->tm_hour * 3600 +
                                 prev_tm->tm_min * 60 +
                                 prev_tm->tm_sec);

                if (prev_day == conn_day &&
                    strcmp(connections[j].ip, connections[i].ip) == 0) {
                    is_unique = 0;
                    break;
                }
            }

            if (is_unique) {
                reports[day_diff].unique_ips++;

                // 检查是否可疑
                for (size_t j = 0; j < ip_stats_count; j++) {
                    if (strcmp(ip_stats[j].ip, connections[i].ip) == 0 &&
                        ip_stats[j].is_suspicious) {
                        reports[day_diff].suspicious_ips++;
                        break;
                    }
                }
            }
        }
    }

    *count = 30;
    return reports;
}