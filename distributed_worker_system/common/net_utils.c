#include "net_utils.h"
#include <unistd.h>
#include <arpa/inet.h>

int send_message(int sockfd, const Message *msg) {
    return write(sockfd, msg, sizeof(Message));
}

int receive_message(int sockfd, Message *msg) {
    return read(sockfd, msg, sizeof(Message));
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