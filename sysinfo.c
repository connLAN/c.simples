#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <curl/curl.h>

#define MAX_URLS 10
#define TIMEOUT_SEC 10

typedef struct {
    const char *name;
    CURLINFO info;
    double value;
} TimeMetric;

// 统一错误处理宏
#define CHECK_NULL(ptr, msg) if (!(ptr)) { fprintf(stderr, "Error: %s\n", (msg)); return; }
#define CURL_CHECK(res, msg) if ((res) != CURLE_OK) { fprintf(stderr, "CURL Error: %s (%s)\n", (msg), curl_easy_strerror(res)); }

// 获取本地IP地址
void print_local_ip() {
    struct ifaddrs *ifaddr, *ifa;
    char ip[INET_ADDRSTRLEN];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs failed");
        return;
    }

    printf("\n=== Local IP Addresses ===\n");
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) 
            continue;
        
        void *addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
        inet_ntop(AF_INET, addr, ip, sizeof(ip));
        if (strcmp(ifa->ifa_name, "lo") != 0) // 跳过回环地址
            printf("%-8s: %s\n", ifa->ifa_name, ip);
    }
    freeifaddrs(ifaddr);
}

// 获取内存使用情况
void print_memory_usage() {
    long total = 0, free = 0, buffers = 0, cached = 0;
    FILE *fp = fopen("/proc/meminfo", "r");
    CHECK_NULL(fp, "Failed to open /proc/meminfo");

    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %ld kB", &total) == 1) continue;
        if (sscanf(line, "MemFree: %ld kB", &free) == 1) continue;
        if (sscanf(line, "Buffers: %ld kB", &buffers) == 1) continue;
        if (sscanf(line, "Cached: %ld kB", &cached) == 1) continue;
    }
    fclose(fp);

    if (total == 0) {
        fprintf(stderr, "Error: Could not read memory info\n");
        return;
    }

    long used = total - free - buffers - cached;
    printf("\n=== Memory Usage ===\n");
    printf("Total:    %6ld MB\n", total/1024);
    printf("Used:     %6ld MB (%.1f%%)\n", used/1024, (used*100.0)/total);
    printf("Free:     %6ld MB\n", free/1024);
}

// 获取CPU使用率
void print_cpu_usage() {
    static unsigned long long last_total = 0, last_idle = 0;
    unsigned long long total = 0, idle = 0, user = 0, nice = 0, system = 0, iowait = 0;
    FILE *fp = fopen("/proc/stat", "r");
    CHECK_NULL(fp, "Failed to open /proc/stat");

    char line[256];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        fprintf(stderr, "Error reading /proc/stat\n");
        return;
    }
    fclose(fp);

    if (sscanf(line+5, "%llu %llu %llu %llu %llu", &user, &nice, &system, &idle, &iowait) < 5) {
        fprintf(stderr, "Error parsing CPU stats\n");
        return;
    }
    total = user + nice + system + idle + iowait;

    if (last_total != 0) { // 不是第一次运行
        double delta_total = total - last_total;
        double delta_idle = idle - last_idle;
        
        if (delta_total > 0) { // 避免除零错误
            double usage = 100.0 * (delta_total - delta_idle) / delta_total;
            printf("\n=== CPU Usage ===\n%.1f%%\n", usage);
        }
    }
    last_total = total;
    last_idle = idle;
}

// CURL回调函数
size_t write_callback(void *data, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    char **resp = (char **)userp;
    
    size_t old_len = *resp ? strlen(*resp) : 0;
    char *new_ptr = realloc(*resp, old_len + realsize + 1);
    if (!new_ptr) {
        return 0; // 内存分配失败
    }
    
    *resp = new_ptr;
    memcpy(*resp + old_len, data, realsize);
    (*resp)[old_len + realsize] = '\0';
    return realsize;
}

// 获取公网IP信息
void fetch_public_ip_info(int ipv6) {
    CURL *curl = curl_easy_init();
    CHECK_NULL(curl, "CURL init failed");

    char *response = NULL;
    const char *url = ipv6 ? "https://api64.ipify.org" : "https://api.ipify.org";
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT_SEC);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK && response) {
        printf("\n=== Public %s ===\n%s\n", ipv6 ? "IPv6" : "IP", response);
    } else {
        CURL_CHECK(res, "Failed to get public IP");
    }

    free(response);
    curl_easy_cleanup(curl);
}

// 测试URL延迟
void test_url_latency(const char *url) {
    CURL *curl = curl_easy_init();
    CHECK_NULL(curl, "CURL init failed");

    TimeMetric metrics[] = {
        {"DNS Lookup", CURLINFO_NAMELOOKUP_TIME, 0},
        {"Connect", CURLINFO_CONNECT_TIME, 0},
        {"TLS Handshake", CURLINFO_APPCONNECT_TIME, 0},
        {"Transfer Start", CURLINFO_STARTTRANSFER_TIME, 0},
        {"Total", CURLINFO_TOTAL_TIME, 0}
    };
    size_t metrics_count = sizeof(metrics)/sizeof(metrics[0]);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD请求
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT_SEC);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        printf("\n=== Latency for %s ===\n", url);
        for (size_t i = 0; i < metrics_count; i++) {
            if (curl_easy_getinfo(curl, metrics[i].info, &metrics[i].value) == CURLE_OK) {
                printf("%-15s: %7.2f ms\n", metrics[i].name, metrics[i].value * 1000);
            }
        }
    } else {
        CURL_CHECK(res, "Latency test failed");
    }
    curl_easy_cleanup(curl);
}

int main() {
    // 初始化CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // 系统信息
    print_local_ip();
    print_memory_usage();
    print_cpu_usage();
    
    // 网络信息
    fetch_public_ip_info(0); // IPv4
    fetch_public_ip_info(1); // IPv6
    
    // 延迟测试
    const char *test_urls[MAX_URLS] = {
        "https://www.baidu.com",
        "https://www.google.com",
        "https://github.com",
        NULL // 结束标记
    };
    
    for (int i = 0; test_urls[i]; i++) {
        test_url_latency(test_urls[i]);
    }
    
    // 清理
    curl_global_cleanup();
    return 0;
}