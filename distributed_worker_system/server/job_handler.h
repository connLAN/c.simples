/**
 * @file job_handler.h
 * @brief Job handler header file for distributed worker system
 */

#ifndef JOB_HANDLER_H
#define JOB_HANDLER_H

#include <pthread.h>
#include "../common/protocol.h"

/* Job status */
#define JOB_STATUS_PENDING    0
#define JOB_STATUS_ASSIGNED   1
#define JOB_STATUS_COMPLETED  2
#define JOB_STATUS_FAILED     3
#define JOB_STATUS_TIMEOUT    4

/* Job structure */
typedef struct {
    int job_id;
    int client_socket;
    int worker_socket;
    int job_type;
    char *input_data;
    size_t input_size;
    char *result_data;
    size_t result_size;
    int status;
    long long submit_time;
    long long start_time;
    long long end_time;
    int retry_count;
} Job;

/**
 * @brief Initialize job handler
 * 
 * @param max_jobs Maximum number of jobs to handle
 * @param job_timeout_seconds Timeout for jobs in seconds
 * @return int 0 on success, negative value on error
 */
int job_handler_init(int max_jobs, int job_timeout_seconds);

/**
 * @brief Cleanup job handler resources
 * 
 * @return int 0 on success, negative value on error
 */
int job_handler_cleanup();

/**
 * @brief Submit a new job
 * 
 * @param client_socket Client socket
 * @param job_type Job type
 * @param input_data Input data
 * @param input_size Size of input data
 * @return int Job ID on success, negative value on error
 */
int job_handler_submit(int client_socket, int job_type, 
                      const char *input_data, size_t input_size);

/**
 * @brief Assign a job to a worker
 * 
 * @param worker_socket Worker socket
 * @return int Job ID on success, negative value if no jobs available
 */
int job_handler_assign(int worker_socket);

/**
 * @brief Complete a job with results
 * 
 * @param job_id Job ID
 * @param result_data Result data
 * @param result_size Size of result data
 * @return int 0 on success, negative value on error
 */
int job_handler_complete(int job_id, const char *result_data, size_t result_size);

/**
 * @brief Mark a job as failed
 * 
 * @param job_id Job ID
 * @param error_code Error code
 * @return int 0 on success, negative value on error
 */
int job_handler_fail(int job_id, int error_code);

/**
 * @brief Get job status
 * 
 * @param job_id Job ID
 * @return int Job status or negative value on error
 */
int job_handler_get_status(int job_id);

/**
 * @brief Get job result
 * 
 * @param job_id Job ID
 * @param result_data Pointer to store result data
 * @param result_size Pointer to store result size
 * @return int 0 on success, negative value on error
 */
int job_handler_get_result(int job_id, char **result_data, size_t *result_size);

/**
 * @brief Check for timed out jobs and handle them
 * 
 * @return int Number of timed out jobs handled
 */
int job_handler_check_timeouts();

/**
 * @brief Get job statistics
 * 
 * @param pending_jobs Pointer to store number of pending jobs
 * @param running_jobs Pointer to store number of running jobs
 * @param completed_jobs Pointer to store number of completed jobs
 * @param failed_jobs Pointer to store number of failed jobs
 * @return int 0 on success, negative value on error
 */
int job_handler_get_stats(int *pending_jobs, int *running_jobs, 
                         int *completed_jobs, int *failed_jobs);

#endif /* JOB_HANDLER_H */