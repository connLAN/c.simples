#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/time.h>
#include <sys/queue.h>
#include "log.h"
#include <cjson/cJSON.h>

// IP statistics tracking structure
typedef struct ip_stats {
    char ip[INET6_ADDRSTRLEN];
    time_t first_seen;
    time_t last_seen;
    uint64_t total_bytes;
    uint64_t daily_bytes;
    uint64_t weekly_bytes;
    uint64_t monthly_bytes;
    int total_connections;
    int daily_connections;
    int weekly_connections;
    int monthly_connections;
    TAILQ_ENTRY(ip_stats) entries;
} ip_stats_t;

// Initialize IP statistics queue
TAILQ_HEAD(ip_stats_head, ip_stats);
static struct ip_stats_head ip_stats_list = TAILQ_HEAD_INITIALIZER(ip_stats_list);
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static time_t last_stats_save = 0;
static time_t last_cleanup = 0;

#define PORT 8080
#define MAX_CONN 5
#define BUFFER_SIZE 1024
#define STATS_FILE "ip_stats.json"
#define STATS_SAVE_INTERVAL 300 // 5 minutes
#define CLEANUP_INTERVAL 3600 // 1 hour
#define MAX_ENTRY_AGE 2592000 // 30 days

// Signal handler for graceful shutdown
static volatile sig_atomic_t shutdown_requested = 0;
static volatile sig_atomic_t status_requested = 0;

void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        shutdown_requested = 1;
    } else if (sig == SIGUSR1) {
        status_requested = 1;
    }
}

void save_ip_stats(void);
void cleanup_old_entries(void);
void print_ip_stats(void);

void update_ip_stats(const char* ip, size_t bytes) {
    pthread_mutex_lock(&stats_mutex);
    
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    
    // Find existing IP or create new entry
    ip_stats_t *entry = NULL;
    TAILQ_FOREACH(entry, &ip_stats_list, entries) {
        if (strcmp(entry->ip, ip) == 0) {
            break;
        }
    }
    
    if (!entry) {
        entry = malloc(sizeof(ip_stats_t));
        strncpy(entry->ip, ip, INET6_ADDRSTRLEN-1);
        entry->ip[INET6_ADDRSTRLEN-1] = '\0';
        entry->first_seen = now;
        entry->total_bytes = 0;
        entry->daily_bytes = 0;
        entry->weekly_bytes = 0;
        entry->monthly_bytes = 0;
        entry->total_connections = 0;
        entry->daily_connections = 0;
        entry->weekly_connections = 0;
        entry->monthly_connections = 0;
        TAILQ_INSERT_TAIL(&ip_stats_list, entry, entries);
    }
    
    // Update statistics
    entry->last_seen = now;
    entry->total_bytes += bytes;
    entry->daily_bytes += bytes;
    entry->weekly_bytes += bytes;
    entry->monthly_bytes += bytes;
    
    // Only count new connections (not subsequent messages)
    if (bytes == 0) {
        entry->total_connections++;
        entry->daily_connections++;
        entry->weekly_connections++;
        entry->monthly_connections++;
    }
    
    // Periodic save to JSON file
    if (now - last_stats_save > STATS_SAVE_INTERVAL) {
        save_ip_stats();
        last_stats_save = now;
    }
    
    // Periodic cleanup of old entries
    if (now - last_cleanup > CLEANUP_INTERVAL) {
        cleanup_old_entries();
        last_cleanup = now;
    }
    
    pthread_mutex_unlock(&stats_mutex);
}

void save_ip_stats() {
    pthread_mutex_lock(&stats_mutex);
    
    cJSON *root = cJSON_CreateArray();
    ip_stats_t *entry;
    
    TAILQ_FOREACH(entry, &ip_stats_list, entries) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ip", entry->ip);
        cJSON_AddNumberToObject(item, "first_seen", (double)entry->first_seen);
        cJSON_AddNumberToObject(item, "last_seen", (double)entry->last_seen);
        cJSON_AddNumberToObject(item, "total_bytes", (double)entry->total_bytes);
        cJSON_AddNumberToObject(item, "daily_bytes", (double)entry->daily_bytes);
        cJSON_AddNumberToObject(item, "weekly_bytes", (double)entry->weekly_bytes);
        cJSON_AddNumberToObject(item, "monthly_bytes", (double)entry->monthly_bytes);
        cJSON_AddNumberToObject(item, "total_connections", entry->total_connections);
        cJSON_AddNumberToObject(item, "daily_connections", entry->daily_connections);
        cJSON_AddNumberToObject(item, "weekly_connections", entry->weekly_connections);
        cJSON_AddNumberToObject(item, "monthly_connections", entry->monthly_connections);
        cJSON_AddItemToArray(root, item);
    }
    
    char *json_str = cJSON_Print(root);
    FILE *fp = fopen(STATS_FILE, "w");
    if (fp) {
        fputs(json_str, fp);
        fclose(fp);
    }
    
    free(json_str);
    cJSON_Delete(root);
    pthread_mutex_unlock(&stats_mutex);
}

void load_ip_stats() {
    FILE *fp = fopen(STATS_FILE, "r");
    if (!fp) return;
    
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char *data = malloc(len + 1);
    fread(data, 1, len, fp);
    data[len] = '\0';
    fclose(fp);
    
    cJSON *root = cJSON_Parse(data);
    if (!root) {
        free(data);
        return;
    }
    
    pthread_mutex_lock(&stats_mutex);
    
    int array_size = cJSON_GetArraySize(root);
    for (int i = 0; i < array_size; i++) {
        cJSON *item = cJSON_GetArrayItem(root, i);
        ip_stats_t *entry = malloc(sizeof(ip_stats_t));
        
        strncpy(entry->ip, cJSON_GetObjectItem(item, "ip")->valuestring, INET6_ADDRSTRLEN-1);
        entry->ip[INET6_ADDRSTRLEN-1] = '\0';
        entry->first_seen = (time_t)cJSON_GetObjectItem(item, "first_seen")->valuedouble;
        entry->last_seen = (time_t)cJSON_GetObjectItem(item, "last_seen")->valuedouble;
        entry->total_bytes = (uint64_t)cJSON_GetObjectItem(item, "total_bytes")->valuedouble;
        entry->daily_bytes = (uint64_t)cJSON_GetObjectItem(item, "daily_bytes")->valuedouble;
        entry->weekly_bytes = (uint64_t)cJSON_GetObjectItem(item, "weekly_bytes")->valuedouble;
        entry->monthly_bytes = (uint64_t)cJSON_GetObjectItem(item, "monthly_bytes")->valuedouble;
        entry->total_connections = cJSON_GetObjectItem(item, "total_connections")->valueint;
        entry->daily_connections = cJSON_GetObjectItem(item, "daily_connections")->valueint;
        entry->weekly_connections = cJSON_GetObjectItem(item, "weekly_connections")->valueint;
        entry->monthly_connections = cJSON_GetObjectItem(item, "monthly_connections")->valueint;
        
        TAILQ_INSERT_TAIL(&ip_stats_list, entry, entries);
    }
    
    pthread_mutex_unlock(&stats_mutex);
    cJSON_Delete(root);
    free(data);
}

void cleanup_old_entries() {
    pthread_mutex_lock(&stats_mutex);
    
    ip_stats_t *entry, *temp;
    time_t now = time(NULL);
    
    // Portable safe iteration
    entry = TAILQ_FIRST(&ip_stats_list);
    while (entry != NULL) {
        temp = TAILQ_NEXT(entry, entries);
        
        if (now - entry->last_seen > MAX_ENTRY_AGE) {
            TAILQ_REMOVE(&ip_stats_list, entry, entries);
            free(entry);
        }
        
        entry = temp;
    }
    
    pthread_mutex_unlock(&stats_mutex);
}

void print_ip_stats() {
    pthread_mutex_lock(&stats_mutex);
    
    printf("\n=== IP Connection Statistics ===\n");
    printf("%-20s %-10s %-10s %-10s %-10s %-10s %-10s %-10s\n", 
           "IP", "Total", "Daily", "Weekly", "Monthly", "T.Conn", "D.Conn", "M.Conn");
    
    ip_stats_t *entry;
    TAILQ_FOREACH(entry, &ip_stats_list, entries) {
        printf("%-20s %-10lu %-10lu %-10lu %-10lu %-10d %-10d %-10d\n", 
               entry->ip, 
               (unsigned long)entry->total_bytes,
               (unsigned long)entry->daily_bytes,
               (unsigned long)entry->weekly_bytes,
               (unsigned long)entry->monthly_bytes,
               entry->total_connections,
               entry->daily_connections,
               entry->monthly_connections);
    }
    
    pthread_mutex_unlock(&stats_mutex);
}

void handle_client(int client_socket, const char* client_ip, size_t bytes_received) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        LOG_ERROR("Failed to read from client %s", client_ip);
        close(client_socket);
        return;
    }
    buffer[bytes_read] = '\0';
    
    printf("Received from %s: %s\n", client_ip, buffer);
    
    // Update IP statistics
    update_ip_stats(client_ip, bytes_read);
    
    // Handle test message specifically
    if (strcmp(buffer, "TEST\n") == 0) {
        const char *response = "SERVER_OK\n";
        ssize_t sent = send(client_socket, response, strlen(response), 0);
        printf("Sent %zd bytes: %s", sent, response);
    } else {
        const char *response = "Message received by server\n";
        ssize_t sent = send(client_socket, response, strlen(response), 0);
        printf("Sent %zd bytes: %s", sent, response);
    }
    
    close(client_socket);
}

int main() {
    // Initialize logging system with DEBUG level
    log_set_level(LOG_DEBUG);
    LOG_INFO("Starting server with DEBUG logging");
    
    // Load existing statistics
    load_ip_stats();
    
    // Set up signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGUSR1, handle_signal);
    
    int server_fd, new_socket;
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
    address.sin_port = htons(PORT);
    
    // Bind socket to port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Start listening
    if (listen(server_fd, MAX_CONN) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on port %d\n", PORT);
    
    while (!shutdown_requested) {
        // Check for status display request
        if (status_requested) {
            status_requested = 0;
            print_ip_stats();
        }

        // Accept new connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            if (shutdown_requested) {
                break; // Exit on shutdown
            }
            perror("accept");
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);
        
        printf("Connection from %s\n", client_ip);
        handle_client(new_socket, client_ip, 0);
    }

    // Cleanup on shutdown
    save_ip_stats();
    LOG_INFO("Shutting down server...");
    close(server_fd);
    
    // Free all IP stats entries
    ip_stats_t *entry, *tmp;
    entry = TAILQ_FIRST(&ip_stats_list);
    while (entry != NULL) {
        tmp = TAILQ_NEXT(entry, entries);
        TAILQ_REMOVE(&ip_stats_list, entry, entries);
        free(entry);
        entry = tmp;
    }
    
    return 0;
}