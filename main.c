#include "heartbeat_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

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