/**
 * @file protocol.h
 * @brief Protocol definitions for distributed worker system
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

/* Maximum size for job data */
#define MAX_JOB_DATA_SIZE 4096

/* Maximum number of jobs in a list */
#define MAX_JOBS_IN_LIST 100

/* Message types */
#define MSG_SUBMIT_JOB       1  /* Client submits a job */
#define MSG_JOB_ACCEPTED     2  /* Server accepts a job */
#define MSG_GET_JOB_STATUS   3  /* Client requests job status */
#define MSG_JOB_STATUS       4  /* Server responds with job status */
#define MSG_GET_JOB_RESULT   5  /* Client requests job result */
#define MSG_JOB_RESULT       6  /* Server responds with job result */
#define MSG_LIST_JOBS        7  /* Client requests job list */
#define MSG_JOB_LIST         8  /* Server responds with job list */
#define MSG_ASSIGN_JOB       9  /* Server assigns job to worker */
#define MSG_JOB_COMPLETED    10 /* Worker completes job */
#define MSG_WORKER_REGISTER  11 /* Worker registers with server */
#define MSG_WORKER_ACCEPTED  12 /* Server accepts worker */
#define MSG_WORKER_HEARTBEAT 13 /* Worker sends heartbeat */
#define MSG_HEARTBEAT_ACK    14 /* Server acknowledges heartbeat */

/* Job status codes */
typedef enum {
    JOB_STATUS_PENDING = 0,   /* Job is pending */
    JOB_STATUS_ASSIGNED = 1,  /* Job is assigned to a worker */
    JOB_STATUS_COMPLETED = 2, /* Job is completed */
    JOB_STATUS_FAILED = 3     /* Job failed */
} JobStatus;

/* Job information structure */
typedef struct {
    int id;                  /* Job ID */
    int type;                /* Job type */
    JobStatus status;        /* Job status */
    int worker_id;           /* Worker ID (if assigned) */
    time_t submit_time;      /* Job submission time */
    time_t complete_time;    /* Job completion time (if completed) */
} JobInfo;

/* Job structure */
typedef struct {
    int type;                           /* Job type */
    size_t data_size;                   /* Size of job data */
    char data[MAX_JOB_DATA_SIZE];       /* Job data */
} Job;

/* Message structure */
typedef struct {
    int type;                           /* Message type */
    int job_id;                         /* Job ID */
    int worker_id;                      /* Worker ID */
    JobStatus status;                   /* Job status */
    Job job;                            /* Job data */
    int job_count;                      /* Number of jobs in list */
    JobInfo jobs[MAX_JOBS_IN_LIST];     /* Job list */
} Message;

/**
 * @brief Send a message over a socket
 * 
 * @param sockfd Socket descriptor
 * @param msg Message to send
 * @return int 0 on success, -1 on failure
 */
int send_message(int sockfd, const Message *msg);

/**
 * @brief Receive a message from a socket
 * 
 * @param sockfd Socket descriptor
 * @param msg Buffer to store received message
 * @return int 0 on success, -1 on failure
 */
int receive_message(int sockfd, Message *msg);

/**
 * @brief Convert job status to string
 * 
 * @param status Job status
 * @return const char* String representation of job status
 */
const char *job_status_to_string(JobStatus status);

#endif /* PROTOCOL_H */