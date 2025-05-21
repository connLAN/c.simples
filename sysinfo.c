#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <curl/curl.h> // 添加 curl 头文件

// Function to get local IP address
void print_local_ip() {
    struct ifaddrs *ifaddr, *ifa;
    char ip[INET_ADDRSTRLEN];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return;
    }

    printf("Local IP addresses:\n");
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            void *addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, addr, ip, sizeof(ip));
            if (strcmp(ifa->ifa_name, "lo") != 0) // skip loopback
                printf("  %s: %s\n", ifa->ifa_name, ip);
        }
    }
    freeifaddrs(ifaddr);
}

// Function to get memory usage
void print_memory_usage() {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        perror("fopen");
        return;
    }
    char line[256];
    long total = 0, free = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %ld kB", &total) == 1) continue;
        if (sscanf(line, "MemAvailable: %ld kB", &free) == 1) break;
    }
    fclose(fp);
    printf("Memory: Total: %ld MB, Available: %ld MB\n", total/1024, free/1024);
}

// Function to get CPU usage
void print_cpu_usage() {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        perror("fopen");
        return;
    }
    char line[256];
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    if (fgets(line, sizeof(line), fp) == NULL) { // fix: check return value
        fclose(fp);
        perror("fgets");
        return;
    }
    sscanf(line, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
    fclose(fp);

    unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
    unsigned long long used = total - idle - iowait;
    printf("CPU Usage: %.2f%%\n", (used * 100.0) / total);
}

// 回调函数，用于处理 curl 接收到的数据
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    char **response = (char **)userp;
    *response = realloc(*response, strlen(*response) + realsize + 1);
    if (*response == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        return 0;
    }
    strncat(*response, (char *)contents, realsize);
    return realsize;
}

// 函数来获取公网 IP
void print_public_ip() {
    CURL *curl;
    CURLcode res;
    char *response = NULL;

    curl = curl_easy_init();
    if(curl) {
        response = calloc(1, 1);
        // 修改为新的 URL
        curl_easy_setopt(curl, CURLOPT_URL, "https://icanhazip.com"); 
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            printf("Public IP: %s\n", response);
        }
        curl_easy_cleanup(curl);
        free(response);
    }
}

int main() {
    print_local_ip();
    print_memory_usage();
    print_cpu_usage();
    print_public_ip(); // 调用新函数
    return 0;
}