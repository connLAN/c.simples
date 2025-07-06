#include "net_utils.h"
#include "protocol.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/socket.h>

int send_message(int sockfd, const Message *msg) {
    Message net_msg = *msg;
    // 转换头字段为网络字节序
    net_msg.header.message_type = htonl(net_msg.header.message_type);
    net_msg.header.client_id = htonl(net_msg.header.client_id);
    net_msg.header.client_id = htonl(net_msg.header.client_id);
    
    ssize_t total = 0;
    ssize_t bytes_left = sizeof(Message);
    const char *ptr = (const char *)&net_msg;
    
    while (bytes_left > 0) {
        ssize_t n = write(sockfd, ptr + total, bytes_left);
        if (n <= 0) return -1;
        total += n;
        bytes_left -= n;
    }
    
    return 0;
}

int receive_message(int sockfd, Message *msg) {
    ssize_t total = 0;
    ssize_t bytes_left = sizeof(Message);
    char *ptr = (char *)msg;
    
    while (bytes_left > 0) {
        ssize_t n = read(sockfd, ptr + total, bytes_left);
        if (n <= 0) return -1;
        total += n;
        bytes_left -= n;
    }
    
    // 转换头字段为主机字节序
    msg->header.message_type = ntohl(msg->header.message_type);
    msg->header.client_id = ntohl(msg->header.client_id);
    msg->header.client_id = ntohl(msg->header.client_id);
    
    return 0;
}

int recv_header(int sockfd, MessageHeader *header) {
    ssize_t total = 0;
    ssize_t bytes_left = sizeof(MessageHeader);
    char *ptr = (char *)header;
    
    while (bytes_left > 0) {
        ssize_t n = read(sockfd, ptr + total, bytes_left);
        if (n <= 0) return -1;
        total += n;
        bytes_left -= n;
    }
    
    return 0;
}

int recv_payload(int sockfd, void *buf, size_t len) {
    ssize_t total = 0;
    ssize_t bytes_left = len;
    char *ptr = (char *)buf;
    
    while (bytes_left > 0) {
        ssize_t n = read(sockfd, ptr + total, bytes_left);
        if (n <= 0) return -1;
        total += n;
        bytes_left -= n;
    }
    
    return 0;
}

int set_socket_timeout(int sockfd, int timeout_sec) {
    struct timeval tv = {
        .tv_sec = timeout_sec,
        .tv_usec = 0
    };
    return setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

int create_client_socket(const char *host, uint16_t port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = inet_addr(host)
    };

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int create_server_socket(uint16_t port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return -1;

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 10) < 0) {
        close(server_fd);
        return -1;
    }

    return server_fd;
}