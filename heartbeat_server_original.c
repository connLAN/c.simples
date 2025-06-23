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

// Global variables initialization
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

// Handle client connection
void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    
    char buffer[BUFFER_SIZE] = {0};
    ssize_t valread = read(client_socket, buffer, BUFFER_SIZE - 1);
    
    if (valread <= 0) {
        close(client_socket);
        return NULL;
    }
    
    buffer[valread] = '\0';
    printf("Received %zd bytes:\n%.*s\n", valread, (int)valread, buffer);
    
    if (buffer[0] == '{') {
        HeartbeatData hb_data = {0};
        if (process_json_data(buffer, &hb_data)) {
            printf("JSON Heartbeat Data:\n");
            printf("Local IP: %s\n", hb_data.local_ip);
            printf("Public IP: %s\n", hb_data.public_ip);
            printf("CPU Usage: %.2f%%\n", hb_data.cpu_usage);
            printf("Memory Usage: %.2f%%\n", hb_data.memory_usage);
            printf("Disk Usage: %.2f%%\n", hb_data.disk_usage);
            printf("Availability: %.2f%%\n", hb_data.availability);
            printf("Latency: %.2f ms\n", hb_data.latency);
            printf("------------------------\n");
            
            add_to_history(&hb_data);
            
            const char *response = "OK";
            send(client_socket, response, strlen(response), 0);
        } else {
            const char *response = "Invalid JSON data";
            send(client_socket, response, strlen(response), 0);
        }
        close(client_socket);
        return NULL;
    }
    
    if (strstr(buffer, "GET /api/heartbeat/history")) {
        char *history_json = get_history_json();
        if (history_json) {
            char http_response[BUFFER_SIZE];
            snprintf(http_response, BUFFER_SIZE,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Connection: close\r\n"
                "Content-Length: %zu\r\n\r\n"
                "%s", strlen(history_json), history_json);
            send(client_socket, http_response, strlen(http_response), 0);
            free(history_json);
        } else {
            const char *http_response = 
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n"
                "Content-Length: 21\r\n\r\n"
                "Failed to get history";
            send(client_socket, http_response, strlen(http_response), 0);
        }
        close(client_socket);
        return NULL;
    }
    else if (strstr(buffer, "POST /api/heartbeat")) {
        char *body = strstr(buffer, "\r\n\r\n");
        if (!body) {
            const char *response = "HTTP/1.1 400 Bad Request\r\n\r\n";
            send(client_socket, response, strlen(response), 0);
            close(client_socket);
            return NULL;
        }
        
        body += 4;
        HeartbeatData hb_data = {0};
        bool valid_data = false;
        
        if (*body == '{') {
            valid_data = process_json_data(body, &hb_data);
        } else {
            char *saveptr;
            char *pair = strtok_r(body, "&", &saveptr);
            int fields_set = 0;
            
            while (pair) {
                char key[MAX_KEY_LEN];
                char value[MAX_VALUE_LEN];
                
                if (parse_key_value(pair, key, sizeof(key), value, sizeof(value))) {
                    if (strcmp(key, "local_ip") == 0 && validate_ip(value)) {
                        strncpy(hb_data.local_ip, value, MAX_IP_LEN - 1);
                        fields_set++;
                    }
                    else if (strcmp(key, "public_ip") == 0 && validate_ip(value)) {
                        strncpy(hb_data.public_ip, value, MAX_IP_LEN - 1);
                        fields_set++;
                    }
                    else if (strcmp(key, "cpu_usage") == 0) {
                        double val = atof(value);
                        if (validate_percentage(val)) {
                            hb_data.cpu_usage = val;
                            fields_set++;
                        }
                    }
                    else if (strcmp(key, "memory_usage") == 0) {
                        double val = atof(value);
                        if (validate_percentage(val)) {
                            hb_data.memory_usage = val;
                            fields_set++;
                        }
                    }
                    else if (strcmp(key, "disk_usage") == 0) {
                        double val = atof(value);
                        if (validate_percentage(val)) {
                            hb_data.disk_usage = val;
                            fields_set++;
                        }
                    }
                    else if (strcmp(key, "availability") == 0) {
                        double val = atof(value);
                        if (validate_percentage(val)) {
                            hb_data.availability = val;
                            fields_set++;
                        }
                    }
                    else if (strcmp(key, "latency") == 0) {
                        double val = atof(value);
                        if (validate_latency(val)) {
                            hb_data.latency = val;
                            fields_set++;
                        }
                    }
                }
                pair = strtok_r(NULL, "&", &saveptr);
            }
            
            valid_data = (fields_set == 7);
        }
        
        if (valid_data) {
            printf("HTTP Heartbeat Data:\n");
            printf("Local IP: %s\n", hb_data.local_ip);
            printf("Public IP: %s\n", hb_data.public_ip);
            printf("CPU Usage: %.2f%%\n", hb_data.cpu_usage);
            printf("Memory Usage: %.2f%%\n", hb_data.memory_usage);
            printf("Disk Usage: %.2f%%\n", hb_data.disk_usage);
            printf("Availability: %.2f%%\n", hb_data.availability);
            printf("Latency: %.2f ms\n", hb_data.latency);
            printf("------------------------\n");
            
            add_to_history(&hb_data);
            
            const char *http_response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n"
                "Content-Length: 2\r\n\r\n"
                "OK";
            send(client_socket, http_response, strlen(http_response), 0);
        } else {
            const char *http_response = 
                "HTTP/1.1 400 Bad Request\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n"
                "Content-Length: 16\r\n\r\n"
                "Invalid data sent";
            send(client_socket, http_response, strlen(http_response), 0);
        }
    }
    else {
        const char *http_response = 
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Connection: close\r\n"
            "Content-Length: 9\r\n\r\n"
            "Not Found";
        send(client_socket, http_response, strlen(http_response), 0);
    }
    
    close(client_socket);
    return NULL;
}

// Function to run TCP server
void *run_tcp_server(void *arg) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("TCP socket failed");
        return NULL;
    }
    
    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("TCP setsockopt");
        close(server_fd);
        return NULL;
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(TCP_PORT);
    
    // Bind the socket to the port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("TCP bind failed");
        close(server_fd);
        return NULL;
    }
    
    // Start listening
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("TCP listen");
        close(server_fd);
        return NULL;
    }
    
    printf("TCP Server listening on port %d...\n", TCP_PORT);
    
    while (1) {
        int *new_socket = malloc(sizeof(int));
        if (!new_socket) {
            perror("malloc failed");
            continue;
        }
        
        if ((*new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("TCP accept");
            free(new_socket);
            continue;
        }
        
        printf("New TCP connection from %s:%d\n", 
               inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void*)new_socket) < 0) {
            perror("could not create thread for TCP client");
            close(*new_socket);
            free(new_socket);
        }
        
        // Detach the thread so we don't need to join it
        pthread_detach(thread_id);
    }
    
    return NULL;
}

// Function to run HTTP server
void *run_http_server(void *arg) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("HTTP socket failed");
        return NULL;
    }
    
    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("HTTP setsockopt");
        close(server_fd);
        return NULL;
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(HTTP_PORT);
    
    // Bind the socket to the port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("HTTP bind failed");
        close(server_fd);
        return NULL;
    }
    
    // Start listening
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("HTTP listen");
        close(server_fd);
        return NULL;
    }
    
    printf("HTTP Server listening on port %d...\n", HTTP_PORT);
    
    while (1) {
        int *new_socket = malloc(sizeof(int));
        if (!new_socket) {
            perror("malloc failed");
            continue;
        }
        
        if ((*new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("HTTP accept");
            free(new_socket);
            continue;
        }
        
        printf("New HTTP connection from %s:%d\n", 
               inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void*)new_socket) < 0) {
            perror("could not create thread for HTTP client");
            close(*new_socket);
            free(new_socket);
        }
        
        // Detach the thread so we don't need to join it
        pthread_detach(thread_id);
    }
    
    return NULL;
}

int main() {
    pthread_t http_thread, tcp_thread;
    
    // Start HTTP server thread
    if (pthread_create(&http_thread, NULL, run_http_server, NULL) != 0) {
        perror("Failed to create HTTP server thread");
        exit(EXIT_FAILURE);
    }
    
    // Start TCP server thread
    if (pthread_create(&tcp_thread, NULL, run_tcp_server, NULL) != 0) {
        perror("Failed to create TCP server thread");
        exit(EXIT_FAILURE);
    }
    
    printf("Server started with HTTP port %d and TCP port %d\n", HTTP_PORT, TCP_PORT);
    
    // Wait for server threads
    pthread_join(http_thread, NULL);
    pthread_join(tcp_thread, NULL);
    
    return 0;
}