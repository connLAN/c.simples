#ifndef SYSINFO_H
#define SYSINFO_H

#include <stdio.h>
#include <curl/curl.h>

// 颜色代码定义
#define COLOR_RESET   "\x1B[0m"
#define COLOR_RED     "\x1B[31m"
#define COLOR_GREEN   "\x1B[32m"
#define COLOR_YELLOW  "\x1B[33m"
#define COLOR_BLUE    "\x1B[34m"
#define COLOR_MAGENTA "\x1B[35m"
#define COLOR_CYAN    "\x1B[36m"
#define COLOR_WHITE   "\x1B[37m"

// ResponseData结构体定义
typedef struct {
    char *data;
    size_t size;
} ResponseData;

// 回调函数声明
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);

// JSON解析函数声明
void extract_json_field(const char *json, const char *field, char *buffer, size_t bufsize);

// 工具函数声明
void format_bytes(unsigned long bytes, char *buffer);

// 系统信息获取函数声明
void get_system_info(void);
void get_memory_info(void);
void get_disk_info(void);
void get_network_info(void);
void get_public_ip(void);

#endif /* SYSINFO_H */