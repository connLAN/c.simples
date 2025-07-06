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
    MSG_TYPE_HEARTBEAT = 0,  // 心跳消息类型
    MSG_TYPE_CLIENT_CONNECT = 1,
    MSG_TYPE_CLIENT_CONNECT_ACK = 2,
    MSG_TYPE_CLIENT_DISCONNECT = 3,
    MSG_TYPE_SUBMIT_JOB = 10,
    MSG_TYPE_JOB_SUBMITTED = 11,
    MSG_TYPE_GET_JOB_STATUS = 12,
    MSG_TYPE_JOB_STATUS = 13,
    MSG_TYPE_GET_JOB_RESULT = 14,
    MSG_TYPE_JOB_RESULT = 15,
    MSG_TYPE_GET_SERVER_STATS = 20,
    MSG_TYPE_SERVER_STATS = 21,
    MSG_TYPE_ERROR = 30,
    MSG_TYPE_REGISTER = 70,
    MSG_TYPE_REGISTER_RESPONSE = 71,
    MSG_TYPE_HEARTBEAT_RESPONSE = 72,
    MSG_TYPE_TASK = 80,
    MSG_TYPE_TASK_RESULT = 81,
    MSG_TYPE_RESULT_ACK = 82,
    MSG_TYPE_SHUTDOWN = 90
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
    
    struct {
        uint32_t status;
    } reg_response;
} MessageBody;

/* Constants */
#define MAX_DATA_SIZE 1024  /* Maximum size of task data */

// 任务状态枚举
typedef enum {
    STATUS_SUCCESS = 0,
    STATUS_ERROR = 1
} TaskStatus;

// 任务结构体
typedef struct {
    uint32_t task_id;
    uint8_t input_data[1024];
    uint8_t output_data[1024];
    uint32_t input_size;
    uint32_t output_size;
} task_t;

// 任务结果结构体
typedef struct {
    uint32_t task_id;
    TaskStatus status;
    uint32_t exec_time_ms;
} task_result_t;

// 完整消息结构
typedef struct {
    MessageHeader header;
    MessageBody body;
} Message;

#endif // PROTOCOL_H
