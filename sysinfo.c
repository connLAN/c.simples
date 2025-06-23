#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>

#include "sysinfo.h"

// 实现write_callback函数
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    ResponseData *resp = (ResponseData *)userp;
    
    char *ptr = realloc(resp->data, resp->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "Out of memory\n");
        return 0;
    }
    
    resp->data = ptr;
    memcpy(&(resp->data[resp->size]), contents, realsize);
    resp->size += realsize;
    resp->data[resp->size] = 0;
    
    return realsize;
}

// 从JSON字符串中提取字段值
void extract_json_field(const char *json, const char *field, char *buffer, size_t bufsize) {
    char field_pattern[128];
    snprintf(field_pattern, sizeof(field_pattern), "\"%s\":", field);
    
    char *start = strstr(json, field_pattern);
    if (start) {
        start += strlen(field_pattern);
        
        // 跳过空白字符
        while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
            start++;
        }
        
        // 检查是否是字符串值（以引号开始）
        if (*start == '"') {
            start++; // 跳过开始的引号
            char *end = strchr(start, '"');
            if (end) {
                size_t len = end - start;
                if (len >= bufsize) len = bufsize - 1;
                strncpy(buffer, start, len);
                buffer[len] = '\0';
                return;
            }
        } 
        // 检查是否是数值或布尔值（不带引号）
        else {
            char *end = start;
            // 找到下一个空白字符、逗号或结束大括号
            while (*end && *end != ' ' && *end != '\t' && *end != '\n' && 
                   *end != '\r' && *end != ',' && *end != '}') {
                end++;
            }
            size_t len = end - start;
            if (len >= bufsize) len = bufsize - 1;
            strncpy(buffer, start, len);
            buffer[len] = '\0';
            return;
        }
    }
    snprintf(buffer, bufsize, "Not available");
}

// 格式化字节大小为人类可读格式
void format_bytes(unsigned long bytes, char *buffer) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    double size = bytes;
    
    while (size > 1024 && i < 4) {
        size /= 1024;
        i++;
    }
    
    sprintf(buffer, "%.2f %s", size, units[i]);
}

// 获取系统基本信息
void get_system_info() {
    struct utsname system_info;
    
    if (uname(&system_info) != 0) {
        perror("uname");
        return;
    }
    
    printf("\n%s=== System Information ===%s\n", COLOR_GREEN, COLOR_RESET);
    printf("%sHostname:      %s%s\n", COLOR_CYAN, COLOR_RESET, system_info.nodename);
    printf("%sOS:            %s%s %s\n", COLOR_CYAN, COLOR_RESET, system_info.sysname, system_info.release);
    printf("%sKernel:        %s%s\n", COLOR_CYAN, COLOR_RESET, system_info.version);
    printf("%sArchitecture:  %s%s\n", COLOR_CYAN, COLOR_RESET, system_info.machine);
}

// 获取内存信息
void get_memory_info() {
    struct sysinfo info;
    
    if (sysinfo(&info) != 0) {
        perror("sysinfo");
        return;
    }
    
    char total_ram[20], free_ram[20], used_ram[20];
    char total_swap[20], free_swap[20], used_swap[20];
    
    format_bytes(info.totalram * info.mem_unit, total_ram);
    format_bytes(info.freeram * info.mem_unit, free_ram);
    format_bytes((info.totalram - info.freeram) * info.mem_unit, used_ram);
    
    format_bytes(info.totalswap * info.mem_unit, total_swap);
    format_bytes(info.freeswap * info.mem_unit, free_swap);
    format_bytes((info.totalswap - info.freeswap) * info.mem_unit, used_swap);
    
    printf("\n%s=== Memory Information ===%s\n", COLOR_GREEN, COLOR_RESET);
    printf("%sTotal RAM:     %s%s\n", COLOR_CYAN, COLOR_RESET, total_ram);
    printf("%sUsed RAM:      %s%s (%.1f%%)\n", COLOR_CYAN, COLOR_RESET, used_ram, 
           100.0 * (info.totalram - info.freeram) / info.totalram);
    printf("%sFree RAM:      %s%s\n", COLOR_CYAN, COLOR_RESET, free_ram);
    
    if (info.totalswap > 0) {
        printf("%sTotal Swap:    %s%s\n", COLOR_CYAN, COLOR_RESET, total_swap);
        printf("%sUsed Swap:     %s%s (%.1f%%)\n", COLOR_CYAN, COLOR_RESET, used_swap,
               100.0 * (info.totalswap - info.freeswap) / info.totalswap);
        printf("%sFree Swap:     %s%s\n", COLOR_CYAN, COLOR_RESET, free_swap);
    }
    
    printf("%sUptime:        %s%ld days, %ld hours, %ld minutes\n", 
           COLOR_CYAN, COLOR_RESET, 
           info.uptime / 86400, (info.uptime % 86400) / 3600, (info.uptime % 3600) / 60);
}

// 获取磁盘信息
void get_disk_info() {
    struct statvfs stat;
    
    if (statvfs("/", &stat) != 0) {
        perror("statvfs");
        return;
    }
    
    unsigned long total_space = stat.f_blocks * stat.f_frsize;
    unsigned long free_space = stat.f_bfree * stat.f_frsize;
    unsigned long used_space = total_space - free_space;
    
    char total[20], free[20], used[20];
    format_bytes(total_space, total);
    format_bytes(free_space, free);
    format_bytes(used_space, used);
    
    printf("\n%s=== Disk Information ===%s\n", COLOR_GREEN, COLOR_RESET);
    printf("%sTotal Space:   %s%s\n", COLOR_CYAN, COLOR_RESET, total);
    printf("%sUsed Space:    %s%s (%.1f%%)\n", COLOR_CYAN, COLOR_RESET, used, 
           100.0 * used_space / total_space);
    printf("%sFree Space:    %s%s\n", COLOR_CYAN, COLOR_RESET, free);
}

// 获取网络接口信息
void get_network_info() {
    struct ifaddrs *ifaddr, *ifa;
    int family;
    
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return;
    }
    
    printf("\n%s=== Network Interfaces ===%s\n", COLOR_GREEN, COLOR_RESET);
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }
        
        family = ifa->ifa_addr->sa_family;
        
        // 只显示IPv4和IPv6地址
        if (family == AF_INET || family == AF_INET6) {
            printf("%s%s: %s", COLOR_CYAN, ifa->ifa_name, COLOR_RESET);
            
            if (family == AF_INET) {
                // IPv4
                struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
                printf("IPv4: %s\n", inet_ntoa(addr->sin_addr));
            } else {
                // IPv6 (简化处理)
                printf("IPv6 address\n");
            }
        }
    }
    
    freeifaddrs(ifaddr);
}

// 获取公网IP和地理位置信息
void get_public_ip() {
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize CURL\n");
        return;
    }

    printf("\n%s=== Public IP Information ===%s\n", COLOR_GREEN, COLOR_RESET);
    
    ResponseData response = {0};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    
    // 尝试使用ipinfo.io API
    curl_easy_setopt(curl, CURLOPT_URL, "https://ipinfo.io/json");
    
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK && response.data) {
        char buffer[256];
        
        // 提取IP地址
        extract_json_field(response.data, "ip", buffer, sizeof(buffer));
        printf("%sIP Address:    %s%s\n", COLOR_CYAN, COLOR_RESET, buffer);
        
        printf("\n%sLocation Information:%s\n", COLOR_YELLOW, COLOR_RESET);
        
        // 提取国家信息
        extract_json_field(response.data, "country", buffer, sizeof(buffer));
        printf("%sCountry:       %s%s\n", COLOR_CYAN, COLOR_RESET, buffer);
        
        // 提取地区信息
        extract_json_field(response.data, "region", buffer, sizeof(buffer));
        printf("%sRegion:        %s%s\n", COLOR_CYAN, COLOR_RESET, buffer);
        
        // 提取城市信息
        extract_json_field(response.data, "city", buffer, sizeof(buffer));
        printf("%sCity:          %s%s\n", COLOR_CYAN, COLOR_RESET, buffer);
        
        // 提取位置信息
        extract_json_field(response.data, "loc", buffer, sizeof(buffer));
        
        // 在两个数字之间插入逗号
        char formatted_loc[64];
        double lat, lon;
        if (sscanf(buffer, "%lf%lf", &lat, &lon) == 2) {
            snprintf(formatted_loc, sizeof(formatted_loc), "%.4f, %.4f", lat, lon);
            printf("%sCoordinates:   %s%s\n", COLOR_CYAN, COLOR_RESET, formatted_loc);
        } else {
            printf("%sCoordinates:   %s%s\n", COLOR_CYAN, COLOR_RESET, buffer);
        }
        
        // 提取时区信息
        extract_json_field(response.data, "timezone", buffer, sizeof(buffer));
        printf("%sTimezone:      %s%s\n", COLOR_CYAN, COLOR_RESET, buffer);
        
        // 提取组织/ISP信息
        extract_json_field(response.data, "org", buffer, sizeof(buffer));
        printf("%sISP/Org:       %s%s\n", COLOR_CYAN, COLOR_RESET, buffer);
    } else {
        fprintf(stderr, "IP info query failed: %s\n", curl_easy_strerror(res));
        
        // 如果ipinfo.io失败，尝试使用ip-api.com
        free(response.data);
        response.data = NULL;
        response.size = 0;
        
        printf("\nTrying alternative API...\n");
        curl_easy_setopt(curl, CURLOPT_URL, "http://ip-api.com/json/");
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK && response.data) {
            char buffer[256];
            
            // 提取IP地址
            extract_json_field(response.data, "query", buffer, sizeof(buffer));
            printf("%sIP Address:    %s%s\n", COLOR_CYAN, COLOR_RESET, buffer);
            
            printf("\n%sLocation Information:%s\n", COLOR_YELLOW, COLOR_RESET);
            
            // 提取国家信息
            extract_json_field(response.data, "country", buffer, sizeof(buffer));
            printf("%sCountry:       %s%s\n", COLOR_CYAN, COLOR_RESET, buffer);
            
            // 提取地区信息
            extract_json_field(response.data, "regionName", buffer, sizeof(buffer));
            printf("%sRegion:        %s%s\n", COLOR_CYAN, COLOR_RESET, buffer);
            
            // 提取城市信息
            extract_json_field(response.data, "city", buffer, sizeof(buffer));
            printf("%sCity:          %s%s\n", COLOR_CYAN, COLOR_RESET, buffer);
            
            // 提取时区信息
            extract_json_field(response.data, "timezone", buffer, sizeof(buffer));
            printf("%sTimezone:      %s%s\n", COLOR_CYAN, COLOR_RESET, buffer);
            
            // 提取ISP信息
            extract_json_field(response.data, "isp", buffer, sizeof(buffer));
            printf("%sISP:           %s%s\n", COLOR_CYAN, COLOR_RESET, buffer);
        } else {
            fprintf(stderr, "All IP API queries failed\n");
        }
    }
    
    free(response.data);
    curl_easy_cleanup(curl);
}

int main() {
    // 初始化curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // 获取各类系统信息
    get_system_info();
    get_memory_info();
    get_disk_info();
    get_network_info();
    get_public_ip();
    
    // 清理curl
    curl_global_cleanup();
    
    printf("\n"); // 添加最后的换行
    return 0;
}