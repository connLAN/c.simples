/**
 * comm_protocol.h - Communication Protocol
 * 
 * This file defines the communication protocol between central server and worker servers.
 */

#ifndef COMM_PROTOCOL_H
#define COMM_PROTOCOL_H

#include <stdint.h>

/* Maximum data size for task input/output */
#define MAX_DATA_SIZE 4096

/* Message types */
#define MSG_TYPE_REGISTER           1  /* Worker registration request */
#define MSG_TYPE_REGISTER_RESPONSE  2  /* Worker registration response */
#define MSG_TYPE_HEARTBEAT          3  /* Worker heartbeat */
#define MSG_TYPE_HEARTBEAT_RESPONSE 4  /* Heartbeat response */
#define MSG_TYPE_TASK               5  /* Task assignment */
#define MSG_TYPE_TASK_RESULT        6  /* Task result */
#define MSG_TYPE_RESULT_ACK         7  /* Result acknowledgment */
#define MSG_TYPE_SHUTDOWN           8  /* Shutdown request */
#define MSG_TYPE_CLIENT_SUBMIT      9  /* Client task submission */
#define MSG_TYPE_CLIENT_RESULT     10  /* Client task result notification */
#define MSG_TYPE_CLIENT_QUERY      11  /* Client task status query */
#define MSG_TYPE_CLIENT_QUERY_RESP 12  /* Client task status response */

/* Status codes */
#define STATUS_SUCCESS              0  /* Operation successful */
#define STATUS_ERROR                1  /* General error */
#define STATUS_TIMEOUT              2  /* Operation timed out */
#define STATUS_INVALID_TASK         3  /* Invalid task */
#define STATUS_BUSY                 4  /* Worker is busy */

/* Message header structure */
typedef struct {
    uint8_t msg_type;    /* Message type */
    uint32_t id;         /* Message ID (task ID or worker ID) */
    uint32_t data_size;  /* Size of payload data */
} msg_header_t;

/* Task structure */
typedef struct {
    uint32_t task_id;                /* Unique task identifier */
    uint8_t input_data[MAX_DATA_SIZE]; /* Task input data */
    uint8_t output_data[MAX_DATA_SIZE]; /* Task output data */
} task_t;

/* Task result structure */
typedef struct {
    uint32_t task_id;      /* Task identifier */
    uint8_t status;        /* Result status code */
    uint32_t exec_time_ms; /* Execution time in milliseconds */
} task_result_t;

/* Client task submission structure */
typedef struct {
    uint32_t client_id;               /* Client identifier */
    uint8_t priority;                 /* Task priority (0-255, higher value means higher priority) */
    uint32_t data_size;               /* Size of input data */
    uint8_t input_data[MAX_DATA_SIZE]; /* Task input data */
} client_task_submission_t;

/* Client task query structure */
typedef struct {
    uint32_t client_id;    /* Client identifier */
    uint32_t task_id;      /* Task identifier */
} client_task_query_t;

/* Client task status structure */
typedef struct {
    uint32_t task_id;      /* Task identifier */
    uint8_t status;        /* Task status code */
    uint8_t completed;     /* 1 if task is completed, 0 otherwise */
    uint32_t exec_time_ms; /* Execution time in milliseconds (if completed) */
    uint32_t data_size;    /* Size of output data (if completed) */
    uint8_t output_data[MAX_DATA_SIZE]; /* Task output data (if completed) */
} client_task_status_t;

#endif /* COMM_PROTOCOL_H */