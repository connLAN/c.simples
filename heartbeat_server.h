#ifndef HEARTBEAT_SERVER_H
#define HEARTBEAT_SERVER_H

#include <stdbool.h>
#include <time.h>
#include <netinet/in.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 4096
#define MAX_KEY_LEN 64
#define MAX_VALUE_LEN 128
#define MAX_IP_LEN INET_ADDRSTRLEN
#define HTTP_PORT 8080
#define TCP_PORT 8081
#define MAX_HEARTBEATS 100

typedef struct {
    char local_ip[MAX_IP_LEN];
    char public_ip[MAX_IP_LEN];
    double cpu_usage;
    double memory_usage;
    double disk_usage;
    double availability;
    double latency;
} HeartbeatData;

// Structure for storing heartbeat history
typedef struct HeartbeatNode {
    HeartbeatData data;
    time_t timestamp;
    struct HeartbeatNode *next;
} HeartbeatNode;

// Global variables for heartbeat history (extern for testing)
extern HeartbeatNode *heartbeat_history;
extern pthread_mutex_t history_mutex;
extern int heartbeat_count;

// Function declarations
void add_to_history(HeartbeatData *data);
void clear_heartbeat_history(void);
char* get_history_json();
bool process_json_data(const char *json_str, HeartbeatData *hb_data);
void url_decode(char *dst, const char *src, size_t dst_size);
bool parse_key_value(const char *pair, char *key, size_t key_len, char *value, size_t value_len);
bool validate_ip(const char *ip);
bool validate_percentage(double value);
bool validate_latency(double value);
void *handle_client(void *arg);
void *run_tcp_server(void *arg);
void *run_http_server(void *arg);

#endif /* HEARTBEAT_SERVER_H */