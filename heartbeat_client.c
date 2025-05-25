// Add these at the VERY TOP of the file
#define _POSIX_C_SOURCE 200112L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>  // Add this missing header for network functions
#include <pthread.h>
#include <time.h>
#include <curl/curl.h>
#include <sys/statvfs.h> // For statvfs
#include <netinet/in.h>   // For NI_MAXHOST and in_addr
#include <errno.h>        // For gai_strerror

#define INTERVAL 10 // 发送间隔时间（秒）
// Update this line to match your server's real address
#define SERVER_URL "http://localhost:8080/api/heartbeat" 

// 如果系统中没有定义 NI_MAXHOST 和 NI_NUMERICHOST，手动定义它们
#ifndef NI_MAXHOST
#define NI_MAXHOST 256
#endif

#ifndef NI_NUMERICHOST
#define NI_NUMERICHOST 1
#endif

// 获取CPU使用率
double get_cpu_usage() {
    FILE* file;
    char line[256];
    double user, nice, system, idle;

    file = fopen("/proc/stat", "r");
    if (!file) {
        perror("Failed to open /proc/stat");
        return -1;
    }
    if (fgets(line, sizeof(line), file) == NULL) {
        perror("Failed to read /proc/stat");
        fclose(file);
        return -1;
    }
    sscanf(line, "%*s %lf %lf %lf %lf", &user, &nice, &system, &idle);

    fclose(file);

    double total_time = user + nice + system + idle;
    static double last_total_time = -1.0;
    static double last_idle_time = -1.0;
    if (last_total_time == -1) {
        last_total_time = total_time;
        last_idle_time = idle;
        sleep(1);
        return get_cpu_usage();
    }

    double cpu_usage = (total_time - last_total_time - (idle - last_idle_time)) / (total_time - last_total_time) * 100;
    last_total_time = total_time;
    last_idle_time = idle;

    return cpu_usage;
}

// 获取内存使用率
double get_memory_usage() {
    FILE* meminfo = fopen("/proc/meminfo", "r");
    if (!meminfo) {
        perror("Failed to open /proc/meminfo");
        return -1;
    }

    unsigned long long mem_total = 0, mem_free = 0, buffers = 0, cached = 0;
    char line[256]; // Declare line here

    while (fgets(line, sizeof(line), meminfo)) {
        if (sscanf(line, "MemTotal: %llu kB", &mem_total) == 1 ||
            sscanf(line, "MemFree: %llu kB", &mem_free) == 1 ||
            sscanf(line, "Buffers: %llu kB", &buffers) == 1 ||
            sscanf(line, "Cached: %llu kB", &cached) == 1)
            continue;
    }
    fclose(meminfo);

    unsigned long long used_mem = mem_total - mem_free - buffers - cached;
    return ((double)used_mem / mem_total) * 100;
}

// 获取硬盘使用率
double get_disk_usage(const char* path) {
    struct statvfs fs_stats; // Include sys/statvfs.h for this structure
    if (statvfs(path, &fs_stats) != 0) {
        perror("Failed to statvfs");
        return -1;
    }

    uint64_t blocks_used = fs_stats.f_blocks - fs_stats.f_bfree;
    uint64_t blocks_total = fs_stats.f_blocks;
    return ((double)blocks_used / blocks_total) * 100;
}

// 获取本地IP地址
void get_local_ip(char* ip_buffer, size_t buffer_size) {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST]; // 确保 NI_MAXHOST 已定义

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        if (family == AF_INET && strcmp(ifa->ifa_name, "lo") != 0) {
            s = getnameinfo(ifa->ifa_addr,
                            (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                            host, NI_MAXHOST,
                            NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                fprintf(stderr, "getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }
            strncpy(ip_buffer, host, buffer_size); // 确保 ip_buffer 被正确赋值
            break;
        }
    }

    freeifaddrs(ifaddr);
}

// 获取公网IP地址
size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    strncat(userp, contents, realsize);
    return realsize;
}

void get_public_ip(char* ip_buffer, size_t buffer_size) {
    CURL *curl;
    CURLcode res;
    char public_ip[INET_ADDRSTRLEN] = "";

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.ipify.org/");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, public_ip);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));

        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    strncpy(ip_buffer, public_ip, buffer_size);
}

// 计算与服务器的时延
double calculate_latency(const char* server_url) {
    CURL *curl;
    CURLcode res;
    double latency = 0.0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, server_url);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // Perform a HEAD request
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L); // Timeout after 5 seconds
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 2L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 3000L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);
        res = curl_easy_perform(curl);

        if(res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME, &latency);
        } else {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    return latency;
}

int main() {
    while (1) {
        char local_ip[INET_ADDRSTRLEN], public_ip[INET_ADDRSTRLEN];
        memset(local_ip, 0, INET_ADDRSTRLEN);
        memset(public_ip, 0, INET_ADDRSTRLEN);

        get_local_ip(local_ip, INET_ADDRSTRLEN);
        get_public_ip(public_ip, INET_ADDRSTRLEN);

        double cpu_usage = get_cpu_usage();
        double memory_usage = get_memory_usage();
        double disk_usage = get_disk_usage("/");
        double latency = calculate_latency(SERVER_URL);

        double availability = 100 - ((cpu_usage + memory_usage + disk_usage) / 3);

        printf("Local IP: %s\n", local_ip);
        printf("Public IP: %s\n", public_ip);
        printf("CPU Usage: %.2f%%\n", cpu_usage);
        printf("Memory Usage: %.2f%%\n", memory_usage);
        printf("Disk Usage: %.2f%%\n", disk_usage);
        printf("Availability: %.2f%%\n", availability);
        printf("Latency: %.2f ms\n", latency * 1000);

        // 构建POST数据
        char post_data[512];
        snprintf(post_data, sizeof(post_data),
                 "local_ip=%s&public_ip=%s&cpu_usage=%.2f&memory_usage=%.2f&disk_usage=%.2f&availability=%.2f&latency=%.2f",
                 local_ip, public_ip, cpu_usage, memory_usage, disk_usage, availability, latency * 1000);

        // 发送POST请求
        CURL *curl;
        CURLcode res;
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
        headers = curl_slist_append(headers, "Connection: close");  // Add this header
    
        curl_global_init(CURL_GLOBAL_ALL);
        curl = curl_easy_init();
        if(curl) {
            // Add verbose mode for debugging
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
            curl_easy_setopt(curl, CURLOPT_URL, SERVER_URL);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);  // Add headers
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
            // Add timeout settings
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L); 
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 4L);
            
            res = curl_easy_perform(curl);
            if(res != CURLE_OK)
                fprintf(stderr, "curl_easy_perform() failed: %s\n",
                        curl_easy_strerror(res));
    
            curl_slist_free_all(headers);  // Cleanup headers
        }
    
        curl_global_cleanup();

        sleep(INTERVAL);
    }

    return 0;
}