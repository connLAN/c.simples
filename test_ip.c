#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

// 定义ResponseData结构体
typedef struct {
    char *data;
    size_t size;
} ResponseData;

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

// 实现get_public_ip函数
void get_public_ip() {
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize CURL\n");
        return;
    }

    printf("\n=== Public IP Information ===\n");
    
    ResponseData response = {0};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    
    // 尝试使用ipinfo.io API（更可靠）
    curl_easy_setopt(curl, CURLOPT_URL, "https://ipinfo.io/json");
    
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK && response.data) {
        printf("API Response: %s\n\n", response.data);
        
        char buffer[256];
        
        // 提取IP地址
        extract_json_field(response.data, "ip", buffer, sizeof(buffer));
        printf("IP Address: %s\n", buffer);
        
        printf("\nLocation Information:\n");
        printf("------------------\n");
        
        // 提取国家信息
        extract_json_field(response.data, "country", buffer, sizeof(buffer));
        printf("Country:  %s\n", buffer);
        
        // 提取地区信息
        extract_json_field(response.data, "region", buffer, sizeof(buffer));
        printf("Region:   %s\n", buffer);
        
        // 提取城市信息
        extract_json_field(response.data, "city", buffer, sizeof(buffer));
        printf("City:     %s\n", buffer);
        
        // 提取位置信息
        extract_json_field(response.data, "loc", buffer, sizeof(buffer));
        printf("Location: %s\n", buffer);
        
        // 提取时区信息
        extract_json_field(response.data, "timezone", buffer, sizeof(buffer));
        printf("Timezone: %s\n", buffer);
        
        // 提取组织/ISP信息
        extract_json_field(response.data, "org", buffer, sizeof(buffer));
        printf("ISP/Org:  %s\n", buffer);
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
            printf("API Response: %s\n\n", response.data);
            
            char buffer[256];
            
            // 提取IP地址
            extract_json_field(response.data, "query", buffer, sizeof(buffer));
            printf("IP Address: %s\n", buffer);
            
            printf("\nLocation Information:\n");
            printf("------------------\n");
            
            // 提取国家信息
            extract_json_field(response.data, "country", buffer, sizeof(buffer));
            printf("Country:  %s\n", buffer);
            
            // 提取地区信息
            extract_json_field(response.data, "regionName", buffer, sizeof(buffer));
            printf("Region:   %s\n", buffer);
            
            // 提取城市信息
            extract_json_field(response.data, "city", buffer, sizeof(buffer));
            printf("City:     %s\n", buffer);
            
            // 提取时区信息
            extract_json_field(response.data, "timezone", buffer, sizeof(buffer));
            printf("Timezone: %s\n", buffer);
            
            // 提取ISP信息
            extract_json_field(response.data, "isp", buffer, sizeof(buffer));
            printf("ISP:      %s\n", buffer);
        } else {
            fprintf(stderr, "All IP API queries failed\n");
        }
    }
    
    free(response.data);
    curl_easy_cleanup(curl);
}

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    get_public_ip();
    curl_global_cleanup();
    return 0;
}