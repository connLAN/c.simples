#ifndef NET_TRAFFIC_ANALYZER_H
#define NET_TRAFFIC_ANALYZER_H

#include <stdint.h>
#include <time.h>

#define MAX_RECORDS 100000
#define MAX_IP_STATS 10000
#define DEFAULT_SUSPICIOUS_REQUESTS_THRESHOLD 100  // 默认短时间内的请求阈值
#define DEFAULT_SUSPICIOUS_TIME_WINDOW 60         // 默认监控时间窗口（秒）
#define MAX_PATTERN_LENGTH 64                     // 最大模式长度
#define MAX_BLACKLIST_SIZE 1000                  // 最大黑名单大小
#define MAX_WHITELIST_SIZE 1000                  // 最大白名单大小
#define MAX_COUNTRY_CODE_LENGTH 3                // 国家代码长度
#define MAX_LOCATION_LENGTH 128                  // 位置信息最大长度
#define CONNECTION_HISTORY_SIZE 10               // 每个IP保存的历史连接数

typedef struct {
    char ip[16];
    time_t timestamp;
    uint64_t bytes;
} ConnectionRecord;

typedef struct {
    char period[32];
    uint64_t total_bytes;
    uint32_t total_connections;
    uint32_t unique_ips;
    uint32_t suspicious_ips;
} TrafficReport;

typedef struct {
    time_t timestamp;
    uint32_t request_count;
    uint64_t bytes;
} ConnectionHistory;

typedef struct {
    char ip[16];
    uint32_t request_count;      // 总请求次数
    uint32_t window_requests;    // 时间窗口内的请求次数
    time_t first_seen;          // 首次请求时间
    time_t last_seen;           // 最后请求时间
    uint8_t is_suspicious;      // 是否可疑
    char country_code[MAX_COUNTRY_CODE_LENGTH];  // 国家代码
    char location[MAX_LOCATION_LENGTH];          // 地理位置信息
    double avg_request_interval; // 平均请求间隔
    uint32_t burst_count;       // 突发请求次数
    ConnectionHistory history[CONNECTION_HISTORY_SIZE]; // 连接历史
    char connection_pattern[MAX_PATTERN_LENGTH]; // 连接模式描述
    uint32_t adaptive_threshold; // 自适应阈值
} IPStats;

typedef struct {
    char ip[16];
    uint32_t request_count;
    time_t first_seen;
    time_t last_seen;
    char reason[128];           // 增加可疑原因描述长度
    char country_code[MAX_COUNTRY_CODE_LENGTH];
    char location[MAX_LOCATION_LENGTH];
    char connection_pattern[MAX_PATTERN_LENGTH];
} SuspiciousIP;

// 配置结构
typedef struct {
    uint32_t suspicious_requests_threshold;
    uint32_t suspicious_time_window;
    uint8_t enable_geo_tracking;
    uint8_t enable_pattern_analysis;
    uint8_t enable_adaptive_threshold;
    char blacklist_file[256];
    char whitelist_file[256];
    char database_file[256];
} AnalyzerConfig;

// 全局变量声明
extern ConnectionRecord* connections;
extern size_t connection_count;
extern IPStats* ip_stats;
extern size_t ip_stats_count;

// 原有函数
void add_connection(const char* ip, time_t ts, uint64_t bytes);
TrafficReport* generate_hourly_report(time_t ref_ts, size_t* count);
TrafficReport* generate_daily_report(time_t ref_ts, size_t* count);
TrafficReport* sort_by_traffic(TrafficReport* reports, size_t count, int ascending);
void export_csv(const TrafficReport* reports, size_t count, const char* filename);
void free_report(TrafficReport* reports, size_t count);

// IP统计相关函数
int check_ip(const char* ip, time_t ts);  // 返回1表示可疑IP，0表示正常
SuspiciousIP* get_suspicious_ips(size_t* count);  // 获取可疑IP列表
void export_suspicious_ips(const char* filename);  // 导出可疑IP报告
void reset_ip_stats(void);  // 重置IP统计信息

// 新增函数
// 配置管理
void init_analyzer(const AnalyzerConfig* config);
void update_config(const AnalyzerConfig* config);
void save_config(const char* filename);
void load_config(const char* filename);

// IP地理位置跟踪
int update_ip_location(const char* ip, const char* country_code, const char* location);
const char* get_ip_location(const char* ip);

// 黑白名单管理
int add_to_blacklist(const char* ip);
int add_to_whitelist(const char* ip);
int remove_from_blacklist(const char* ip);
int remove_from_whitelist(const char* ip);
int is_blacklisted(const char* ip);
int is_whitelisted(const char* ip);
void load_blacklist(const char* filename);
void load_whitelist(const char* filename);
void save_blacklist(const char* filename);
void save_whitelist(const char* filename);

// 连接模式分析
void analyze_connection_pattern(const char* ip);
const char* get_connection_pattern(const char* ip);

// 自适应阈值管理
void update_adaptive_threshold(const char* ip);
uint32_t get_adaptive_threshold(const char* ip);

// 数据持久化
void save_ip_stats(const char* filename);
void load_ip_stats(const char* filename);

// 高级分析功能
void detect_port_scan(const char* ip, uint16_t port);
void detect_ddos_attempt(const char* ip);
void analyze_traffic_pattern(const char* ip);

// 清理和维护
void cleanup_old_records(time_t cutoff_time);
void optimize_memory_usage(void);

#endif // NET_TRAFFIC_ANALYZER_H