#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <time.h>
#include <unistd.h>

// 作业状态枚举
typedef enum {
    JOB_STATUS_PENDING = 0,
    JOB_STATUS_RUNNING,
    JOB_STATUS_COMPLETED, 
    JOB_STATUS_FAILED,
    JOB_STATUS_TIMEOUT
} JobStatus;

// 消息类型枚举
typedef enum {
    MSG_TYPE_CLIENT_CONNECT = 1,
    MSG_TYPE_CLIENT_CONNECT_ACK,
    MSG_TYPE_CLIENT_DISCONNECT,
    MSG_TYPE_SUBMIT_JOB,
    MSG_TYPE_JOB_SUBMITTED,
    MSG_TYPE_GET_JOB_STATUS,
    MSG_TYPE_JOB_STATUS,
    MSG_TYPE_GET_JOB_RESULT,
    MSG_TYPE_JOB_RESULT,
    MSG_TYPE_GET_SERVER_STATS,
    MSG_TYPE_SERVER_STATS,
    MSG_TYPE_ERROR
} MessageType;

// 消息头结构
typedef struct {
    MessageType message_type;
    uint32_t client_id;
} MessageHeader;

// 消息体联合
typedef union {
    struct {
        uint32_t client_id;
    } client_connect_ack;
    
    struct {
        uint32_t job_type;
        uint8_t data[1024];
        uint32_t data_size;
    } submit_job;
    
    struct {
        uint32_t job_id;
    } job_submitted;
    
    struct {
        uint32_t job_id;
        JobStatus status;
    } job_status;
    
    struct {
        uint32_t job_id;
        uint8_t result_data[1024];
        uint32_t result_size;
    } job_result;
    
    struct {
        uint32_t active_clients;
        uint32_t active_workers;
        uint32_t pending_jobs;
        uint32_t running_jobs;
        uint32_t completed_jobs;
        uint32_t failed_jobs;
    } server_stats;
    
    struct {
        uint32_t error_code;
        char error_message[256];
    } error;
} MessageBody;

// 完整消息结构
typedef struct {
    MessageHeader header;
    MessageBody body;
} Message;

#endif // PROTOCOL_H