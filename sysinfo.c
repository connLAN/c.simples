#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <signal.h>
#include <math.h>

#define MAX_URLS 10
#define TIMEOUT_SEC 10
#define MAX_LATENCY 1000.0  // 最大可接受的网络延迟(ms)
#define MAX_RETRIES 3
#define MIN_AVAILABILITY 20.0 // 最低可用性估值

typedef struct {
    const char *name;
    CURLINFO info;
    double value;
} TimeMetric;

// 中国国内不同区域的测试域名
const char *china_domains[] = {
    "https://www.baidu.com",
    "https://www.taobao.com",
    "https://www.qq.com",
    "https://www.jd.com",
    "https://www.sina.com.cn",
    "https://www.163.com",
    "https://www.ifeng.com",
    "https://www.xinhuanet.com",
    "https://www.gov.cn",
    "https://www.toutiao.com",
    NULL
};

// 获取本地IP地址
void print_local_ip() {
    struct ifaddrs *ifaddr, *ifa;
    char ip[INET_ADDRSTRLEN];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs failed");
        return;
    }

    printf("\n=== 本地IP地址 ===\n");
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) 
            continue;
        
        void *addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
        inet_ntop(AF_INET, addr, ip, sizeof(ip));
        if (strcmp(ifa->ifa_name, "lo") != 0)
            printf("%-8s: %s\n", ifa->ifa_name, ip);
    }
    freeifaddrs(ifaddr);
}

// 获取内存使用情况
double get_memory_usage() {
    long total = 0, free = 0, buffers = 0, cached = 0;
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        perror("无法打开/proc/meminfo");
        return -1;
    }

    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %ld kB", &total) == 1) continue;
        if (sscanf(line, "MemFree: %ld kB", &free) == 1) continue;
        if (sscanf(line, "Buffers: %ld kB", &buffers) == 1) continue;
        if (sscanf(line, "Cached: %ld kB", &cached) == 1) continue;
    }
    fclose(fp);

    if (total == 0) {
        fprintf(stderr, "错误: 无法读取内存信息\n");
        return -1;
    }

    long used = total - free - buffers - cached;
    double usage = (used * 100.0) / total;
    
    printf("\n=== 内存使用 ===\n");
    printf("总量:    %6ld MB\n", total/1024);
    printf("已用:     %6ld MB (%.1f%%)\n", used/1024, usage);
    printf("空闲:     %6ld MB\n", free/1024);
    
    return usage;
}

// 获取CPU使用率
double get_cpu_usage() {
    static int first_run = 1;  // 新增首次运行标志
    static unsigned long long last_total = 0, last_idle = 0;
    unsigned long long total = 0, idle = 0, user = 0, nice = 0, system = 0, iowait = 0;
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        perror("无法打开/proc/stat");
        return -1;
    }

    char line[256];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        fprintf(stderr, "读取/proc/stat错误\n");
        return -1;
    }
    fclose(fp);

    if (sscanf(line+5, "%llu %llu %llu %llu %llu", &user, &nice, &system, &idle, &iowait) < 5) {
        fprintf(stderr, "解析CPU状态错误\n");
        return -1;
    }
    total = user + nice + system + idle + iowait;

    double usage = -1;
    if (first_run) {
        // 首次运行只记录初始值，不计算使用率
        first_run = 0;
    } else if (last_total != 0 && (total - last_total) > 0) {  // 修改判断条件
        usage = 100.0 * ((total - last_total) - (idle - last_idle)) / (total - last_total);
        printf("\n=== CPU使用率 ===\n%.1f%%\n", usage);
    }
    last_total = total;
    last_idle = idle;
    
    return usage;
}

// CURL回调函数
size_t write_callback(void *data, size_t size, size_t nmemb, void *userp) {
    (void)data; (void)size; (void)nmemb; (void)userp;
    return size * nmemb;
}

// 测试URL延迟
double test_url_latency(const char *url) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "CURL初始化失败\n");
        return -1;
    }

    TimeMetric metrics[] = {
        {"DNS查询", CURLINFO_NAMELOOKUP_TIME, 0},
        {"连接时间", CURLINFO_CONNECT_TIME, 0},
        {"TLS握手", CURLINFO_APPCONNECT_TIME, 0},
        {"首字节时间", CURLINFO_STARTTRANSFER_TIME, 0},
        {"总时间", CURLINFO_TOTAL_TIME, 0}
    };
    size_t metrics_count = sizeof(metrics)/sizeof(metrics[0]);

    int retries = 0;
    double total_latency = -1;
    CURLcode res;

    do {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT_SEC);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

        res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            printf("\n=== %s 延迟测试 ===\n", url);
            for (size_t i = 0; i < metrics_count; i++) {
                if (curl_easy_getinfo(curl, metrics[i].info, &metrics[i].value) == CURLE_OK) {
                    metrics[i].value *= 1000; // 转换为毫秒
                    printf("%-15s: %7.2f ms\n", metrics[i].name, metrics[i].value);
                    if (strcmp(metrics[i].name, "总时间") == 0) {
                        total_latency = metrics[i].value;
                    }
                }
            }
            break;
        } else {
            fprintf(stderr, "延迟测试失败: %s\n", curl_easy_strerror(res));
            if (retries < MAX_RETRIES - 1) {
                fprintf(stderr, "2秒后重试 %s...\n", url);
                sleep(2);
            }
        }
        retries++;
    } while (retries < MAX_RETRIES);

    curl_easy_cleanup(curl);
    
    return total_latency;
}

// 计算系统可用性(强制返回一个估值)
double calculate_availability(double cpu_usage, double memory_usage, double avg_latency, int data_status) {
    // 默认值(最差情况)
    double default_cpu = 100.0;
    double default_mem = 100.0;
    double default_latency = MAX_LATENCY * 2;
    
    // 根据数据状态调整
    if (data_status == 0) { // 完整数据
        // 使用实际值
    } else if (data_status == 1) { // 部分数据
        if (cpu_usage < 0) cpu_usage = default_cpu;
        if (memory_usage < 0) memory_usage = default_mem;
        if (avg_latency < 0) avg_latency = default_latency;
    } else { // 无数据
        cpu_usage = default_cpu;
        memory_usage = default_mem;
        avg_latency = default_latency;
    }
    
    // 确保数值在合理范围内
    cpu_usage = fmax(0, fmin(100, cpu_usage));
    memory_usage = fmax(0, fmin(100, memory_usage));
    avg_latency = fmax(0, avg_latency);
    
    // 归一化指标
    double cpu_score = 1.0 - (cpu_usage / 100.0);
    double memory_score = 1.0 - (memory_usage / 100.0);
    double network_score = (MAX_LATENCY - fmin(avg_latency, MAX_LATENCY*2)) / MAX_LATENCY;
    network_score = fmax(0, fmin(1, network_score));
    
    // 权重分配
    const double weights[] = {0.3, 0.3, 0.4}; // CPU,内存,网络
    double availability = (cpu_score * weights[0]) + 
                        (memory_score * weights[1]) + 
                        (network_score * weights[2]);
    
    // 确保最低可用性
    availability = fmax(MIN_AVAILABILITY/100.0, availability);
    
    return availability * 100.0;
}

// 信号处理
void handle_signal(int sig) {
    printf("\n收到信号 %d, 清理资源...\n", sig);
    curl_global_cleanup();
    exit(0);
}

int main() {
    // 注册信号处理
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // 初始化CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // 获取系统信息
    print_local_ip();
    get_cpu_usage();  // 新增：预热CPU数据采集
    sleep(1);         // 新增：等待1秒采集间隔
    double cpu_usage = get_cpu_usage();
    double memory_usage = get_memory_usage();
    
    // 测试国内不同区域域名延迟
    double total_latency = 0;
    int valid_tests = 0;
    int total_tests = 0;
    
    printf("\n=== 中国网络延迟测试 ===\n");
    for (int i = 0; china_domains[i]; i++) {
        total_tests++;
        double latency = test_url_latency(china_domains[i]);
        if (latency >= 0) {
            total_latency += latency;
            valid_tests++;
        }
    }
    
    // 计算平均延迟(无有效数据时使用最大值)
    double avg_latency = (valid_tests > 0) ? (total_latency / valid_tests) : -1;
    
    // 确定数据状态:
    // 0=完整数据, 1=部分数据, 2=无数据
    int data_status = 2;
    if (cpu_usage >= 0 && memory_usage >= 0 && valid_tests > 0) {
        data_status = 0;
    } else if ((cpu_usage >= 0 || memory_usage >= 0 || valid_tests > 0)) {
        data_status = 1;
    }
    
    // 强制计算可用性
    double availability = calculate_availability(cpu_usage, memory_usage, avg_latency, data_status);
    
    // 显示结果
    printf("\n=== 系统可用性指标 ===\n");
    if (cpu_usage >= 0) {
        printf("CPU使用率:        %6.1f%%\n", cpu_usage);
    } else {
        printf("CPU使用率:        无数据\n");
    }
    
    if (memory_usage >= 0) {
        printf("内存使用率:       %6.1f%%\n", memory_usage);
    } else {
        printf("内存使用率:       无数据\n");
    }
    
    if (avg_latency >= 0) {
        printf("平均网络延迟:     %5.1f ms (%d/%d成功)\n", avg_latency, valid_tests, total_tests);
    } else {
        printf("平均网络延迟:     无数据\n");
    }
    
    printf("------------------------------\n");
    
    if (data_status == 0) {
        printf("系统可用性:       %5.1f%% (基于完整数据)\n", availability);
    } else if (data_status == 1) {
        printf("系统可用性:       %5.1f%% (基于部分数据，保守估计)\n", availability);
    } else {
        printf("系统可用性:       %5.1f%% (无数据，最低估值)\n", availability);
    }
    
    // 健康状态提示
    if (availability < 60.0) {
        printf("\n警告: 系统可用性低于60%%，建议立即检查！\n");
    } else if (availability < 80.0) {
        printf("\n注意: 系统可用性低于80%%，建议关注\n");
    }
    
    // 清理资源
    curl_global_cleanup();
    return 0;
}