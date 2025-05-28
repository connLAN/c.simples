#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <signal.h>
#include <math.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <pthread.h>

#define MAX_URLS 10
#define TIMEOUT_SEC 10
#define MAX_LATENCY 1000.0
#define MAX_RETRIES 3
#define MIN_AVAILABILITY 20.0
#define TR_MAX_HOPS 30
#define TR_TIMEOUT 2
#define TR_PACKET_SIZE 64
#define TR_PORT 33434
#define TR_PORT_V6 33434
#define ICMP_HEADER_LEN 8
#define ICMP6_HEADER_LEN 8
#define MAX_THREADS 10

typedef struct {
    const char *name;
    CURLINFO info;
    double value;
} TimeMetric;

typedef struct {
    union {
        struct sockaddr_in v4;
        struct sockaddr_in6 v6;
    } target;
    int sockfd;
    int ttl;
    struct timeval start_time;
    int is_ipv6;
} TraceRouteContext;

struct icmp_header {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
};

struct icmp6_header {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
};

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

/* Function prototypes */
void print_local_ip();
double get_memory_usage();
double get_cpu_usage();
size_t write_callback(void *data, size_t size, size_t nmemb, void *userp);
double test_url_latency(const char *url);
double calculate_availability(double cpu_usage, double memory_usage, double avg_latency, int data_status);
void handle_signal(int sig);
void trace_route(const char *dest);
void trace_route_to_buffer(const char *dest, FILE *out);
static int create_icmp_socket(int is_ipv6);
static int send_probe(TraceRouteContext *ctx);
static int recv_response(TraceRouteContext *ctx, char *host, char *ip);
unsigned short in_cksum(unsigned short *addr, int len);
void check_privileges();

typedef struct {
    const char *url;
    double result;
    int valid;
    char route_info[2048];  // 存储traceroute结果
} ThreadArg;

// 修改线程函数
void* thread_test_latency(void *arg) {
    ThreadArg *targ = (ThreadArg *)arg;
    
    // 临时注释掉traceroute调用
    /*
    FILE *fp = fmemopen(targ->route_info, sizeof(targ->route_info), "w");
    if (fp) {
        trace_route_to_buffer(targ->url, fp);
        fclose(fp);
    }
    */
    
    // 执行延迟测试
    targ->result = test_url_latency(targ->url);
    targ->valid = (targ->result >= 0) ? 1 : 0;
    return NULL;
}

unsigned short in_cksum(unsigned short *addr, int len) {
    int nleft = len;
    int sum = 0;
    unsigned short *w = addr;
    unsigned short answer = 0;

    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1) {
        *(unsigned char *)(&answer) = *(unsigned char *)w;
        sum += answer;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;
    return answer;
}

void check_privileges() {
    if (getuid() != 0) {
        fprintf(stderr, "Warning: Root privileges required for full functionality\n");
    }
}

void print_local_ip() {
    struct ifaddrs *ifaddr, *ifa;
    char ip[INET6_ADDRSTRLEN];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs failed");
        return;
    }

    printf("\n=== Local IP Addresses ===\n");
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) 
            continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            void *addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, addr, ip, sizeof(ip));
            if (strcmp(ifa->ifa_name, "lo") != 0)
                printf("%-8s IPv4: %s\n", ifa->ifa_name, ip);
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            void *addr = &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
            inet_ntop(AF_INET6, addr, ip, sizeof(ip));
            if (strcmp(ifa->ifa_name, "lo") != 0)
                printf("%-8s IPv6: %s\n", ifa->ifa_name, ip);
        }
    }
    freeifaddrs(ifaddr);
}

double get_memory_usage() {
    long total = 0, free = 0, buffers = 0, cached = 0;
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        perror("Can't open /proc/meminfo");
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
        fprintf(stderr, "Error: Can't read memory info\n");
        return -1;
    }

    long used = total - free - buffers - cached;
    double usage = (used * 100.0) / total;
    
    printf("\n=== Memory Usage ===\n");
    printf("Total:    %6ld MB\n", total/1024);
    printf("Used:     %6ld MB (%.1f%%)\n", used/1024, usage);
    printf("Free:     %6ld MB\n", free/1024);
    
    return usage;
}

double get_cpu_usage() {
    static int first_run = 1;
    static unsigned long long last_total = 0, last_idle = 0;
    unsigned long long total = 0, idle = 0, user = 0, nice = 0, system = 0, iowait = 0;
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        perror("Can't open /proc/stat");
        return -1;
    }

    char line[256];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        fprintf(stderr, "Error reading /proc/stat\n");
        return -1;
    }
    fclose(fp);

    if (sscanf(line+5, "%llu %llu %llu %llu %llu", &user, &nice, &system, &idle, &iowait) < 5) {
        fprintf(stderr, "Error parsing CPU stats\n");
        return -1;
    }
    total = user + nice + system + idle + iowait;

    double usage = -1;
    if (first_run) {
        first_run = 0;
    } else if (last_total != 0 && (total - last_total) > 0) {
        usage = 100.0 * ((total - last_total) - (idle - last_idle)) / (total - last_total);
        printf("\n=== CPU Usage ===\n%.1f%%\n", usage);
    }
    last_total = total;
    last_idle = idle;
    
    return usage;
}

size_t write_callback(void *data, size_t size, size_t nmemb, void *userp) {
    return size * nmemb;
}

double test_url_latency(const char *url) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "CURL init failed\n");
        return -1;
    }

    TimeMetric metrics[] = {
        {"DNS Lookup", CURLINFO_NAMELOOKUP_TIME, 0},
        {"Connect", CURLINFO_CONNECT_TIME, 0},
        {"TLS Handshake", CURLINFO_APPCONNECT_TIME, 0},
        {"First Byte", CURLINFO_STARTTRANSFER_TIME, 0},
        {"Total", CURLINFO_TOTAL_TIME, 0}
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
            printf("\n=== Latency for %s ===\n", url);
            for (size_t i = 0; i < metrics_count; i++) {
                if (curl_easy_getinfo(curl, metrics[i].info, &metrics[i].value) == CURLE_OK) {
                    metrics[i].value *= 1000;
                    printf("%-15s: %7.2f ms\n", metrics[i].name, metrics[i].value);
                    if (strcmp(metrics[i].name, "Total") == 0) {
                        total_latency = metrics[i].value;
                    }
                }
            }
            break;
        } else {
            fprintf(stderr, "Latency test failed: %s\n", curl_easy_strerror(res));
            if (retries < MAX_RETRIES - 1) {
                fprintf(stderr, "Retrying in 2 seconds...\n");
                sleep(2);
            }
        }
        retries++;
    } while (retries < MAX_RETRIES);

    curl_easy_cleanup(curl);
    
    return total_latency;
}

double calculate_availability(double cpu_usage, double memory_usage, double avg_latency, int data_status) {
    double default_cpu = 100.0;
    double default_mem = 100.0;
    double default_latency = MAX_LATENCY * 2;
    
    if (data_status == 1) {
        if (cpu_usage < 0) cpu_usage = default_cpu;
        if (memory_usage < 0) memory_usage = default_mem;
        if (avg_latency < 0) avg_latency = default_latency;
    } else if (data_status == 2) {
        cpu_usage = default_cpu;
        memory_usage = default_mem;
        avg_latency = default_latency;
    }
    
    cpu_usage = fmax(0, fmin(100, cpu_usage));
    memory_usage = fmax(0, fmin(100, memory_usage));
    avg_latency = fmax(0, avg_latency);
    
    double cpu_score = 1.0 - (cpu_usage / 100.0);
    double memory_score = 1.0 - (memory_usage / 100.0);
    double network_score = (MAX_LATENCY - fmin(avg_latency, MAX_LATENCY*2)) / MAX_LATENCY;
    network_score = fmax(0, fmin(1, network_score));
    
    const double weights[] = {0.3, 0.3, 0.4};
    double availability = (cpu_score * weights[0]) + 
                         (memory_score * weights[1]) + 
                         (network_score * weights[2]);
    
    return fmax(MIN_AVAILABILITY, availability * 100.0);
}

void handle_signal(int sig) {
    printf("\nReceived signal %d, cleaning up...\n", sig);
    curl_global_cleanup();
    exit(0);
}

static int create_icmp_socket(int is_ipv6) {
    int sock;
    if (is_ipv6) {
        sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
        if (sock < 0) {
            perror("Can't create raw IPv6 socket");
            return -1;
        }
    } else {
        sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        if (sock < 0) {
            perror("Can't create raw IPv4 socket");
            return -1;
        }
    }

    struct timeval tv = {TR_TIMEOUT, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    return sock;
}

static int send_probe(TraceRouteContext *ctx) {
    char packet[TR_PACKET_SIZE];
    if (ctx->is_ipv6) {
        struct icmp6_header *icmp = (struct icmp6_header *)packet;
        memset(packet, 0, sizeof(packet));
        icmp->type = ICMP6_ECHO_REQUEST;
        icmp->code = 0;
        icmp->id = getpid() & 0xFFFF;
        icmp->seq = ctx->ttl;
        // ICMPv6 checksum calculation is more complex and may require pseudo-header
        // For simplicity, we set it to 0 here, the kernel will handle it
        icmp->checksum = 0;

        if (setsockopt(ctx->sockfd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, 
                      &ctx->ttl, sizeof(ctx->ttl)) < 0) {
            perror("Can't set IPv6 TTL");
            return -1;
        }

        return sendto(ctx->sockfd, packet, sizeof(packet), 0,
                     (struct sockaddr*)&ctx->target.v6, sizeof(ctx->target.v6));
    } else {
        struct icmp_header *icmp = (struct icmp_header *)packet;
        memset(packet, 0, sizeof(packet));
        icmp->type = ICMP_ECHO;
        icmp->code = 0;
        icmp->id = getpid() & 0xFFFF;
        icmp->seq = ctx->ttl;
        icmp->checksum = in_cksum((unsigned short *)icmp, sizeof(packet));

        if (setsockopt(ctx->sockfd, IPPROTO_IP, IP_TTL, 
                      &ctx->ttl, sizeof(ctx->ttl)) < 0) {
            perror("Can't set TTL");
            return -1;
        }

        return sendto(ctx->sockfd, packet, sizeof(packet), 0,
                     (struct sockaddr*)&ctx->target.v4, sizeof(ctx->target.v4));
    }
}

static int recv_response(TraceRouteContext *ctx, char *host, char *ip) {
    union {
        struct sockaddr_in v4;
        struct sockaddr_in6 v6;
    } from;
    socklen_t from_len = ctx->is_ipv6 ? sizeof(from.v6) : sizeof(from.v4);
    char buf[512];
    int hlen;

    ssize_t len = recvfrom(ctx->sockfd, buf, sizeof(buf), 0,
                         // 添加显式类型转换
                         (struct sockaddr*)(ctx->is_ipv6 ? (void*)&from.v6 : (void*)&from.v4), 
                         &from_len);
    if (len < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recvfrom failed");
        }
        return -1;
    }

    if (ctx->is_ipv6) {
        inet_ntop(AF_INET6, &from.v6.sin6_addr, ip, INET6_ADDRSTRLEN - 1);
        ip[INET6_ADDRSTRLEN - 1] = '\0';
    } else {
        strncpy(ip, inet_ntoa(from.v4.sin_addr), INET_ADDRSTRLEN - 1);
        ip[INET_ADDRSTRLEN - 1] = '\0';
    }

    if (ctx->is_ipv6) {
        struct icmp6_header *icmp_hdr = (struct icmp6_header *)buf;
        if (icmp_hdr->type != ICMP6_TIME_EXCEEDED && icmp_hdr->type != ICMP6_ECHO_REPLY)
            return -1;

        if (getnameinfo((struct sockaddr*)&from.v6, sizeof(from.v6),
                       host, NI_MAXHOST, NULL, 0, 0) != 0) {
            strncpy(host, "*", NI_MAXHOST - 1);
            host[NI_MAXHOST - 1] = '\0';
        }

        return icmp_hdr->type;
    } else {
        struct ip *ip_hdr = (struct ip *)buf;
        hlen = ip_hdr->ip_hl << 2;

        if (ip_hdr->ip_p != IPPROTO_ICMP || len < hlen + ICMP_HEADER_LEN)
            return -1;

        struct icmp_header *icmp_hdr = (struct icmp_header *)(buf + hlen);

        if (icmp_hdr->type != ICMP_TIMXCEED && icmp_hdr->type != ICMP_ECHOREPLY)
            return -1;

        if (getnameinfo((struct sockaddr*)&from.v4, sizeof(from.v4),
                       host, NI_MAXHOST, NULL, 0, 0) != 0) {
            strncpy(host, "*", NI_MAXHOST - 1);
            host[NI_MAXHOST - 1] = '\0';
        }

        return icmp_hdr->type;
    }
}

void trace_route(const char *dest) {
    printf("\n=== Route to %s ===\n", dest);

    const char *host = dest;
    const char *protocol_pos = strstr(dest, "://");
    if (protocol_pos) {
        host = protocol_pos + 3;
    }

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_protocol = IPPROTO_ICMP;

    if (getaddrinfo(host, NULL, &hints, &res) != 0) {
        printf("Can't resolve host: %s\n", host);
        return;
    }

    TraceRouteContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    for (p = res; p != NULL; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            ctx.is_ipv6 = 0;
            memcpy(&ctx.target.v4, p->ai_addr, sizeof(ctx.target.v4));
            break;
        } else if (p->ai_family == AF_INET6) {
            ctx.is_ipv6 = 1;
            memcpy(&ctx.target.v6, p->ai_addr, sizeof(ctx.target.v6));
            break;
        }
    }

    if (p == NULL) {
        printf("No valid address found for %s\n", host);
        freeaddrinfo(res);
        return;
    }

    freeaddrinfo(res);

    ctx.ttl = 1;
    if ((ctx.sockfd = create_icmp_socket(ctx.is_ipv6)) < 0) return;

    while (ctx.ttl <= TR_MAX_HOPS) {
        char hostname[NI_MAXHOST] = {0};
        char ip[INET6_ADDRSTRLEN] = {0};
        int icmp_type = -1;

        gettimeofday(&ctx.start_time, NULL);

        if (send_probe(&ctx) < 0) break;
        icmp_type = recv_response(&ctx, hostname, ip);

        struct timeval end_time;
        gettimeofday(&end_time, NULL);
        double elapsed = (end_time.tv_sec - ctx.start_time.tv_sec) * 1000.0;
        elapsed += (end_time.tv_usec - ctx.start_time.tv_usec) / 1000.0;

        if (icmp_type != -1) {
            printf("%2d  %-15s  %-25s  %5.1f ms\n", ctx.ttl, ip,
                   (strcmp(hostname, "*") != 0) ? hostname : "", elapsed);

            if ((ctx.is_ipv6 && icmp_type == ICMP6_ECHO_REPLY) ||
                (!ctx.is_ipv6 && icmp_type == ICMP_ECHOREPLY)) {
                printf("Destination reached after %d hops\n", ctx.ttl);
                break;
            }
        } else {
            printf("%2d  %-15s\n", ctx.ttl, "***");
        }

        ctx.ttl++;
    }
    close(ctx.sockfd);
}

void trace_route_to_buffer(const char *dest, FILE *out) {
    // Similar modifications as trace_route function to support IPv6
    fprintf(out, "\n=== Route to %s ===\n", dest);
    
    const char *host = dest;
    const char *protocol_pos = strstr(dest, "://");
    if (protocol_pos) {
        host = protocol_pos + 3;
    }

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_protocol = IPPROTO_ICMP;

    if (getaddrinfo(host, NULL, &hints, &res) != 0) {
        fprintf(out, "Can't resolve host: %s\n", host);
        return;
    }

    TraceRouteContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    for (p = res; p != NULL; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            ctx.is_ipv6 = 0;
            memcpy(&ctx.target.v4, p->ai_addr, sizeof(ctx.target.v4));
            break;
        } else if (p->ai_family == AF_INET6) {
            ctx.is_ipv6 = 1;
            memcpy(&ctx.target.v6, p->ai_addr, sizeof(ctx.target.v6));
            break;
        }
    }

    if (p == NULL) {
        fprintf(out, "No valid address found for %s\n", host);
        freeaddrinfo(res);
        return;
    }

    freeaddrinfo(res);

    ctx.ttl = 1;
    if ((ctx.sockfd = create_icmp_socket(ctx.is_ipv6)) < 0) return;

    while (ctx.ttl <= TR_MAX_HOPS) {
        char hostname[NI_MAXHOST] = {0};
        char ip[INET6_ADDRSTRLEN] = {0};
        int icmp_type = -1;

        gettimeofday(&ctx.start_time, NULL);

        if (send_probe(&ctx) < 0) break;
        icmp_type = recv_response(&ctx, hostname, ip);

        struct timeval end_time;
        gettimeofday(&end_time, NULL);
        double elapsed = (end_time.tv_sec - ctx.start_time.tv_sec) * 1000.0;
        elapsed += (end_time.tv_usec - ctx.start_time.tv_usec) / 1000.0;

        if (icmp_type != -1) {
            fprintf(out, "%2d  %-15s  %-25s  %5.1f ms\n", ctx.ttl, ip,
                   (strcmp(hostname, "*") != 0) ? hostname : "", elapsed);

            if ((ctx.is_ipv6 && icmp_type == ICMP6_ECHO_REPLY) ||
                (!ctx.is_ipv6 && icmp_type == ICMP_ECHOREPLY)) {
                fprintf(out, "Destination reached after %d hops\n", ctx.ttl);
                break;
            }
        } else {
            fprintf(out, "%2d  %-15s\n", ctx.ttl, "***");
        }

        ctx.ttl++;
    }
    close(ctx.sockfd);
}

// 在函数原型部分添加
void get_public_ip(int ipv6);

// 在已有的print_local_ip函数后添加新函数
void get_public_ip(int ipv6) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "CURL init failed\n");
        return;
    }

    const char *api_url = ipv6 ? 
        "https://api64.ipify.org?format=json" : 
        "https://api.ipify.org?format=json";

    char ip[INET6_ADDRSTRLEN] = {0};
    char buffer[256] = {0};
    
    curl_easy_setopt(curl, CURLOPT_URL, api_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT_SEC);
    
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        // 简单解析JSON响应
        char *start = strchr(buffer, '"');
        if (start && (start = strchr(start+1, '"'))) {
            char *end = strchr(start+1, '"');
            if (end) {
                *end = '\0';
                strncpy(ip, start+1, sizeof(ip)-1);
            }
        }
        
        if (strlen(ip) > 0) {
            printf("%s Public IP: %s\n", ipv6 ? "IPv6" : "IPv4", ip);
        } else {
            printf("Failed to get %s public IP\n", ipv6 ? "IPv6" : "IPv4");
        }
    } else {
        fprintf(stderr, "IP query failed: %s\n", curl_easy_strerror(res));
    }
    
    curl_easy_cleanup(curl);
}

// 修改main函数中的本地IP打印部分
int main() {
    check_privileges();
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    print_local_ip();
    get_cpu_usage();
    sleep(1);
    double cpu_usage = get_cpu_usage();
    double memory_usage = get_memory_usage();
    
    printf("\n=== Network Latency Test ===\n");
    
    pthread_t threads[MAX_URLS];
    ThreadArg args[MAX_URLS];
    int thread_count = 0;

    for (int i = 0; china_domains[i]; i++) {
        // 注释掉原来的traceroute调用
        // trace_route(china_domains[i]);
        
        args[thread_count].url = china_domains[i];
        args[thread_count].valid = 0;
        if (pthread_create(&threads[thread_count], NULL, 
                         thread_test_latency, &args[thread_count]) != 0) {
            fprintf(stderr, "Failed to create thread for %s\n", china_domains[i]);
            continue;
        }
        thread_count++;
    }

    // 等待所有线程完成
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    // 处理结果
    double total_latency = 0;
    int valid_tests = 0;
    for (int i = 0; i < thread_count; i++) {
        // 先打印该URL的traceroute结果
        printf("\n%s", args[i].route_info);
        
        if (args[i].valid) {
            total_latency += args[i].result;
            valid_tests++;
        }
    }
    
    double avg_latency = (valid_tests > 0) ? (total_latency / valid_tests) : -1;
    
    int data_status = 2;
    if (cpu_usage >= 0 && memory_usage >= 0 && valid_tests > 0) {
        data_status = 0;
    } else if ((cpu_usage >= 0 || memory_usage >= 0 || valid_tests > 0)) {
        data_status = 1;
    }
    
    double availability = calculate_availability(cpu_usage, memory_usage, avg_latency, data_status);
    
    printf("\n=== System Availability ===\n");
    if (cpu_usage >= 0) {
        printf("CPU Usage:        %6.1f%%\n", cpu_usage);
    } else {
        printf("CPU Usage:        N/A\n");
    }
    
    if (memory_usage >= 0) {
        printf("Memory Usage:     %6.1f%%\n", memory_usage);
    } else {
        printf("Memory Usage:     N/A\n");
    }
    
    if (avg_latency >= 0) {
        // 修复这里：把total_tests改为thread_count
        printf("Avg Latency:      %5.1f ms (%d/%d)\n", avg_latency, valid_tests, thread_count);
    } else {
        printf("Avg Latency:      N/A\n");
    }
    
    printf("------------------------------\n");
    
    const char *status_msg[] = {
        "Based on complete data",
        "Based on partial data (conservative estimate)",
        "No data available (minimum estimate)"
    };
    printf("System Availability: %5.1f%% (%s)\n", availability, status_msg[data_status]);
    
    if (availability < 60.0) {
        printf("\nWARNING: Availability below 60%%, immediate attention needed!\n");
    } else if (availability < 80.0) {
        printf("\nNOTICE: Availability below 80%%, monitoring recommended\n");
    }
    
    curl_global_cleanup();
    return 0;
}
