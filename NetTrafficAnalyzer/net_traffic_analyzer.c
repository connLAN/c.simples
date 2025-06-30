#include "net_traffic_analyzer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static ConnectionRecord *connections = NULL;
static size_t connections_size = 0;
static size_t connections_capacity = 0;

// IP统计数据
static IPStats *ip_stats = NULL;
static size_t ip_stats_size = 0;
static size_t ip_stats_capacity = 0;

// 查找或创建IP统计记录
static IPStats* find_or_create_ip_stats(const char* ip) {
    // 查找现有记录
    for (size_t i = 0; i < ip_stats_size; i++) {
        if (strcmp(ip_stats[i].ip, ip) == 0) {
            return &ip_stats[i];
        }
    }

    // 如果达到最大容量，返回NULL
    if (ip_stats_size >= MAX_IP_STATS) {
        return NULL;
    }

    // 需要扩展数组
    if (ip_stats_size >= ip_stats_capacity) {
        size_t new_capacity = ip_stats_capacity ? ip_stats_capacity * 2 : 16;
        if (new_capacity > MAX_IP_STATS) new_capacity = MAX_IP_STATS;
        
        IPStats *new_stats = realloc(ip_stats, new_capacity * sizeof(IPStats));
        if (!new_stats) return NULL;
        
        ip_stats = new_stats;
        ip_stats_capacity = new_capacity;
    }

    // 初始化新记录
    IPStats *stats = &ip_stats[ip_stats_size++];
    memset(stats, 0, sizeof(IPStats));
    strncpy(stats->ip, ip, sizeof(stats->ip) - 1);
    return stats;
}

// 检查IP是否可疑
int check_ip(const char* ip, time_t ts) {
    IPStats *stats = find_or_create_ip_stats(ip);
    if (!stats) return 1; // 如果无法创建统计记录，视为可疑

    // 更新统计信息
    if (stats->first_seen == 0) {
        stats->first_seen = ts;
    }
    stats->last_seen = ts;
    stats->request_count++;

    // 更新时间窗口内的请求计数
    time_t window_start = ts - SUSPICIOUS_TIME_WINDOW;
    if (stats->last_seen > window_start) {
        stats->window_requests++;
    } else {
        stats->window_requests = 1;
    }

    // 检查是否可疑
    if (stats->window_requests >= SUSPICIOUS_REQUESTS_THRESHOLD) {
        stats->is_suspicious = 1;
        return 1;
    }

    return 0;
}

// 获取可疑IP列表
SuspiciousIP* get_suspicious_ips(size_t* count) {
    *count = 0;
    size_t capacity = 16;
    SuspiciousIP* suspicious = malloc(capacity * sizeof(SuspiciousIP));
    if (!suspicious) return NULL;

    for (size_t i = 0; i < ip_stats_size; i++) {
        if (ip_stats[i].is_suspicious) {
            if (*count >= capacity) {
                capacity *= 2;
                SuspiciousIP* new_suspicious = realloc(suspicious, capacity * sizeof(SuspiciousIP));
                if (!new_suspicious) {
                    free(suspicious);
                    return NULL;
                }
                suspicious = new_suspicious;
            }

            SuspiciousIP* sus = &suspicious[*count];
            strncpy(sus->ip, ip_stats[i].ip, sizeof(sus->ip) - 1);
            sus->ip[sizeof(sus->ip) - 1] = '\0'; /* 确保字符串以null结尾 */
            sus->request_count = ip_stats[i].request_count;
            sus->first_seen = ip_stats[i].first_seen;
            sus->last_seen = ip_stats[i].last_seen;
            snprintf(sus->reason, sizeof(sus->reason), 
                    "High frequency: %u requests in %d seconds", 
                    ip_stats[i].window_requests, 
                    SUSPICIOUS_TIME_WINDOW);
            (*count)++;
        }
    }

    return suspicious;
}

// 导出可疑IP报告
void export_suspicious_ips(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        perror("Error opening suspicious IPs file");
        return;
    }

    // 写入CSV表头
    fprintf(file, "IP,Requests,First Seen,Last Seen,Reason\n");

    // 获取可疑IP列表
    size_t count;
    SuspiciousIP* suspicious = get_suspicious_ips(&count);
    if (!suspicious) {
        fclose(file);
        return;
    }

    // 写入每条记录
    char first_seen[32], last_seen[32];
    for (size_t i = 0; i < count; i++) {
        strftime(first_seen, sizeof(first_seen), "%Y-%m-%d %H:%M:%S", 
                localtime(&suspicious[i].first_seen));
        strftime(last_seen, sizeof(last_seen), "%Y-%m-%d %H:%M:%S", 
                localtime(&suspicious[i].last_seen));
        
        fprintf(file, "%s,%u,%s,%s,\"%s\"\n",
                suspicious[i].ip,
                suspicious[i].request_count,
                first_seen,
                last_seen,
                suspicious[i].reason);
    }

    free(suspicious);
    fclose(file);
}

// 重置IP统计信息
void reset_ip_stats(void) {
    if (ip_stats) {
        free(ip_stats);
        ip_stats = NULL;
    }
    ip_stats_size = 0;
    ip_stats_capacity = 0;
}

void add_connection(const char* ip, time_t ts, uint64_t bytes) {
    if (connections_size >= MAX_RECORDS) return;

    if (connections_size >= connections_capacity) {
        size_t new_capacity = connections_capacity ? connections_capacity * 2 : 1;
        if (new_capacity > MAX_RECORDS) new_capacity = MAX_RECORDS;
        
        ConnectionRecord *new_conn = realloc(connections, new_capacity * sizeof(ConnectionRecord));
        if (!new_conn) return;
        
        connections = new_conn;
        connections_capacity = new_capacity;
    }

    ConnectionRecord *r = &connections[connections_size++];
    strncpy(r->ip, ip, sizeof(r->ip) - 1);
    r->ip[sizeof(r->ip) - 1] = '\0'; /* 确保字符串以null结尾 */
    r->timestamp = ts;
    r->bytes = bytes;
    
    // 检查IP是否可疑并更新统计信息
    int is_suspicious = check_ip(ip, ts);
    if (is_suspicious) {
        printf("警告: 可疑IP检测到: %s\n", ip);
    }
}

TrafficReport* generate_hourly_report(time_t ref_ts, size_t* count) {
    (void)ref_ts; /* 标记参数为有意未使用 */
    TrafficReport *reports = NULL;
    *count = 0;

    for (size_t i = 0; i < connections_size; i++) {
        time_t hour = (connections[i].timestamp / 3600) * 3600;
        char period[32];
        strftime(period, sizeof(period), "%Y-%m-%d %H:00", gmtime(&hour));

        // ... grouping logic ...
    }

    return reports;
}

// 比较函数用于qsort
static int compare_traffic_reports(const void *a, const void *b, int ascending) {
    const TrafficReport *ra = (const TrafficReport*)a;
    const TrafficReport *rb = (const TrafficReport*)b;
    
    // 使用条件运算符处理大数值比较，避免整数溢出
    if (ascending) {
        return (ra->total_bytes > rb->total_bytes) ? 1 : 
               (ra->total_bytes < rb->total_bytes) ? -1 : 0;
    } else {
        return (rb->total_bytes > ra->total_bytes) ? 1 : 
               (rb->total_bytes < ra->total_bytes) ? -1 : 0;
    }
}

// 升序比较函数
static int compare_ascending(const void *a, const void *b) {
    return compare_traffic_reports(a, b, 1);
}

// 降序比较函数
static int compare_descending(const void *a, const void *b) {
    return compare_traffic_reports(a, b, 0);
}

TrafficReport* sort_by_traffic(TrafficReport* reports, size_t count, int ascending) {
    qsort(reports, count, sizeof(TrafficReport), 
          ascending ? compare_ascending : compare_descending);
    return reports;
}

// 生成每日报告
TrafficReport* generate_daily_report(time_t ref_ts, size_t* count) {
    // 初始化返回值
    *count = 0;
    TrafficReport* reports = malloc(sizeof(TrafficReport));
    if (!reports) return NULL;

    // 如果没有连接记录，返回空报告
    if (connections_size == 0) {
        return reports;
    }

    // 获取参考日期的开始时间
    struct tm* timeinfo = localtime(&ref_ts);
    timeinfo->tm_hour = 0;
    timeinfo->tm_min = 0;
    timeinfo->tm_sec = 0;
    time_t day_start = mktime(timeinfo);

    // 统计当天的总流量
    uint64_t total_bytes = 0;
    for (size_t i = 0; i < connections_size; i++) {
        if (connections[i].timestamp >= day_start && 
            connections[i].timestamp < day_start + 24*60*60) {
            total_bytes += connections[i].bytes;
        }
    }

    // 创建报告
    strftime(reports[0].period, sizeof(reports[0].period), "%Y-%m-%d", timeinfo);
    reports[0].total_bytes = total_bytes;
    *count = 1;

    return reports;
}

// 导出报告为CSV文件
void export_csv(const TrafficReport* reports, size_t count, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        perror("Error opening file");
        return;
    }

    // 写入CSV表头
    const char *header1 = "Period";
    const char *header2 = "Total Bytes";
    if (fputs(header1, file) < 0 || 
        fputc(',', file) < 0 || 
        fputs(header2, file) < 0 || 
        fputc('\n', file) < 0) {
        perror("Error writing header");
        fclose(file);
        return;
    }

    // 写入每条记录
    char buffer[256];
    for (size_t i = 0; i < count; i++) {
        snprintf(buffer, sizeof(buffer), "%s,%lu\n", 
                reports[i].period, reports[i].total_bytes);
        if (fputs(buffer, file) < 0) {
            perror("Error writing record");
            fclose(file);
            return;
        }
    }

    fclose(file);
}

// 释放报告内存
void free_report(TrafficReport* reports, size_t count) {
    (void)count; /* 标记参数为有意未使用 */
    if (reports) {
        free(reports);
    }
}