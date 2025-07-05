/**
 * @file worker.c
 * @brief Worker implementation for distributed worker system
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
#include <sys/time.h>

#include "worker.h"
#include "../common/logger.h"
#include "../common/net_utils.h"
#include "../common/protocol.h"
#include "../common/common.h"

/* Global worker instance */
static Worker g_worker;

/* Statistics */
static int g_jobs_processed = 0;
static int g_jobs_failed = 0;
static long long g_total_processing_time_ms = 0;

/* Forward declarations for internal functions */
static void *heartbeat_thread_func(void *arg);
static void *job_thread_func(void *arg);
static long long get_current_time_ms();

int worker_init(const WorkerConfig *config) {
    if (!config) {
        log_error("Invalid worker configuration");
        return -1;
    }

    /* Initialize worker structure */
    memset(&g_worker, 0, sizeof(Worker));
    memcpy(&g_worker.config, config, sizeof(WorkerConfig));
    g_worker.worker_id = -1;
    g_worker.server_socket = -1;
    g_worker.is_running = 0;
    g_worker.is_connected = 0;
    
    /* Initialize mutex */
    if (pthread_mutex_init(&g_worker.lock, NULL) != 0) {
        log_error("Failed to initialize worker mutex: %s", strerror(errno));
        return -1;
    }
    
    log_info("Worker initialized");
    
    return 0;
}

int worker_start() {
    /* Check if worker is already running */
    pthread_mutex_lock(&g_worker.lock);
    if (g_worker.is_running) {
        pthread_mutex_unlock(&g_worker.lock);
        log_error("Worker is already running");
        return -1;
    }
    
    /* Set running flag */
    g_worker.is_running = 1;
    pthread_mutex_unlock(&g_worker.lock);
    
    /* Connect to server */
    if (worker_connect_to_server() < 0) {
        log_error("Failed to connect to server");
        pthread_mutex_lock(&g_worker.lock);
        g_worker.is_running = 0;
        pthread_mutex_unlock(&g_worker.lock);
        return -1;
    }
    
    /* Register with server */
    if (worker_register() < 0) {
        log_error("Failed to register with server");
        worker_disconnect_from_server();
        pthread_mutex_lock(&g_worker.lock);
        g_worker.is_running = 0;
        pthread_mutex_unlock(&g_worker.lock);
        return -1;
    }
    
    /* Create heartbeat thread */
    if (pthread_create(&g_worker.heartbeat_thread, NULL, heartbeat_thread_func, NULL) != 0) {
        log_error("Failed to create heartbeat thread: %s", strerror(errno));
        worker_disconnect_from_server();
        pthread_mutex_lock(&g_worker.lock);
        g_worker.is_running = 0;
        pthread_mutex_unlock(&g_worker.lock);
        return -1;
    }
    
    /* Create job thread */
    if (pthread_create(&g_worker.job_thread, NULL, job_thread_func, NULL) != 0) {
        log_error("Failed to create job thread: %s", strerror(errno));
        pthread_cancel(g_worker.heartbeat_thread);
        pthread_join(g_worker.heartbeat_thread, NULL);
        worker_disconnect_from_server();
        pthread_mutex_lock(&g_worker.lock);
        g_worker.is_running = 0;
        pthread_mutex_unlock(&g_worker.lock);
        return -1;
    }
    
    log_info("Worker started");
    
    return 0;
}

int worker_stop() {
    /* Check if worker is running */
    pthread_mutex_lock(&g_worker.lock);
    if (!g_worker.is_running) {
        pthread_mutex_unlock(&g_worker.lock);
        log_error("Worker is not running");
        return -1;
    }
    
    /* Clear running flag */
    g_worker.is_running = 0;
    pthread_mutex_unlock(&g_worker.lock);
    
    /* Wait for threads to finish */
    pthread_join(g_worker.heartbeat_thread, NULL);
    pthread_join(g_worker.job_thread, NULL);
    
    /* Disconnect from server */
    worker_disconnect_from_server();
    
    /* Cleanup resources */
    pthread_mutex_destroy(&g_worker.lock);
    
    log_info("Worker stopped");
    
    return 0;
}

int worker_connect_to_server() {
    /* Create socket */
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        log_error("Failed to create socket: %s", strerror(errno));
        return -1;
    }
    
    /* Connect to server */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_worker.config.server_port);
    
    if (inet_pton(AF_INET, g_worker.config.server_ip, &server_addr.sin_addr) <= 0) {
        log_error("Invalid server IP address: %s", g_worker.config.server_ip);
        close(socket_fd);
        return -1;
    }
    
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_error("Failed to connect to server %s:%d: %s", 
                 g_worker.config.server_ip, g_worker.config.server_port, strerror(errno));
        close(socket_fd);
        return -1;
    }
    
    /* Send worker connect message */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.header.message_type = MSG_TYPE_WORKER_CONNECT;
    
    if (send_message(socket_fd, &msg) < 0) {
        log_error("Failed to send worker connect message");
        close(socket_fd);
        return -1;
    }
    
    /* Receive connect acknowledgment */
    if (receive_message(socket_fd, &msg) < 0) {
        log_error("Failed to receive connect acknowledgment");
        close(socket_fd);
        return -1;
    }
    
    if (msg.header.message_type != MSG_TYPE_WORKER_CONNECT_ACK) {
        log_error("Unexpected response to connect message: %d", msg.header.message_type);
        close(socket_fd);
        return -1;
    }
    
    /* Update worker state */
    pthread_mutex_lock(&g_worker.lock);
    g_worker.server_socket = socket_fd;
    g_worker.is_connected = 1;
    pthread_mutex_unlock(&g_worker.lock);
    
    log_info("Connected to server %s:%d", g_worker.config.server_ip, g_worker.config.server_port);
    
    return 0;
}

int worker_disconnect_from_server() {
    pthread_mutex_lock(&g_worker.lock);
    
    if (!g_worker.is_connected) {
        pthread_mutex_unlock(&g_worker.lock);
        return 0;
    }
    
    /* Send disconnect message if registered */
    if (g_worker.worker_id >= 0) {
        Message msg;
        memset(&msg, 0, sizeof(Message));
        msg.header.message_type = MSG_TYPE_WORKER_DISCONNECT;
        msg.header.worker_id = g_worker.worker_id;
        
        send_message(g_worker.server_socket, &msg);
        
        /* Wait for acknowledgment (best effort) */
        receive_message(g_worker.server_socket, &msg);
    }
    
    /* Close socket */
    close(g_worker.server_socket);
    g_worker.server_socket = -1;
    g_worker.is_connected = 0;
    
    pthread_mutex_unlock(&g_worker.lock);
    
    log_info("Disconnected from server");
    
    return 0;
}

int worker_register() {
    pthread_mutex_lock(&g_worker.lock);
    
    if (!g_worker.is_connected) {
        pthread_mutex_unlock(&g_worker.lock);
        log_error("Not connected to server");
        return -1;
    }
    
    /* Prepare registration message */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.header.message_type = MSG_TYPE_WORKER_REGISTER;
    
    strncpy(msg.body.worker_register.ip_address, g_worker.config.worker_ip, 
           sizeof(msg.body.worker_register.ip_address) - 1);
    msg.body.worker_register.port = g_worker.config.worker_port;
    
    /* Copy supported job types */
    int num_types = g_worker.config.num_job_types;
    if (num_types > MAX_JOB_TYPES) {
        num_types = MAX_JOB_TYPES;
    }
    
    for (int i = 0; i < num_types; i++) {
        msg.body.worker_register.job_types_supported[i] = g_worker.config.job_types[i];
    }
    
    msg.body.worker_register.num_job_types = num_types;
    
    int socket = g_worker.server_socket;
    
    pthread_mutex_unlock(&g_worker.lock);
    
    /* Send registration message */
    if (send_message(socket, &msg) < 0) {
        log_error("Failed to send registration message");
        return -1;
    }
    
    /* Receive registration response */
    if (receive_message(socket, &msg) < 0) {
        log_error("Failed to receive registration response");
        return -1;
    }
    
    if (msg.header.message_type == MSG_TYPE_ERROR) {
        log_error("Registration failed: %s (code %d)", 
                 msg.body.error.error_message, msg.body.error.error_code);
        return -1;
    }
    
    if (msg.header.message_type != MSG_TYPE_WORKER_REGISTERED) {
        log_error("Unexpected response to registration: %d", msg.header.message_type);
        return -1;
    }
    
    /* Update worker ID */
    pthread_mutex_lock(&g_worker.lock);
    g_worker.worker_id = msg.body.worker_registered.worker_id;
    pthread_mutex_unlock(&g_worker.lock);
    
    log_info("Registered with server, worker ID: %d", g_worker.worker_id);
    
    return 0;
}

int worker_send_heartbeat() {
    pthread_mutex_lock(&g_worker.lock);
    
    if (!g_worker.is_connected || g_worker.worker_id < 0) {
        pthread_mutex_unlock(&g_worker.lock);
        log_error("Not connected or registered");
        return -1;
    }
    
    /* Prepare heartbeat message */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.header.message_type = MSG_TYPE_WORKER_HEARTBEAT;
    msg.header.worker_id = g_worker.worker_id;
    
    int socket = g_worker.server_socket;
    
    pthread_mutex_unlock(&g_worker.lock);
    
    /* Send heartbeat message */
    if (send_message(socket, &msg) < 0) {
        log_error("Failed to send heartbeat message");
        return -1;
    }
    
    /* Receive heartbeat response */
    if (receive_message(socket, &msg) < 0) {
        log_error("Failed to receive heartbeat response");
        return -1;
    }
    
    if (msg.header.message_type != MSG_TYPE_WORKER_HEARTBEAT_ACK) {
        log_error("Unexpected response to heartbeat: %d", msg.header.message_type);
        return -1;
    }
    
    log_debug("Heartbeat acknowledged");
    
    return 0;
}

int worker_request_job() {
    pthread_mutex_lock(&g_worker.lock);
    
    if (!g_worker.is_connected || g_worker.worker_id < 0) {
        pthread_mutex_unlock(&g_worker.lock);
        log_error("Not connected or registered");
        return -1;
    }
    
    /* Prepare job request message */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.header.message_type = MSG_TYPE_REQUEST_JOB;
    msg.header.worker_id = g_worker.worker_id;
    
    int socket = g_worker.server_socket;
    
    pthread_mutex_unlock(&g_worker.lock);
    
    /* Send job request message */
    if (send_message(socket, &msg) < 0) {
        log_error("Failed to send job request message");
        return -1;
    }
    
    /* Receive job response */
    if (receive_message(socket, &msg) < 0) {
        log_error("Failed to receive job response");
        return -1;
    }
    
    if (msg.header.message_type == MSG_TYPE_NO_JOB_AVAILABLE) {
        log_debug("No jobs available");
        return 0;
    }
    
    if (msg.header.message_type != MSG_TYPE_JOB_ASSIGNED) {
        log_error("Unexpected response to job request: %d", msg.header.message_type);
        return -1;
    }
    
    int job_id = msg.body.job_assigned.job_id;
    int job_type = msg.body.job_assigned.job_type;
    const char *input_data = msg.body.job_assigned.data;
    size_t input_size = msg.body.job_assigned.data_size;
    
    log_info("Received job %d of type %d, input_size=%zu", job_id, job_type, input_size);
    
    /* Process job */
    char *result_data = NULL;
    size_t result_size = 0;
    long long start_time = get_current_time_ms();
    
    int result = worker_process_job(job_id, job_type, input_data, input_size, 
                                   &result_data, &result_size);
    
    long long end_time = get_current_time_ms();
    long long processing_time = end_time - start_time;
    
    if (result < 0) {
        log_error("Failed to process job %d", job_id);
        worker_send_job_failure(job_id, ERR_JOB_PROCESSING_FAILED);
        g_jobs_failed++;
        return job_id;
    }
    
    /* Send job completion */
    if (worker_send_job_completion(job_id, result_data, result_size, processing_time) < 0) {
        log_error("Failed to send job completion for job %d", job_id);
        free(result_data);
        g_jobs_failed++;
        return job_id;
    }
    
    /* Update statistics */
    g_jobs_processed++;
    g_total_processing_time_ms += processing_time;
    
    /* Free result data */
    free(result_data);
    
    log_info("Job %d completed in %lld ms", job_id, processing_time);
    
    return job_id;
}

int worker_process_job(int job_id, int job_type, const char *input_data, size_t input_size,
                      char **result_data, size_t *result_size) {
    if (!result_data || !result_size) {
        log_error("Invalid result pointers");
        return -1;
    }
    
    /* Initialize result */
    *result_data = NULL;
    *result_size = 0;
    
    /* Process job based on type */
    switch (job_type) {
        case JOB_TYPE_ECHO: {
            /* Echo job just returns the input data */
            char *data_copy = (char *)malloc(input_size);
            if (!data_copy) {
                log_error("Failed to allocate memory for result data");
                return -1;
            }
            
            memcpy(data_copy, input_data, input_size);
            *result_data = data_copy;
            *result_size = input_size;
            
            log_debug("Processed echo job %d", job_id);
            break;
        }
        
        case JOB_TYPE_REVERSE: {
            /* Reverse job reverses the input data */
            char *data_copy = (char *)malloc(input_size);
            if (!data_copy) {
                log_error("Failed to allocate memory for result data");
                return -1;
            }
            
            for (size_t i = 0; i < input_size; i++) {
                data_copy[i] = input_data[input_size - i - 1];
            }
            
            *result_data = data_copy;
            *result_size = input_size;
            
            log_debug("Processed reverse job %d", job_id);
            break;
        }
        
        case JOB_TYPE_UPPERCASE: {
            /* Uppercase job converts input to uppercase */
            char *data_copy = (char *)malloc(input_size);
            if (!data_copy) {
                log_error("Failed to allocate memory for result data");
                return -1;
            }
            
            for (size_t i = 0; i < input_size; i++) {
                if (input_data[i] >= 'a' && input_data[i] <= 'z') {
                    data_copy[i] = input_data[i] - 'a' + 'A';
                } else {
                    data_copy[i] = input_data[i];
                }
            }
            
            *result_data = data_copy;
            *result_size = input_size;
            
            log_debug("Processed uppercase job %d", job_id);
            break;
        }
        
        default:
            log_error("Unsupported job type: %d", job_type);
            return -1;
    }
    
    return 0;
}

int worker_send_job_completion(int job_id, const char *result_data, size_t result_size,
                              long long processing_time_ms) {
    pthread_mutex_lock(&g_worker.lock);
    
    if (!g_worker.is_connected || g_worker.worker_id < 0) {
        pthread_mutex_unlock(&g_worker.lock);
        log_error("Not connected or registered");
        return -1;
    }
    
    /* Prepare job completion message */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.header.message_type = MSG_TYPE_JOB_COMPLETED;
    msg.header.worker_id = g_worker.worker_id;
    msg.body.job_completed.job_id = job_id;
    msg.body.job_completed.result_data = result_data;
    msg.body.job_completed.result_size = result_size;
    msg.body.job_completed.processing_time_ms = processing_time_ms;
    
    int socket = g_worker.server_socket;
    
    pthread_mutex_unlock(&g_worker.lock);
    
    /* Send job completion message */
    if (send_message(socket, &msg) < 0) {
        log_error("Failed to send job completion message");
        return -1;
    }
    
    /* Receive completion acknowledgment */
    if (receive_message(socket, &msg) < 0) {
        log_error("Failed to receive completion acknowledgment");
        return -1;
    }
    
    if (msg.header.message_type != MSG_TYPE_JOB_COMPLETION_ACK) {
        log_error("Unexpected response to job completion: %d", msg.header.message_type);
        return -1;
    }
    
    log_debug("Job completion acknowledged");
    
    return 0;
}

int worker_send_job_failure(int job_id, int error_code) {
    pthread_mutex_lock(&g_worker.lock);
    
    if (!g_worker.is_connected || g_worker.worker_id < 0) {
        pthread_mutex_unlock(&g_worker.lock);
        log_error("Not connected or registered");
        return -1;
    }
    
    /* Prepare job failure message */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.header.message_type = MSG_TYPE_JOB_FAILED;
    msg.header.worker_id = g_worker.worker_id;
    msg.body.job_failed.job_id = job_id;
    msg.body.job_failed.error_code = error_code;
    
    int socket = g_worker.server_socket;
    
    pthread_mutex_unlock(&g_worker.lock);
    
    /* Send job failure message */
    if (send_message(socket, &msg) < 0) {
        log_error("Failed to send job failure message");
        return -1;
    }
    
    /* Receive failure acknowledgment */
    if (receive_message(socket, &msg) < 0) {
        log_error("Failed to receive failure acknowledgment");
        return -1;
    }
    
    if (msg.header.message_type != MSG_TYPE_JOB_FAILURE_ACK) {
        log_error("Unexpected response to job failure: %d", msg.header.message_type);
        return -1;
    }
    
    log_debug("Job failure acknowledged");
    
    return 0;
}

int worker_get_stats(int *jobs_processed, int *jobs_failed, double *avg_processing_time_ms) {
    if (jobs_processed) *jobs_processed = g_jobs_processed;
    if (jobs_failed) *jobs_failed = g_jobs_failed;
    
    if (avg_processing_time_ms) {
        if (g_jobs_processed > 0) {
            *avg_processing_time_ms = (double)g_total_processing_time_ms / g_jobs_processed;
        } else {
            *avg_processing_time_ms = 0.0;
        }
    }
    
    return 0;
}

/* Heartbeat thread function */
static void *heartbeat_thread_func(void *arg) {
    log_info("Heartbeat thread started");
    
    while (1) {
        /* Check if worker is still running */
        pthread_mutex_lock(&g_worker.lock);
        int is_running = g_worker.is_running;
        pthread_mutex_unlock(&g_worker.lock);
        
        if (!is_running) {
            log_info("Heartbeat thread stopping");
            break;
        }
        
        /* Send heartbeat */
        if (worker_send_heartbeat() < 0) {
            log_error("Failed to send heartbeat, attempting to reconnect");
            
            /* Attempt to reconnect */
            worker_disconnect_from_server();
            
            if (worker_connect_to_server() < 0) {
                log_error("Failed to reconnect to server");
                
                /* Sleep before retry */
                sleep(g_worker.config.reconnect_interval_seconds);
                continue;
            }
            
            /* Re-register with server */
            if (worker_register() < 0) {
                log_error("Failed to re-register with server");
                worker_disconnect_from_server();
                
                /* Sleep before retry */
                sleep(g_worker.config.reconnect_interval_seconds);
                continue;
            }
            
            log_info("Successfully reconnected to server");
        }
        
        /* Sleep until next heartbeat */
        sleep(g_worker.config.heartbeat_interval_seconds);
    }
    
    log_info("Heartbeat thread exited");
    return NULL;
}

/* Job thread function */
static void *job_thread_func(void *arg) {
    log_info("Job thread started");
    
    while (1) {
        /* Check if worker is still running */
        pthread_mutex_lock(&g_worker.lock);
        int is_running = g_worker.is_running;
        int is_connected = g_worker.is_connected;
        pthread_mutex_unlock(&g_worker.lock);
        
        if (!is_running) {
            log_info("Job thread stopping");
            break;
        }
        
        /* Skip if not connected */
        if (!is_connected) {
            sleep(1);
            continue;
        }
        
        /* Request job */
        int result = worker_request_job();
        
        if (result < 0) {
            log_error("Failed to request job, will retry");
            sleep(1);
        } else if (result == 0) {
            /* No job available, wait a bit before retrying */
            usleep(500000); /* 500ms */
        } else {
            /* Job processed successfully, continue immediately */
        }
    }
    
    log_info("Job thread exited");
    return NULL;
}

/* Get current time in milliseconds */
static long long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}