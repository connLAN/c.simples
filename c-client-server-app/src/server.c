#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "utils.h"
#include "log.h"

volatile sig_atomic_t reload_requested = 0;

void handle_sighup(int sig) {
    reload_requested = 1;
}

#define PORT 8080
#define MAX_CONN 5
#define BUFFER_SIZE 1024

// Function declarations (implemented in utils.c)
int is_ip_blacklisted(const char *ip);
int is_ip_whitelisted(const char *ip);

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    read(client_socket, buffer, BUFFER_SIZE);
    printf("Received: %s\n", buffer);
    
    const char *response = "Message received by server";
    send(client_socket, response, strlen(response), 0);
    close(client_socket);
}

int main() {
    // Initialize logging system
    log_set_level(LOG_INFO);
    LOG_INFO("Starting server");
    
    // Initialize IP lists
    init_ip_lists();
    
    // Setup SIGHUP handler for dynamic reload
    struct sigaction sa;
    sa.sa_handler = handle_sighup;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGHUP, &sa, NULL);
    LOG_INFO("SIGHUP handler registered for IP list reload");
    
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
    
    while (1) {
        // Accept new connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);
        
        printf("Connection from %s\n", client_ip);
        
        // Check IP against blacklist/whitelist
        if (is_ip_blacklisted(client_ip)) {
        printf("Debug: IP %s is blacklisted\n", client_ip);
            printf("Blocked connection from blacklisted IP: %s\n", client_ip);
            close(new_socket);
            continue;
        }
        
        handle_client(new_socket);
    }
    
    return 0;
}