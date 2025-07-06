#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <stdint.h>
#include "protocol.h"

// 统一消息接口
int send_message(int sockfd, const Message *msg);
int receive_message(int sockfd, Message *msg);
int recv_header(int sockfd, MessageHeader *header);
int recv_payload(int sockfd, void *buf, size_t len);

// 辅助函数
int create_client_socket(const char *host, uint16_t port);
int connect_to_server(const char *host, uint16_t port);
int create_server_socket(uint16_t port);
int set_socket_timeout(int sockfd, int timeout_sec);

#endif // NET_UTILS_H
