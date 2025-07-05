/**
 * @file worker.h
 * @brief Worker header file for distributed worker system
 */

#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>

/* Worker configuration */
typedef struct {
    char server_ip[16];
    int server_port;
    char worker_ip[16];
    int worker_port;
    int max_concurrent_jobs;
    int reconnect_interval_seconds;
    int heartbeat_interval_seconds;
    int job_types[MAX_JOB_TYPES];
    int num_job_types;
} WorkerConfig;

/* Worker structure */
typedef struct {
    WorkerConfig config;
    int worker_id;
    int server_socket;
    int is_running;
    int is_connected;
    pthread_t heartbeat_thread;
    pthread_t job_thread;
    pthread_mutex_t lock;
} Worker;

/**
 * @brief Initialize worker
 * 
 * @param config Worker configuration
 * @return int 0 on success, negative value on error
 */
int worker_init(const WorkerConfig *config);

/**
 * @brief Start worker
 * 
 * @return int 0 on success, negative value on error
 */
int worker_start();

/**
 * @brief Stop worker
 * 
 * @return int 0 on success, negative value on error
 */
int worker_stop();

/**
 * @brief Connect to server
 * 
 * @return int 0 on success, negative value on error
 */
int worker_connect_to_server();

/**
 * @brief Disconnect from server
 * 
 * @return int 0 on success, negative value on error
 */
int worker_disconnect_from_server();

/**
 * @brief Register worker with server
 * 
 * @return int 0 on success, negative value on error
 */
int worker_register();

/**
 * @brief Send heartbeat to server
 * 
 * @return int 0 on success, negative value on error
 */
int worker_send_heartbeat();

/**
 * @brief Request job from server
 * 
 * @return int Job ID on success, negative value on error
 */
int worker_request_job();

/**
 * @brief Process job
 * 
 * @param job_id Job ID
 * @param job_type Job type
 * @param input_data Job input data
 * @param input_size Job input data size
 * @param result_data Pointer to store result data
 * @param result_size Pointer to store result data size
 * @return int 0 on success, negative value on error
 */
int worker_process_job(int job_id, int job_type, const char *input_data, size_t input_size,
                      char **result_data, size_t *result_size);

/**
 * @brief Send job completion to server
 * 
 * @param job_id Job ID
 * @param result_data Job result data
 * @param result_size Job result data size
 * @param processing_time_ms Processing time in milliseconds
 * @return int 0 on success, negative value on error
 */
int worker_send_job_completion(int job_id, const char *result_data, size_t result_size,
                              long long processing_time_ms);

/**
 * @brief Send job failure to server
 * 
 * @param job_id Job ID
 * @param error_code Error code
 * @return int 0 on success, negative value on error
 */
int worker_send_job_failure(int job_id, int error_code);

/**
 * @brief Get worker statistics
 * 
 * @param jobs_processed Pointer to store number of jobs processed
 * @param jobs_failed Pointer to store number of jobs failed
 * @param avg_processing_time_ms Pointer to store average processing time in milliseconds
 * @return int 0 on success, negative value on error
 */
int worker_get_stats(int *jobs_processed, int *jobs_failed, double *avg_processing_time_ms);

#endif /* WORKER_H */