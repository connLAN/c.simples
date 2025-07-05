/**
 * net_utils.c - Network Utilities Implementation
 */

#include "net_utils.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* Create a server socket */
int create_server_socket(uint16_t port, int backlog) {
    int sockfd;
    struct sockaddr_in server_addr;
    int opt = 1;
    
    /* Create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_ERROR("Failed to create socket: %s", strerror(errno));
        return -1;
    }
    
    /* Set socket options */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_ERROR("Failed to set socket options: %s", strerror(errno));
        close(sockfd);
        return -1;
    }
    
    /* Initialize server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);
    
    /* Bind socket to address */
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("Failed to bind socket: %s", strerror(errno));
        close(sockfd);
        return -1;
    }
    
    /* Listen for connections */
    if (listen(sockfd, backlog) < 0) {
        LOG_ERROR("Failed to listen on socket: %s", strerror(errno));
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

/* Create a client socket and connect to server */
int create_client_socket(const char *server_ip, uint16_t server_port) {
    int sockfd;
    struct sockaddr_in server_addr;
    
    /* Create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_ERROR("Failed to create socket: %s", strerror(errno));
        return -1;
    }
    
    /* Initialize server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    
    /* Convert IP address from text to binary form */
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        LOG_ERROR("Invalid address: %s", server_ip);
        close(sockfd);
        return -1;
    }
    
    /* Connect to server */
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("Failed to connect to server: %s", strerror(errno));
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

/* Set socket timeout */
int set_socket_timeout(int sockfd, int timeout_sec) {
    struct timeval tv;
    
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        LOG_ERROR("Failed to set receive timeout: %s", strerror(errno));
        return -1;
    }
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        LOG_ERROR("Failed to set send timeout: %s", strerror(errno));
        return -1;
    }
    
    return 0;
}

/* Helper function to ensure all data is sent */
static int send_all(int sockfd, const void *buffer, size_t length) {
    const char *ptr = (const char *)buffer;
    size_t bytes_sent = 0;
    int n;
    
    while (bytes_sent < length) {
        n = send(sockfd, ptr + bytes_sent, length - bytes_sent, 0);
        if (n <= 0) {
            if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
                continue; /* Retry on interruption or would block */
            }
            return -1; /* Error or connection closed */
        }
        bytes_sent += n;
    }
    
    return bytes_sent;
}

/* Helper function to ensure all data is received */
static int recv_all(int sockfd, void *buffer, size_t length) {
    char *ptr = (char *)buffer;
    size_t bytes_received = 0;
    int n;
    
    while (bytes_received < length) {
        n = recv(sockfd, ptr + bytes_received, length - bytes_received, 0);
        if (n <= 0) {
            if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
                continue; /* Retry on interruption or would block */
            }
            return -1; /* Error or connection closed */
        }
        bytes_received += n;
    }
    
    return bytes_received;
}

/* Send a message (header + payload) */
int send_message(int sockfd, uint8_t msg_type, uint32_t id, 
                const void *data, uint32_t data_size) {
    msg_header_t header;
    int result;
    
    /* Initialize header */
    header.msg_type = msg_type;
    header.id = htonl(id);
    header.data_size = htonl(data_size);
    
    /* Send header */
    result = send_all(sockfd, &header, sizeof(header));
    if (result < 0) {
        LOG_ERROR("Failed to send message header: %s", strerror(errno));
        return -1;
    }
    
    /* Send payload if any */
    if (data != NULL && data_size > 0) {
        result = send_all(sockfd, data, data_size);
        if (result < 0) {
            LOG_ERROR("Failed to send message payload: %s", strerror(errno));
            return -1;
        }
    }
    
    return sizeof(header) + data_size;
}

/* Receive message header */
int recv_header(int sockfd, msg_header_t *header) {
    int result;
    
    if (header == NULL) {
        return -1;
    }
    
    /* Receive header */
    result = recv_all(sockfd, header, sizeof(msg_header_t));
    if (result < 0) {
        LOG_ERROR("Failed to receive message header: %s", strerror(errno));
        return -1;
    }
    
    /* Convert from network byte order */
    header->id = ntohl(header->id);
    header->data_size = ntohl(header->data_size);
    
    return 0;
}

/* Receive message payload */
int recv_payload(int sockfd, void *buffer, uint32_t size) {
    int result;
    
    if (buffer == NULL || size == 0) {
        return -1;
    }
    
    /* Receive payload */
    result = recv_all(sockfd, buffer, size);
    if (result < 0) {
        LOG_ERROR("Failed to receive message payload: %s", strerror(errno));
        return -1;
    }
    
    return result;
}