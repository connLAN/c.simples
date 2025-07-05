/**
 * @file server.h
 * @brief Server header file for distributed worker system
 */

#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>
#include "../common/protocol.h"

/* Server configuration */
typedef struct {
    char ip_address[16];
    int port;
    int max_clients;
    int max_workers;
    int max_jobs;
    int job_timeout_seconds;
} ServerConfig;

/* Server state */
typedef struct {
    int server_socket;
    int is_running;
    pthread_t accept_thread;
    pthread_mutex_t lock;
    ServerConfig config;
} Server;

/**
 * @brief Initialize server with given configuration
 * 
 * @param config Server configuration
 * @return int 0 on success, negative value on error
 */
int server_init(const ServerConfig *config);

/**
 * @brief Start server and begin accepting connections
 * 
 * @return int 0 on success, negative value on error
 */
int server_start();

/**
 * @brief Stop server and cleanup resources
 * 
 * @return int 0 on success, negative value on error
 */
int server_stop();

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
int server_get_stats(int *active_clients, int *active_workers, 
                     int *pending_jobs, int *running_jobs, 
                     int *completed_jobs, int *failed_jobs);

/**
 * @brief Handle a client message
 * 
 * @param client_socket Client socket
 * @param msg Message received from client
 * @return int 0 on success, negative value on error
 */
int server_handle_client_message(int client_socket, const Message *msg);

/**
 * @brief Handle a worker message
 * 
 * @param worker_socket Worker socket
 * @param msg Message received from worker
 * @return int 0 on success, negative value on error
 */
int server_handle_worker_message(int worker_socket, const Message *msg);

#endif /* SERVER_H */