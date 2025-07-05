/**
 * @file worker_manager.h
 * @brief Worker manager header file for distributed worker system
 */

#ifndef WORKER_MANAGER_H
#define WORKER_MANAGER_H

#include <pthread.h>

/* Worker status */
#define WORKER_STATUS_IDLE       0
#define WORKER_STATUS_BUSY       1
#define WORKER_STATUS_OFFLINE    2

/* Worker structure */
typedef struct {
    int worker_id;
    int socket;
    int status;
    int current_job_id;
    char ip_address[16];
    int port;
    long long last_heartbeat;
    int job_types_supported[MAX_JOB_TYPES];
    int job_count;
    int failed_job_count;
    long long total_processing_time;
} Worker;

/**
 * @brief Initialize worker manager
 * 
 * @param max_workers Maximum number of workers to manage
 * @return int 0 on success, negative value on error
 */
int worker_manager_init(int max_workers);

/**
 * @brief Cleanup worker manager resources
 * 
 * @return int 0 on success, negative value on error
 */
int worker_manager_cleanup();

/**
 * @brief Register a new worker
 * 
 * @param socket Worker socket
 * @param ip_address Worker IP address
 * @param port Worker port
 * @param job_types_supported Array of supported job types
 * @param num_job_types Number of supported job types
 * @return int Worker ID on success, negative value on error
 */
int worker_manager_register(int socket, const char *ip_address, int port,
                           const int *job_types_supported, int num_job_types);

/**
 * @brief Unregister a worker
 * 
 * @param worker_id Worker ID
 * @return int 0 on success, negative value on error
 */
int worker_manager_unregister(int worker_id);

/**
 * @brief Find a worker by socket
 * 
 * @param socket Worker socket
 * @return int Worker ID on success, negative value if not found
 */
int worker_manager_find_by_socket(int socket);

/**
 * @brief Update worker status
 * 
 * @param worker_id Worker ID
 * @param status New status
 * @return int 0 on success, negative value on error
 */
int worker_manager_update_status(int worker_id, int status);

/**
 * @brief Update worker heartbeat
 * 
 * @param worker_id Worker ID
 * @return int 0 on success, negative value on error
 */
int worker_manager_update_heartbeat(int worker_id);

/**
 * @brief Assign job to worker
 * 
 * @param worker_id Worker ID
 * @param job_id Job ID
 * @return int 0 on success, negative value on error
 */
int worker_manager_assign_job(int worker_id, int job_id);

/**
 * @brief Complete job for worker
 * 
 * @param worker_id Worker ID
 * @param processing_time_ms Processing time in milliseconds
 * @return int 0 on success, negative value on error
 */
int worker_manager_complete_job(int worker_id, long long processing_time_ms);

/**
 * @brief Fail job for worker
 * 
 * @param worker_id Worker ID
 * @return int 0 on success, negative value on error
 */
int worker_manager_fail_job(int worker_id);

/**
 * @brief Find available worker for job type
 * 
 * @param job_type Job type
 * @return int Worker ID on success, negative value if no worker available
 */
int worker_manager_find_available(int job_type);

/**
 * @brief Check for inactive workers and handle them
 * 
 * @param timeout_seconds Timeout in seconds
 * @return int Number of inactive workers handled
 */
int worker_manager_check_inactive(int timeout_seconds);

/**
 * @brief Get worker statistics
 * 
 * @param total_workers Pointer to store total number of workers
 * @param idle_workers Pointer to store number of idle workers
 * @param busy_workers Pointer to store number of busy workers
 * @return int 0 on success, negative value on error
 */
int worker_manager_get_stats(int *total_workers, int *idle_workers, int *busy_workers);

#endif /* WORKER_MANAGER_H */