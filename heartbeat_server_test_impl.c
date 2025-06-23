#include "heartbeat_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <json-c/json.h>

// Global variables initialization for testing
HeartbeatNode *heartbeat_history = NULL;
pthread_mutex_t history_mutex = PTHREAD_MUTEX_INITIALIZER;
int heartbeat_count = 0;

// Validation functions
bool validate_ip(const char *ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip, &(sa.sin_addr)) != 0;
}

bool validate_percentage(double value) {
    return value >= 0.0 && value <= 100.0;
}

bool validate_latency(double value) {
    return value >= 0.0;
}

// URL decoding function
void url_decode(char *dst, const char *src, size_t dst_size) {
    size_t i = 0;
    size_t j = 0;
    
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '%' && isxdigit(src[i + 1]) && isxdigit(src[i + 2])) {
            char hex[3] = {src[i + 1], src[i + 2], 0};
            dst[j] = strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dst[j] = ' ';
            i++;
        } else {
            dst[j] = src[i];
            i++;
        }
        j++;
    }
    dst[j] = '\0';
}

// Parse key-value pairs
bool parse_key_value(const char *pair, char *key, size_t key_len, char *value, size_t value_len) {
    const char *eq = strchr(pair, '=');
    if (!eq) return false;
    
    size_t key_size = eq - pair;
    if (key_size >= key_len) return false;
    
    strncpy(key, pair, key_size);
    key[key_size] = '\0';
    
    const char *val_start = eq + 1;
    size_t val_size = strlen(val_start);
    if (val_size >= value_len) return false;
    
    url_decode(value, val_start, value_len);
    return true;
}

// Process JSON data
bool process_json_data(const char *json_str, HeartbeatData *hb_data) {
    struct json_object *parsed_json;
    struct json_object *local_ip;
    struct json_object *public_ip;
    struct json_object *cpu_usage;
    struct json_object *memory_usage;
    struct json_object *disk_usage;
    struct json_object *availability;
    struct json_object *latency;
    
    parsed_json = json_tokener_parse(json_str);
    if (!parsed_json) return false;
    
    if (!json_object_object_get_ex(parsed_json, "local_ip", &local_ip) ||
        !json_object_object_get_ex(parsed_json, "public_ip", &public_ip) ||
        !json_object_object_get_ex(parsed_json, "cpu_usage", &cpu_usage) ||
        !json_object_object_get_ex(parsed_json, "memory_usage", &memory_usage) ||
        !json_object_object_get_ex(parsed_json, "disk_usage", &disk_usage) ||
        !json_object_object_get_ex(parsed_json, "availability", &availability) ||
        !json_object_object_get_ex(parsed_json, "latency", &latency)) {
        json_object_put(parsed_json);
        return false;
    }
    
    const char *local_ip_str = json_object_get_string(local_ip);
    const char *public_ip_str = json_object_get_string(public_ip);
    
    if (!validate_ip(local_ip_str) || !validate_ip(public_ip_str)) {
        json_object_put(parsed_json);
        return false;
    }
    
    strncpy(hb_data->local_ip, local_ip_str, MAX_IP_LEN - 1);
    strncpy(hb_data->public_ip, public_ip_str, MAX_IP_LEN - 1);
    
    hb_data->cpu_usage = json_object_get_double(cpu_usage);
    hb_data->memory_usage = json_object_get_double(memory_usage);
    hb_data->disk_usage = json_object_get_double(disk_usage);
    hb_data->availability = json_object_get_double(availability);
    hb_data->latency = json_object_get_double(latency);
    
    if (!validate_percentage(hb_data->cpu_usage) ||
        !validate_percentage(hb_data->memory_usage) ||
        !validate_percentage(hb_data->disk_usage) ||
        !validate_percentage(hb_data->availability) ||
        !validate_latency(hb_data->latency)) {
        json_object_put(parsed_json);
        return false;
    }
    
    json_object_put(parsed_json);
    return true;
}

// Add heartbeat data to history
void add_to_history(HeartbeatData *data) {
    HeartbeatNode *new_node = malloc(sizeof(HeartbeatNode));
    if (!new_node) return;
    
    memcpy(&new_node->data, data, sizeof(HeartbeatData));
    new_node->timestamp = time(NULL);
    
    pthread_mutex_lock(&history_mutex);
    
    new_node->next = heartbeat_history;
    heartbeat_history = new_node;
    
    heartbeat_count++;
    
    // Remove oldest entry if we exceed MAX_HEARTBEATS
    if (heartbeat_count > MAX_HEARTBEATS) {
        HeartbeatNode *current = heartbeat_history;
        HeartbeatNode *prev = NULL;
        
        while (current->next) {
            prev = current;
            current = current->next;
        }
        
        if (prev) {
            prev->next = NULL;
            free(current);
            heartbeat_count--;
        }
    }
    
    pthread_mutex_unlock(&history_mutex);
}

// Get history as JSON
char* get_history_json() {
    struct json_object *json_array = json_object_new_array();
    if (!json_array) return NULL;
    
    pthread_mutex_lock(&history_mutex);
    
    HeartbeatNode *current = heartbeat_history;
    while (current) {
        struct json_object *entry = json_object_new_object();
        
        json_object_object_add(entry, "local_ip", 
                             json_object_new_string(current->data.local_ip));
        json_object_object_add(entry, "public_ip", 
                             json_object_new_string(current->data.public_ip));
        json_object_object_add(entry, "cpu_usage", 
                             json_object_new_double(current->data.cpu_usage));
        json_object_object_add(entry, "memory_usage", 
                             json_object_new_double(current->data.memory_usage));
        json_object_object_add(entry, "disk_usage", 
                             json_object_new_double(current->data.disk_usage));
        json_object_object_add(entry, "availability", 
                             json_object_new_double(current->data.availability));
        json_object_object_add(entry, "latency", 
                             json_object_new_double(current->data.latency));
        json_object_object_add(entry, "timestamp", 
                             json_object_new_int64(current->timestamp));
        
        json_object_array_add(json_array, entry);
        current = current->next;
    }
    
    pthread_mutex_unlock(&history_mutex);
    
    const char *json_str = json_object_to_json_string_ext(json_array, 
                                                         JSON_C_TO_STRING_PRETTY);
    char *result = strdup(json_str);
    
    json_object_put(json_array);
    return result;
}