#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <stdint.h>
#include "protocol.h"

// 统一消息接口
int send_message(int sockfd, const Message *msg);
int receive_message(int sockfd, Message *msg);

// 辅助函数
int connect_to_server(const char *host, uint16_t port);
int create_server_socket(uint16_t port);

#endif // NET_UTILS_H