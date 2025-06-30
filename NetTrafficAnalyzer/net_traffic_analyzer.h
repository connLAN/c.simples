#ifndef NET_TRAFFIC_ANALYZER_H
#define NET_TRAFFIC_ANALYZER_H

#include <stdint.h>
#include <time.h>

#define MAX_RECORDS 100000
#define MAX_IP_STATS 10000
#define SUSPICIOUS_REQUESTS_THRESHOLD 100  // 短时间内的请求阈值
#define SUSPICIOUS_TIME_WINDOW 60         // 监控时间窗口（秒）

typedef struct {
    char ip[16];
    time_t timestamp;
    uint64_t bytes;
} ConnectionRecord;

typedef struct {
    char period[32];
    uint64_t total_bytes;
} TrafficReport;

typedef struct {
    char ip[16];
    uint32_t request_count;      // 总请求次数
    uint32_t window_requests;    // 时间窗口内的请求次数
    time_t first_seen;          // 首次请求时间
    time_t last_seen;           // 最后请求时间
    uint8_t is_suspicious;      // 是否可疑
} IPStats;

typedef struct {
    char ip[16];
    uint32_t request_count;
    time_t first_seen;
    time_t last_seen;
    char reason[64];            // 可疑原因描述
} SuspiciousIP;

// 原有函数
void add_connection(const char* ip, time_t ts, uint64_t bytes);
TrafficReport* generate_hourly_report(time_t ref_ts, size_t* count);
TrafficReport* generate_daily_report(time_t ref_ts, size_t* count);
TrafficReport* sort_by_traffic(TrafficReport* reports, size_t count, int ascending);
void export_csv(const TrafficReport* reports, size_t count, const char* filename);
void free_report(TrafficReport* reports, size_t count);

// 新增IP统计相关函数
int check_ip(const char* ip, time_t ts);  // 返回1表示可疑IP，0表示正常
SuspiciousIP* get_suspicious_ips(size_t* count);  // 获取可疑IP列表
void export_suspicious_ips(const char* filename);  // 导出可疑IP报告
void reset_ip_stats(void);  // 重置IP统计信息

#endif // NET_TRAFFIC_ANALYZER_H