/**
 * @file client.h
 * @brief Client header file for distributed worker system
 */

#ifndef CLIENT_H
#define CLIENT_H

#include <pthread.h>

/* Client configuration */
typedef struct {
    char server_ip[16];
    int server_port;
    int reconnect_interval_seconds;
} ClientConfig;

/* Client structure */
typedef struct {
    ClientConfig config;
    int client_id;
    int server_socket;
    int is_connected;
    pthread_mutex_t lock;
} Client;

/**
 * @brief Initialize client
 * 
 * @param config Client configuration
 * @return int 0 on success, negative value on error
 */
int client_init(const ClientConfig *config);

/**
 * @brief Cleanup client resources
 * 
 * @return int 0 on success, negative value on error
 */
int client_cleanup();

/**
 * @brief Connect to server
 * 
 * @return int 0 on success, negative value on error
 */
int client_connect_to_server();

/**
 * @brief Disconnect from server
 * 
 * @return int 0 on success, negative value on error
 */
int client_disconnect_from_server();

/**
 * @brief Submit job to server
 * 
 * @param job_type Job type
 * @param input_data Job input data
 * @param input_size Job input data size
 * @return int Job ID on success, negative value on error
 */
int client_submit_job(int job_type, const char *input_data, size_t input_size);

/**
 * @brief Get job status
 * 
 * @param job_id Job ID
 * @return int Job status (JOB_STATUS_*) on success, negative value on error
 */
int client_get_job_status(int job_id);

/**
 * @brief Get job result
 * 
 * @param job_id Job ID
 * @param result_data Pointer to store result data
 * @param result_size Pointer to store result data size
 * @return int 0 on success, negative value on error
 */
int client_get_job_result(int job_id, char **result_data, size_t *result_size);

/**
 * @brief Wait for job completion
 * 
 * @param job_id Job ID
 * @param timeout_seconds Timeout in seconds (0 for no timeout)
 * @return int 0 on success, negative value on error
 */
int client_wait_for_job(int job_id, int timeout_seconds);

/**
 * @brief Get server statistics
 * 
 * @param active_clients Pointer to store number of active clients
 * @param active_workers Pointer to store number of active workers
 * @param pending_jobs Pointer to store number of pending jobs
 * @param running_jobs Pointer to store number of running jobs
 * @param completed_jobs Pointer to store number of completed jobs
 * @param failed_jobs Pointer to store number of failed jobs
 * @return int 0 on success, negative value on error
 */
int client_get_server_stats(int *active_clients, int *active_workers,
                           int *pending_jobs, int *running_jobs,
                           int *completed_jobs, int *failed_jobs);

#endif /* CLIENT_H */