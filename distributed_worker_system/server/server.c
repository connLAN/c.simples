/**
 * @file server.c
 * @brief Server implementation for distributed worker system
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>

#include "server.h"
#include "job_handler.h"
#include "worker_manager.h"
#include "../common/logger.h"
#include "../common/net_utils.h"
#include "../common/protocol.h"
#include "../common/common.h"

/* Global server instance */
static Server g_server;

/* Forward declarations for internal functions */
static void *accept_thread_func(void *arg);
static int handle_client_connection(int client_socket);
static int handle_worker_connection(int worker_socket);
static int send_error_response(int socket, int error_code, const char *error_msg);

/**
 * Accept thread function that handles incoming connections
 */
static void *accept_thread_func(void *arg) {
    log_info("Accept thread started");
    
    while (1) {
        /* Check if server is still running */
        pthread_mutex_lock(&g_server.lock);
        int is_running = g_server.is_running;
        pthread_mutex_unlock(&g_server.lock);
        
        if (!is_running) {
            log_info("Accept thread stopping");
            break;
        }
        
        /* Accept new connection */
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_socket = accept(g_server.server_socket, 
                                  (struct sockaddr *)&client_addr, 
                                  &client_addr_len);
        
        if (client_socket < 0) {
            /* Check if server is stopping */
            if (!is_running) {
                break;
            }
            
            log_error("Failed to accept connection: %s", strerror(errno));
            continue;
        }
        
        /* Get client IP address */
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        log_info("New connection from %s:%d", client_ip, ntohs(client_addr.sin_port));
        
        /* Receive initial message to determine client type */
        Message msg;
        if (receive_message(client_socket, &msg) < 0) {
            log_error("Failed to receive initial message from %s", client_ip);
            close(client_socket);
            continue;
        }
        
        /* Handle connection based on message type */
        if (msg.header.message_type == MSG_TYPE_CLIENT_CONNECT) {
            /* Spawn a new thread to handle client connection */
            pthread_t client_thread;
            int *socket_ptr = malloc(sizeof(int));
            if (!socket_ptr) {
                log_error("Failed to allocate memory for client socket");
                close(client_socket);
                continue;
            }
            
            *socket_ptr = client_socket;
            
            if (pthread_create(&client_thread, NULL, 
                              (void *(*)(void *))handle_client_connection, 
                              socket_ptr) != 0) {
                log_error("Failed to create client thread: %s", strerror(errno));
                free(socket_ptr);
                close(client_socket);
                continue;
            }
            
            /* Detach thread to automatically clean up when it exits */
            pthread_detach(client_thread);
            
            log_info("Client connection handler started for %s", client_ip);
        } 
        else if (msg.header.message_type == MSG_TYPE_WORKER_CONNECT) {
            /* Spawn a new thread to handle worker connection */
            pthread_t worker_thread;
            int *socket_ptr = malloc(sizeof(int));
            if (!socket_ptr) {
                log_error("Failed to allocate memory for worker socket");
                close(client_socket);
                continue;
            }
            
            *socket_ptr = client_socket;
            
            if (pthread_create(&worker_thread, NULL, 
                              (void *(*)(void *))handle_worker_connection, 
                              socket_ptr) != 0) {
                log_error("Failed to create worker thread: %s", strerror(errno));
                free(socket_ptr);
                close(client_socket);
                continue;
            }
            
            /* Detach thread to automatically clean up when it exits */
            pthread_detach(worker_thread);
            
            log_info("Worker connection handler started for %s", client_ip);
        } 
        else {
            log_error("Invalid initial message type: %d", msg.header.message_type);
            send_error_response(client_socket, ERR_INVALID_MESSAGE, "Invalid initial message");
            close(client_socket);
        }
    }
    
    log_info("Accept thread exited");
    return NULL;
}

/**
 * Handle client connection in a separate thread
 */
static int handle_client_connection(int client_socket) {
    int *socket_ptr = (int *)&client_socket;
    int socket = *socket_ptr;
    free(socket_ptr);
    
    /* Send connection acknowledgment */
    Message response;
    memset(&response, 0, sizeof(Message));
    response.header.message_type = MSG_TYPE_CLIENT_CONNECT_ACK;
    
    if (send_message(socket, &response) < 0) {
        log_error("Failed to send connection acknowledgment");
        close(socket);
        return -1;
    }
    
    /* Handle client messages */
    while (1) {
        /* Check if server is still running */
        pthread_mutex_lock(&g_server.lock);
        int is_running = g_server.is_running;
        pthread_mutex_unlock(&g_server.lock);
        
        if (!is_running) {
            log_info("Client handler stopping due to server shutdown");
            break;
        }
        
        /* Receive message */
        Message msg;
        int result = receive_message(socket, &msg);
        
        if (result == 0) {
            log_info("Client disconnected");
            break;
        } 
        else if (result < 0) {
            log_error("Error receiving message from client: %s", strerror(errno));
            break;
        }
        
        /* Handle message */
        if (server_handle_client_message(socket, &msg) < 0) {
            log_error("Error handling client message");
            break;
        }
    }
    
    /* Cleanup */
    close(socket);
    log_info("Client handler exited");
    return 0;
}

/**
 * Handle worker connection in a separate thread
 */
static int handle_worker_connection(int worker_socket) {
    int *socket_ptr = (int *)&worker_socket;
    int socket = *socket_ptr;
    free(socket_ptr);
    
    /* Send connection acknowledgment */
    Message response;
    memset(&response, 0, sizeof(Message));
    response.header.message_type = MSG_TYPE_WORKER_CONNECT_ACK;
    
    if (send_message(socket, &response) < 0) {
        log_error("Failed to send connection acknowledgment");
        close(socket);
        return -1;
    }
    
    /* Handle worker messages */
    while (1) {
        /* Check if server is still running */
        pthread_mutex_lock(&g_server.lock);
        int is_running = g_server.is_running;
        pthread_mutex_unlock(&g_server.lock);
        
        if (!is_running) {
            log_info("Worker handler stopping due to server shutdown");
            break;
        }
        
        /* Receive message */
        Message msg;
        int result = receive_message(socket, &msg);
        
        if (result == 0) {
            log_info("Worker disconnected");
            
            /* Find worker ID by socket */
            int worker_id = worker_manager_find_by_socket(socket);
            if (worker_id >= 0) {
                /* Unregister worker */
                worker_manager_unregister(worker_id);
                log_info("Worker %d unregistered due to disconnect", worker_id);
            }
            
            break;
        } 
        else if (result < 0) {
            log_error("Error receiving message from worker: %s", strerror(errno));
            break;
        }
        
        /* Handle message */
        if (server_handle_worker_message(socket, &msg) < 0) {
            log_error("Error handling worker message");
            break;
        }
    }
    
    /* Cleanup */
    close(socket);
    log_info("Worker handler exited");
    return 0;
}

/**
 * Send error response to client or worker
 */
static int send_error_response(int socket, int error_code, const char *error_msg) {
    Message response;
    memset(&response, 0, sizeof(Message));
    response.header.message_type = MSG_TYPE_ERROR;
    response.body.error.error_code = error_code;
    
    if (error_msg) {
        strncpy(response.body.error.error_message, error_msg, 
                sizeof(response.body.error.error_message) - 1);
    }
    
    if (send_message(socket, &response) < 0) {
        log_error("Failed to send error response");
        return -1;
    }
    
    return 0;
}

int server_init(const ServerConfig *config) {
    if (!config) {
        log_error("Invalid server configuration");
        return -1;
    }

    /* Initialize server structure */
    memset(&g_server, 0, sizeof(Server));
    memcpy(&g_server.config, config, sizeof(ServerConfig));
    
    /* Initialize mutex */
    if (pthread_mutex_init(&g_server.lock, NULL) != 0) {
        log_error("Failed to initialize server mutex: %s", strerror(errno));
        return -1;
    }

    /* Initialize job handler */
    if (job_handler_init(config->max_jobs, config->job_timeout_seconds) < 0) {
        log_error("Failed to initialize job handler");
        pthread_mutex_destroy(&g_server.lock);
        return -1;
    }

    /* Initialize worker manager */
    if (worker_manager_init(config->max_workers) < 0) {
        log_error("Failed to initialize worker manager");
        job_handler_cleanup();
        pthread_mutex_destroy(&g_server.lock);
        return -1;
    }

    /* Create server socket */
    g_server.server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server.server_socket < 0) {
        log_error("Failed to create server socket: %s", strerror(errno));
        worker_manager_cleanup();
        job_handler_cleanup();
        pthread_mutex_destroy(&g_server.lock);
        return -1;
    }

    /* Set socket options */
    int opt = 1;
    if (setsockopt(g_server.server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_error("Failed to set socket options: %s", strerror(errno));
        close(g_server.server_socket);
        worker_manager_cleanup();
        job_handler_cleanup();
        pthread_mutex_destroy(&g_server.lock);
        return -1;
    }

    /* Bind socket to address and port */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config->port);
    
    if (strlen(config->ip_address) > 0) {
        if (inet_pton(AF_INET, config->ip_address, &server_addr.sin_addr) <= 0) {
            log_error("Invalid IP address: %s", config->ip_address);
            close(g_server.server_socket);
            worker_manager_cleanup();
            job_handler_cleanup();
            pthread_mutex_destroy(&g_server.lock);
            return -1;
        }
    } else {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(g_server.server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_error("Failed to bind server socket: %s", strerror(errno));
        close(g_server.server_socket);
        worker_manager_cleanup();
        job_handler_cleanup();
        pthread_mutex_destroy(&g_server.lock);
        return -1;
    }

    /* Listen for connections */
    if (listen(g_server.server_socket, config->max_clients) < 0) {
        log_error("Failed to listen on server socket: %s", strerror(errno));
        close(g_server.server_socket);
        worker_manager_cleanup();
        job_handler_cleanup();
        pthread_mutex_destroy(&g_server.lock);
        return -1;
    }

    log_info("Server initialized on %s:%d", 
             strlen(config->ip_address) > 0 ? config->ip_address : "0.0.0.0", 
             config->port);
    
    return 0;
}

int server_start() {
    /* Check if server is already running */
    pthread_mutex_lock(&g_server.lock);
    if (g_server.is_running) {
        pthread_mutex_unlock(&g_server.lock);
        log_error("Server is already running");
        return -1;
    }
    
    /* Set running flag */
    g_server.is_running = 1;
    pthread_mutex_unlock(&g_server.lock);

    /* Create accept thread */
    if (pthread_create(&g_server.accept_thread, NULL, accept_thread_func, NULL) != 0) {
        log_error("Failed to create accept thread: %s", strerror(errno));
        pthread_mutex_lock(&g_server.lock);
        g_server.is_running = 0;
        pthread_mutex_unlock(&g_server.lock);
        return -1;
    }

    log_info("Server started");
    return 0;
}

int server_stop() {
    /* Check if server is running */
    pthread_mutex_lock(&g_server.lock);
    if (!g_server.is_running) {
        pthread_mutex_unlock(&g_server.lock);
        log_error("Server is not running");
        return -1;
    }
    
    /* Clear running flag */
    g_server.is_running = 0;
    pthread_mutex_unlock(&g_server.lock);

    /* Close server socket to interrupt accept() */
    close(g_server.server_socket);
    
    /* Wait for accept thread to finish */
    pthread_join(g_server.accept_thread, NULL);
    
    /* Cleanup resources */
    worker_manager_cleanup();
    job_handler_cleanup();
    pthread_mutex_destroy(&g_server.lock);
    
    log_info("Server stopped");
    return 0;
}

int server_get_stats(int *active_clients, int *active_workers, 
                     int *pending_jobs, int *running_jobs, 
                     int *completed_jobs, int *failed_jobs) {
    /* Get worker statistics */
    int total_workers, idle_workers, busy_workers;
    if (worker_manager_get_stats(&total_workers, &idle_workers, &busy_workers) < 0) {
        log_error("Failed to get worker statistics");
        return -1;
    }
    
    /* Get job statistics */
    int pending, running, completed, failed;
    if (job_handler_get_stats(&pending, &running, &completed, &failed) < 0) {
        log_error("Failed to get job statistics");
        return -1;
    }
    
    /* Set output values */
    if (active_workers) *active_workers = total_workers;
    if (active_clients) *active_clients = 0; /* TODO: Track active clients */
    if (pending_jobs) *pending_jobs = pending;
    if (running_jobs) *running_jobs = running;
    if (completed_jobs) *completed_jobs = completed;
    if (failed_jobs) *failed_jobs = failed;
    
    return 0;
}

int server_handle_client_message(int client_socket, const Message *msg) {
    if (!msg) {
        log_error("Invalid message");
        return -1;
    }
    
    Message response;
    memset(&response, 0, sizeof(Message));
    response.header.message_id = msg->header.message_id;
    response.header.client_id = msg->header.client_id;
    
    switch (msg->header.message_type) {
        case MSG_TYPE_SUBMIT_JOB: {
            log_debug("Received job submission from client");
            
            /* Submit job to job handler */
            int job_id = job_handler_submit(client_socket, msg->body.submit_job.job_type,
                                           msg->body.submit_job.data, msg->body.submit_job.data_size);
            
            if (job_id < 0) {
                log_error("Failed to submit job");
                return send_error_response(client_socket, ERR_JOB_SUBMISSION_FAILED, 
                                          "Failed to submit job");
            }
            
            /* Send response */
            response.header.message_type = MSG_TYPE_JOB_SUBMITTED;
            response.body.job_submitted.job_id = job_id;
            
            if (send_message(client_socket, &response) < 0) {
                log_error("Failed to send job submission response");
                return -1;
            }
            
            log_info("Job %d submitted successfully", job_id);
            break;
        }
        
        case MSG_TYPE_GET_JOB_STATUS: {
            int job_id = msg->body.get_job_status.job_id;
            log_debug("Received job status request for job %d", job_id);
            
            /* Get job status */
            int status = job_handler_get_status(job_id);
            
            if (status < 0) {
                log_error("Failed to get status for job %d", job_id);
                return send_error_response(client_socket, ERR_JOB_NOT_FOUND, 
                                          "Job not found");
            }
            
            /* Send response */
            response.header.message_type = MSG_TYPE_JOB_STATUS;
            response.body.job_status.job_id = job_id;
            response.body.job_status.status = status;
            
            if (send_message(client_socket, &response) < 0) {
                log_error("Failed to send job status response");
                return -1;
            }
            
            log_debug("Sent status for job %d: %d", job_id, status);
            break;
        }
        
        case MSG_TYPE_GET_JOB_RESULT: {
            int job_id = msg->body.get_job_result.job_id;
            log_debug("Received job result request for job %d", job_id);
            
            /* Get job status first */
            int status = job_handler_get_status(job_id);
            
            if (status < 0) {
                log_error("Failed to get status for job %d", job_id);
                return send_error_response(client_socket, ERR_JOB_NOT_FOUND, 
                                          "Job not found");
            }
            
            if (status != JOB_STATUS_COMPLETED) {
                log_error("Job %d is not completed (status: %d)", job_id, status);
                return send_error_response(client_socket, ERR_JOB_NOT_COMPLETED, 
                                          "Job is not completed");
            }
            
            /* Get job result */
            char *result_data = NULL;
            size_t result_size = 0;
            
            if (job_handler_get_result(job_id, &result_data, &result_size) < 0) {
                log_error("Failed to get result for job %d", job_id);
                return send_error_response(client_socket, ERR_INTERNAL_ERROR, 
                                          "Failed to get job result");
            }
            
            /* Send response */
            response.header.message_type = MSG_TYPE_JOB_RESULT;
            response.body.job_result.job_id = job_id;
            response.body.job_result.data = result_data;
            response.body.job_result.data_size = result_size;
            
            if (send_message(client_socket, &response) < 0) {
                log_error("Failed to send job result response");
                free(result_data);
                return -1;
            }
            
            free(result_data);
            log_debug("Sent result for job %d", job_id);
            break;
        }
        
        default:
            log_error("Unknown message type from client: %d", msg->header.message_type);
            return send_error_response(client_socket, ERR_INVALID_MESSAGE, 
                                      "Invalid message type");
    }
    
    return 0;
}

int server_handle_worker_message(int worker_socket, const Message *msg) {
    if (!msg) {
        log_error("Invalid message");
        return -1;
    }
    
    Message response;
    memset(&response, 0, sizeof(Message));
    response.header.message_id = msg->header.message_id;
    response.header.worker_id = msg->header.worker_id;
    
    switch (msg->header.message_type) {
        case MSG_TYPE_WORKER_REGISTER: {
            log_debug("Received worker registration");
            
            /* Register worker */
            int worker_id = worker_manager_register(
                worker_socket,
                msg->body.worker_register.ip_address,
                msg->body.worker_register.port,
                msg->body.worker_register.job_types_supported,
                msg->body.worker_register.num_job_types
            );
            
            if (worker_id < 0) {
                log_error("Failed to register worker");
                return send_error_response(worker_socket, ERR_WORKER_REGISTRATION_FAILED, 
                                          "Failed to register worker");
            }
            
            /* Send response */
            response.header.message_type = MSG_TYPE_WORKER_REGISTERED;
            response.body.worker_registered.worker_id = worker_id;
            
            if (send_message(worker_socket, &response) < 0) {
                log_error("Failed to send worker registration response");
                return -1;
            }
            
            log_info("Worker %d registered successfully", worker_id);
            break;
        }
        
        case MSG_TYPE_WORKER_HEARTBEAT: {
            int worker_id = msg->header.worker_id;
            log_debug("Received heartbeat from worker %d", worker_id);
            
            /* Update worker heartbeat */
            if (worker_manager_update_heartbeat(worker_id) < 0) {
                log_error("Failed to update heartbeat for worker %d", worker_id);
                return send_error_response(worker_socket, ERR_WORKER_NOT_FOUND, 
                                          "Worker not found");
            }
            
            /* Send response */
            response.header.message_type = MSG_TYPE_WORKER_HEARTBEAT_ACK;
            
            if (send_message(worker_socket, &response) < 0) {
                log_error("Failed to send heartbeat response to worker %d", worker_id);
                return -1;
            }
            
            log_debug("Sent heartbeat acknowledgment to worker %d", worker_id);
            break;
        }
        
        case MSG_TYPE_REQUEST_JOB: {
            int worker_id = msg->header.worker_id;
            log_debug("Received job request from worker %d", worker_id);
            
            /* Update worker status to idle */
            if (worker_manager_update_status(worker_id, WORKER_STATUS_IDLE) < 0) {
                log_error("Failed to update status for worker %d", worker_id);
                return send_error_response(worker_socket, ERR_WORKER_NOT_FOUND, 
                                          "Worker not found");
            }
            
            /* Assign job to worker */
            int job_id = job_handler_assign(worker_socket);
            
            if (job_id < 0) {
                /* No jobs available, send no job response */
                response.header.message_type = MSG_TYPE_NO_JOB_AVAILABLE;
                
                if (send_message(worker_socket, &response) < 0) {
                    log_error("Failed to send no job response to worker %d", worker_id);
                    return -1;
                }
                
                log_debug("No jobs available for worker %d", worker_id);
                return 0;
            }
            
            /* Update worker status to busy */
            if (worker_manager_assign_job(worker_id, job_id) < 0) {
                log_error("Failed to assign job %d to worker %d", job_id, worker_id);
                return -1;
            }
            
            log_info("Assigned job %d to worker %d", job_id, worker_id);
            break;
        }
        
        case MSG_TYPE_JOB_COMPLETED: {
            int worker_id = msg->header.worker_id;
            int job_id = msg->body.job_completed.job_id;
            long long processing_time = msg->body.job_completed.processing_time_ms;
            
            log_debug("Received job completion from worker %d for job %d", worker_id, job_id);
            
            /* Complete job */
            if (job_handler_complete(job_id, msg->body.job_completed.result_data, 
                                    msg->body.job_completed.result_size) < 0) {
                log_error("Failed to complete job %d", job_id);
                return send_error_response(worker_socket, ERR_JOB_NOT_FOUND, 
                                          "Job not found");
            }
            
            /* Update worker status */
            if (worker_manager_complete_job(worker_id, processing_time) < 0) {
                log_error("Failed to update worker %d after job completion", worker_id);
                return -1;
            }
            
            /* Send response */
            response.header.message_type = MSG_TYPE_JOB_COMPLETION_ACK;
            response.body.job_completion_ack.job_id = job_id;
            
            if (send_message(worker_socket, &response) < 0) {
                log_error("Failed to send job completion acknowledgment");
                return -1;
            }
            
            log_info("Job %d completed by worker %d in %lld ms", job_id, worker_id, processing_time);
            break;
        }
        
        case MSG_TYPE_JOB_FAILED: {
            int worker_id = msg->header.worker_id;
            int job_id = msg->body.job_failed.job_id;
            int error_code = msg->body.job_failed.error_code;
            
            log_debug("Received job failure from worker %d for job %d: error %d", 
                     worker_id, job_id, error_code);
            
            /* Mark job as failed */
            if (job_handler_fail(job_id, error_code) < 0) {
                log_error("Failed to mark job %d as failed", job_id);
                return send_error_response(worker_socket, ERR_JOB_NOT_FOUND, 
                                          "Job not found");
            }
            
            /* Update worker status */
            if (worker_manager_fail_job(worker_id) < 0) {
                log_error("Failed to update worker %d after job failure", worker_id);
                return -1;
            }
            
            /* Send response */
            response.header.message_type = MSG_TYPE_JOB_FAILURE_ACK;
            response.body.job_failure_ack.job_id = job_id;
            
            if (send_message(worker_socket, &response) < 0) {
                log_error("Failed to send job failure acknowledgment");
                return -1;
            }
            
            log_info("Job %d failed by worker %d with error %d", job_id, worker_id, error_code);
            break;
        }
        
        case MSG_TYPE_WORKER_DISCONNECT: {
            int worker_id = msg->header.worker_id;
            log_debug("Received disconnect request from worker %d", worker_id);
            
            /* Unregister worker */
            if (worker_manager_unregister(worker_id) < 0) {
                log_error("Failed to unregister worker %d", worker_id);
                return send_error_response(worker_socket, ERR_WORKER_NOT_FOUND, 
                                          "Worker not found");
            }
            
            /* Send response */
            response.header.message_type = MSG_TYPE_WORKER_DISCONNECT_ACK;
            
            if (send_message(worker_socket, &response) < 0) {
                log_error("Failed to send disconnect acknowledgment");
                return -1;
            }
            
            log_info("Worker %d disconnected", worker_id);
            break;
        }
        
        default:
            log_error("Unknown message type from worker: %d", msg->header.message_type);
            return send_error_response(worker_socket, ERR_INVALID_MESSAGE, 
                                      "Invalid message type");
    }
    
    return 0;
}