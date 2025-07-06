#include "protocol.h"
#include "../include/logger.h"
#include <string.h>
#include <unistd.h>

int parse_message(const char *buffer, Message *msg) {
    if (buffer == NULL || msg == NULL) {
        LOG_ERROR("Invalid arguments");
        return -1;
    }
    memcpy(msg, buffer, sizeof(Message));
    return 0;
}

int validate_message(const Message *msg) {
    if (msg == NULL) {
        LOG_ERROR("Null message");
        return -1;
    }
    // 添加其他验证逻辑
    return 0;
}
