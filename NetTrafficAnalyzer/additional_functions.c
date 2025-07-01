// 清理和维护
void cleanup_old_records(time_t cutoff_time) {
    size_t new_count = 0;
    
    // 清理连接记录
    for (size_t i = 0; i < connection_count; i++) {
        if (connections[i].timestamp > cutoff_time) {
            if (i != new_count) {
                connections[new_count] = connections[i];
            }
            new_count++;
        }
    }
    connection_count = new_count;
    
    // 清理IP统计信息
    new_count = 0;
    for (size_t i = 0; i < ip_stats_count; i++) {
        if (ip_stats[i].last_seen > cutoff_time) {
            if (i != new_count) {
                ip_stats[new_count] = ip_stats[i];
            }
            new_count++;
        }
    }
    ip_stats_count = new_count;
}

void optimize_memory_usage(void) {
    // 如果使用量低于容量的25%，则收缩数组
    if (connection_count < MAX_RECORDS / 4) {
        size_t new_size = MAX_RECORDS / 2;
        ConnectionRecord* new_connections = realloc(connections, 
                                                  new_size * sizeof(ConnectionRecord));
        if (new_connections) {
            connections = new_connections;
        }
    }
    
    if (ip_stats_count < MAX_IP_STATS / 4) {
        size_t new_size = MAX_IP_STATS / 2;
        IPStats* new_stats = realloc(ip_stats, new_size * sizeof(IPStats));
        if (new_stats) {
            ip_stats = new_stats;
        }
    }
}

// 导出可疑IP报告
void export_suspicious_ips(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) return;
    
    fprintf(file, "IP,RequestCount,FirstSeen,LastSeen,Pattern,Location\n");
    
    for (size_t i = 0; i < ip_stats_count; i++) {
        if (ip_stats[i].is_suspicious) {
            char first_seen_str[64] = {0};
            char last_seen_str[64] = {0};
            
            struct tm* tm_info = localtime(&ip_stats[i].first_seen);
            strftime(first_seen_str, sizeof(first_seen_str), "%Y-%m-%d %H:%M:%S", tm_info);
            
            tm_info = localtime(&ip_stats[i].last_seen);
            strftime(last_seen_str, sizeof(last_seen_str), "%Y-%m-%d %H:%M:%S", tm_info);
            
            fprintf(file, "%s,%u,%s,%s,%s,%s %s\n",
                   ip_stats[i].ip,
                   ip_stats[i].request_count,
                   first_seen_str,
                   last_seen_str,
                   ip_stats[i].connection_pattern,
                   ip_stats[i].country_code,
                   ip_stats[i].location);
        }
    }
    
    fclose(file);
}

SuspiciousIP* get_suspicious_ips(size_t* count) {
    // 计算可疑IP数量
    size_t suspicious_count = 0;
    for (size_t i = 0; i < ip_stats_count; i++) {
        if (ip_stats[i].is_suspicious) {
            suspicious_count++;
        }
    }
    
    if (suspicious_count == 0) {
        *count = 0;
        return NULL;
    }
    
    // 分配内存
    SuspiciousIP* result = malloc(suspicious_count * sizeof(SuspiciousIP));
    if (!result) {
        *count = 0;
        return NULL;
    }
    
    // 填充数据
    size_t index = 0;
    for (size_t i = 0; i < ip_stats_count; i++) {
        if (ip_stats[i].is_suspicious) {
            strncpy(result[index].ip, ip_stats[i].ip, sizeof(result[index].ip) - 1);
            result[index].request_count = ip_stats[i].request_count;
            result[index].first_seen = ip_stats[i].first_seen;
            result[index].last_seen = ip_stats[i].last_seen;
            
            snprintf(result[index].reason, sizeof(result[index].reason),
                    "Requests: %u, Threshold: %u, Pattern: %s",
                    ip_stats[i].window_requests,
                    ip_stats[i].adaptive_threshold,
                    ip_stats[i].connection_pattern);
            
            strncpy(result[index].country_code, ip_stats[i].country_code, 
                   sizeof(result[index].country_code) - 1);
            strncpy(result[index].location, ip_stats[i].location, 
                   sizeof(result[index].location) - 1);
            strncpy(result[index].connection_pattern, ip_stats[i].connection_pattern, 
                   sizeof(result[index].connection_pattern) - 1);
            
            index++;
        }
    }
    
    *count = suspicious_count;
    return result;
}

// 配置管理
void init_analyzer(const AnalyzerConfig* config) {
    if (config) {
        memcpy(&current_config, config, sizeof(AnalyzerConfig));
    } else {
        // 使用默认配置
        current_config.suspicious_requests_threshold = DEFAULT_SUSPICIOUS_REQUESTS_THRESHOLD;
        current_config.suspicious_time_window = DEFAULT_SUSPICIOUS_TIME_WINDOW;
        current_config.enable_geo_tracking = 1;
        current_config.enable_pattern_analysis = 1;
        current_config.enable_adaptive_threshold = 1;
        strcpy(current_config.blacklist_file, "blacklist.txt");
        strcpy(current_config.whitelist_file, "whitelist.txt");
        strcpy(current_config.database_file, "ip_stats.csv");
    }
    
    // 初始化内存
    if (!connections) {
        connections = malloc(sizeof(ConnectionRecord) * MAX_RECORDS);
    }
    
    if (!ip_stats) {
        ip_stats = malloc(sizeof(IPStats) * MAX_IP_STATS);
    }
    
    // 加载黑白名单
    if (strlen(current_config.blacklist_file) > 0) {
        load_blacklist(current_config.blacklist_file);
    }
    
    if (strlen(current_config.whitelist_file) > 0) {
        load_whitelist(current_config.whitelist_file);
    }
    
    // 加载IP统计数据
    if (strlen(current_config.database_file) > 0) {
        load_ip_stats(current_config.database_file);
    }
}

void update_config(const AnalyzerConfig* config) {
    if (!config) return;
    
    memcpy(&current_config, config, sizeof(AnalyzerConfig));
}

void save_config(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) return;
    
    fprintf(file, "suspicious_requests_threshold=%u\n", current_config.suspicious_requests_threshold);
    fprintf(file, "suspicious_time_window=%u\n", current_config.suspicious_time_window);
    fprintf(file, "enable_geo_tracking=%u\n", current_config.enable_geo_tracking);
    fprintf(file, "enable_pattern_analysis=%u\n", current_config.enable_pattern_analysis);
    fprintf(file, "enable_adaptive_threshold=%u\n", current_config.enable_adaptive_threshold);
    fprintf(file, "blacklist_file=%s\n", current_config.blacklist_file);
    fprintf(file, "whitelist_file=%s\n", current_config.whitelist_file);
    fprintf(file, "database_file=%s\n", current_config.database_file);
    
    fclose(file);
}

void load_config(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return;
    
    char line[1024];
    char key[256];
    char value[768];
    
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%[^=]=%[^\n]", key, value) == 2) {
            if (strcmp(key, "suspicious_requests_threshold") == 0) {
                current_config.suspicious_requests_threshold = atoi(value);
            } else if (strcmp(key, "suspicious_time_window") == 0) {
                current_config.suspicious_time_window = atoi(value);
            } else if (strcmp(key, "enable_geo_tracking") == 0) {
                current_config.enable_geo_tracking = atoi(value);
            } else if (strcmp(key, "enable_pattern_analysis") == 0) {
                current_config.enable_pattern_analysis = atoi(value);
            } else if (strcmp(key, "enable_adaptive_threshold") == 0) {
                current_config.enable_adaptive_threshold = atoi(value);
            } else if (strcmp(key, "blacklist_file") == 0) {
                strncpy(current_config.blacklist_file, value, sizeof(current_config.blacklist_file) - 1);
            } else if (strcmp(key, "whitelist_file") == 0) {
                strncpy(current_config.whitelist_file, value, sizeof(current_config.whitelist_file) - 1);
            } else if (strcmp(key, "database_file") == 0) {
                strncpy(current_config.database_file, value, sizeof(current_config.database_file) - 1);
            }
        }
    }
    
    fclose(file);
}

// IP地理位置跟踪
int update_ip_location(const char* ip, const char* country_code, const char* location) {
    IPStats* stats = find_or_create_ip_stats(ip);
    if (!stats) return 0;
    
    strncpy(stats->country_code, country_code, sizeof(stats->country_code) - 1);
    strncpy(stats->location, location, sizeof(stats->location) - 1);
    
    return 1;
}

const char* get_ip_location(const char* ip) {
    static char result[MAX_LOCATION_LENGTH + MAX_COUNTRY_CODE_LENGTH + 2];
    
    for (size_t i = 0; i < ip_stats_count; i++) {
        if (strcmp(ip_stats[i].ip, ip) == 0) {
            snprintf(result, sizeof(result), "%s, %s", 
                    ip_stats[i].country_code, ip_stats[i].location);
            return result;
        }
    }
    
    return "Unknown";
}

const char* get_connection_pattern(const char* ip) {
    for (size_t i = 0; i < ip_stats_count; i++) {
        if (strcmp(ip_stats[i].ip, ip) == 0) {
            return ip_stats[i].connection_pattern;
        }
    }
    
    return "No pattern data";
}

// 高级分析功能
void detect_port_scan(const char* ip, uint16_t port) {
    IPStats* stats = find_or_create_ip_stats(ip);
    if (!stats) return;
    
    // 这里可以实现端口扫描检测逻辑
    // 例如，跟踪每个IP访问的不同端口数量
    // 如果在短时间内访问多个不同端口，可能是端口扫描
    
    // 简单实现：如果请求频率异常高，标记为可疑
    if (stats->window_requests > stats->adaptive_threshold * 2) {
        stats->is_suspicious = 1;
        printf("Warning: Possible port scan detected from IP: %s on port %u\n", 
               ip, port);
    }
}

void detect_ddos_attempt(const char* ip) {
    // 这里可以实现DDoS检测逻辑
    // 例如，检查总体流量模式，识别分布式攻击
    
    // 简单实现：检查总体连接数是否异常高
    time_t current_time = time(NULL);
    time_t window_start = current_time - current_config.suspicious_time_window;
    
    uint32_t total_connections = 0;
    for (size_t i = 0; i < connection_count; i++) {
        if (connections[i].timestamp > window_start) {
            total_connections++;
        }
    }
    
    // 如果总连接数超过阈值的10倍，可能是DDoS攻击
    if (total_connections > current_config.suspicious_requests_threshold * 10) {
        printf("Warning: Possible DDoS attack detected! Total connections in window: %u\n", 
               total_connections);
    }
}

void analyze_traffic_pattern(const char* ip) {
    IPStats* stats = find_or_create_ip_stats(ip);
    if (!stats) return;
    
    // 这里可以实现流量模式分析
    // 例如，检测周期性请求、突发请求等
    
    // 简单实现：检测突发请求
    if (stats->burst_count > 3) {
        printf("Warning: Unusual traffic pattern detected from IP: %s\n", ip);
        printf("Burst count: %u, Avg interval: %.2f seconds\n", 
               stats->burst_count, stats->avg_request_interval);
    }
}

// 重置IP统计信息
void reset_ip_stats(void) {
    if (ip_stats) {
        free(ip_stats);
        ip_stats = NULL;
    }
    ip_stats_count = 0;
}

// 生成报告函数
TrafficReport* generate_hourly_report(time_t ref_ts, size_t* count) {
    struct tm* tm_info = localtime(&ref_ts);
    time_t start_hour = ref_ts - (tm_info->tm_min * 60 + tm_info->tm_sec);
    
    // 创建24小时的报告
    TrafficReport* reports = malloc(24 * sizeof(TrafficReport));
    if (!reports) {
        *count = 0;
        return NULL;
    }
    
    // 初始化报告
    for (int i = 0; i < 24; i++) {
        time_t hour_ts = start_hour - i * 3600;
        struct tm* hour_tm = localtime(&hour_ts);
        
        snprintf(reports[i].period, sizeof(reports[i].period), 
                "%04d-%02d-%02d %02d:00",
                hour_tm->tm_year + 1900, hour_tm->tm_mon + 1, 