#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 4096
#define MAX_KEY_LEN 64
#define MAX_VALUE_LEN 128
#define MAX_IP_LEN INET_ADDRSTRLEN
#define HTTP_PORT 8080

typedef struct {
    char local_ip[MAX_IP_LEN];
    char public_ip[MAX_IP_LEN];
    double cpu_usage;
    double memory_usage;
    double disk_usage;
    double availability;
    double latency;
} HeartbeatData;

// Function to URL decode a string
void url_decode(char *dst, const char *src, size_t dst_size) {
    char a, b;
    size_t i = 0;
    size_t j = 0;
    
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '%') {
            if (isxdigit(src[i+1]) && isxdigit(src[i+2])) {
                a = src[i+1];
                b = src[i+2];
                a = a - (a >= 'A' ? 'A' - 10 : (a >= 'a' ? 'a' - 10 : '0'));
                b = b - (b >= 'A' ? 'A' - 10 : (b >= 'a' ? 'a' - 10 : '0'));
                dst[j++] = 16 * a + b;
                i += 3;
            } else {
                dst[j++] = src[i++];
            }
        } else if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
}

// Improved function to parse a single key-value pair
bool parse_key_value(const char *pair, char *key, size_t key_len, 
                    char *value, size_t value_len) {
    const char *eq = strchr(pair, '=');
    if (!eq || eq == pair) return false;
    
    size_t pair_key_len = eq - pair;
    if (pair_key_len >= key_len) return false;
    
    strncpy(key, pair, pair_key_len);
    key[pair_key_len] = '\0';
    
    char decoded_value[MAX_VALUE_LEN];
    url_decode(decoded_value, eq + 1, sizeof(decoded_value));
    
    if (strlen(decoded_value) >= value_len) return false;
    strncpy(value, decoded_value, value_len - 1);
    value[value_len - 1] = '\0';
    
    return true;
}

// Function to validate IP address format
bool validate_ip(const char *ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip, &(sa.sin_addr)) != 0;
}

// Function to validate percentage values
bool validate_percentage(double value) {
    return value >= 0.0 && value <= 100.0;
}

// Function to validate latency
bool validate_latency(double value) {
    return value >= 0.0;
}

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg); // Free the allocated socket immediately after use
    
    char buffer[BUFFER_SIZE] = {0};
    ssize_t valread = read(client_socket, buffer, BUFFER_SIZE - 1);
    
    if (valread <= 0) {
        close(client_socket);
        return NULL;
    }
    
    // Null-terminate the received data
    buffer[valread] = '\0';
    
    // Debug: print the raw request
    printf("Received %zd bytes:\n%.*s\n", valread, (int)valread, buffer);
    
    // Check for POST request
    if (strstr(buffer, "POST /api/heartbeat")) {
        // Find the start of POST data
        char *post_data = strstr(buffer, "\r\n\r\n");
        if (post_data) {
            post_data += 4;
            
            // Debug: print the raw POST data
            printf("Raw POST data: %s\n", post_data);
            
            HeartbeatData hb_data = {0};
            bool valid_data = true;
            
            char *saveptr;
            char *token = strtok_r(post_data, "&", &saveptr);
            while (token && valid_data) {
                char key[MAX_KEY_LEN];
                char value[MAX_VALUE_LEN];
                
                if (!parse_key_value(token, key, sizeof(key), value, sizeof(value))) {
                    printf("Failed to parse key-value pair: %s\n", token);
                    token = strtok_r(NULL, "&", &saveptr);
                    continue;
                }
                
                if (strcmp(key, "local_ip") == 0) {
                    if (validate_ip(value)) {
                        strncpy(hb_data.local_ip, value, sizeof(hb_data.local_ip) - 1);
                    } else {
                        printf("Invalid local IP: %s\n", value);
                        valid_data = false;
                    }
                }
                else if (strcmp(key, "public_ip") == 0) {
                    if (validate_ip(value)) {
                        strncpy(hb_data.public_ip, value, sizeof(hb_data.public_ip) - 1);
                    } else {
                        printf("Invalid public IP: %s\n", value);
                        valid_data = false;
                    }
                }
                else if (strcmp(key, "cpu_usage") == 0) {
                    hb_data.cpu_usage = atof(value);
                    if (!validate_percentage(hb_data.cpu_usage)) {
                        printf("Invalid CPU usage: %.2f\n", hb_data.cpu_usage);
                        valid_data = false;
                    }
                }
                else if (strcmp(key, "memory_usage") == 0) {
                    hb_data.memory_usage = atof(value);
                    if (!validate_percentage(hb_data.memory_usage)) {
                        printf("Invalid memory usage: %.2f\n", hb_data.memory_usage);
                        valid_data = false;
                    }
                }
                else if (strcmp(key, "disk_usage") == 0) {
                    hb_data.disk_usage = atof(value);
                    if (!validate_percentage(hb_data.disk_usage)) {
                        printf("Invalid disk usage: %.2f\n", hb_data.disk_usage);
                        valid_data = false;
                    }
                }
                else if (strcmp(key, "availability") == 0) {
                    hb_data.availability = atof(value);
                    if (!validate_percentage(hb_data.availability)) {
                        printf("Invalid availability: %.2f\n", hb_data.availability);
                        valid_data = false;
                    }
                }
                else if (strcmp(key, "latency") == 0) {
                    hb_data.latency = atof(value);
                    if (!validate_latency(hb_data.latency)) {
                        printf("Invalid latency: %.2f\n", hb_data.latency);
                        valid_data = false;
                    }
                }
                
                token = strtok_r(NULL, "&", &saveptr);
            }
            
            if (valid_data) {
                // Print the parsed data
                printf("Heartbeat Data:\n");
                printf("Local IP: %s\n", hb_data.local_ip);
                printf("Public IP: %s\n", hb_data.public_ip);
                printf("CPU Usage: %.2f%%\n", hb_data.cpu_usage);
                printf("Memory Usage: %.2f%%\n", hb_data.memory_usage);
                printf("Disk Usage: %.2f%%\n", hb_data.disk_usage);
                printf("Availability: %.2f%%\n", hb_data.availability);
                printf("Latency: %.2f ms\n", hb_data.latency);
                printf("------------------------\n");
                
                // Send success response
                const char *http_response = 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/plain\r\n"
                    "Connection: close\r\n"
                    "Content-Length: 2\r\n\r\n"
                    "OK";
                send(client_socket, http_response, strlen(http_response), 0);
            } else {
                // Send validation error response
                const char *http_response = 
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Type: text/plain\r\n"
                    "Connection: close\r\n"
                    "Content-Length: 21\r\n\r\n"
                    "Invalid Data Received";
                send(client_socket, http_response, strlen(http_response), 0);
            }
            
            close(client_socket);
            return NULL;
        }
    }
    
    // Send error response for invalid requests
    const char *http_response = 
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "Content-Length: 12\r\n\r\n"
        "Bad Request";
    send(client_socket, http_response, strlen(http_response), 0);
    
    close(client_socket);
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(HTTP_PORT);
    
    // Bind the socket to the port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Start listening
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on port %d...\n", HTTP_PORT);
    
    while (1) {
        int *new_socket = malloc(sizeof(int));
        if (!new_socket) {
            perror("malloc failed");
            continue;
        }
        
        if ((*new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            free(new_socket);
            continue;
        }
        
        printf("New connection from %s:%d\n", 
               inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void*)new_socket) < 0) {
            perror("could not create thread");
            close(*new_socket);
            free(new_socket);
        }
        
        // Detach the thread so we don't need to join it
        pthread_detach(thread_id);
    }
    
    return 0;
}