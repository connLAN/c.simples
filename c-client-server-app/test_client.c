#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

int main() {
    pid_t server_pid = fork();
    
    if (server_pid == 0) {
        // Child process - run server
        execl("./server", "./server", NULL);
        perror("execl failed");
        exit(1);
    } else if (server_pid > 0) {
        // Parent process - test client
        sleep(1); // Wait for server to start
        
        printf("=== Testing server status ===\n");
        kill(server_pid, SIGUSR1);
        sleep(1);
        
        printf("\n=== Testing raw TCP connection ===\n");
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket creation failed");
            return 1;
        }

        struct sockaddr_in servaddr;
        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(8080);
        servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
            perror("connection failed");
            close(sockfd);
            return 1;
        }
        
        printf("✓ TCP connection established\n");
        
        // Test simple message exchange
        char buffer[1024];
        const char *test_msg = "TEST\n";
        write(sockfd, test_msg, strlen(test_msg));
        ssize_t bytes_read = read(sockfd, buffer, sizeof(buffer)-1);
        if (bytes_read > 0) {
            // Only print the exact bytes received (excluding any null terminator)
            printf("Server response: %.*s\n", (int)bytes_read, buffer);
        } else {
            printf("No response from server\n");
        }
        
        close(sockfd);
        
        printf("\n=== Stopping server ===\n");
        kill(server_pid, SIGTERM);
        wait(NULL);
        printf("Test complete\n");
    } else {
        perror("fork failed");
        return 1;
    }
    
    return 0;
}