/**
 * @file client.c
 * @brief Client implementation for distributed worker system
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

#include "client.h"
#include "../common/logger.h"
#include "../common/net_utils.h"
#include "../common/protocol.h"
#include "../common/common.h"

/* Global client instance */
static Client g_client;

/* Forward declarations for internal functions */
static long long get_current_time_ms();

int client_init(const ClientConfig *config) {
    if (!config) {
        log_error("Invalid client configuration");
        return -1;
    }

    /* Initialize client structure */
    memset(&g_client, 0, sizeof(Client));
    memcpy(&g_client.config, config, sizeof(ClientConfig));
    g_client.client_id = -1;
    g_client.server_socket = -1;
    g_client.is_connected = 0;
    
    /* Initialize mutex */
    if (pthread_mutex_init(&g_client.lock, NULL) != 0) {
        log_error("Failed to initialize client mutex: %s", strerror(errno));
        return -1;
    }
    
    log_info("Client initialized");
    
    return 0;
}

int client_cleanup() {
    /* Disconnect from server if connected */
    client_disconnect_from_server();
    
    /* Cleanup resources */
    pthread_mutex_destroy(&g_client.lock);
    
    log_info("Client cleaned up");
    
    return 0;
}

int client_connect_to_server() {
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
    server_addr.sin_port = htons(g_client.config.server_port);
    
    if (inet_pton(AF_INET, g_client.config.server_ip, &server_addr.sin_addr) <= 0) {
        log_error("Invalid server IP address: %s", g_client.config.server_ip);
        close(socket_fd);
        return -1;
    }
    
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_error("Failed to connect to server %s:%d: %s", 
                 g_client.config.server_ip, g_client.config.server_port, strerror(errno));
        close(socket_fd);
        return -1;
    }
    
    /* Send client connect message */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.header.message_type = MSG_TYPE_CLIENT_CONNECT;
    
    if (send_message(socket_fd, &msg) < 0) {
        log_error("Failed to send client connect message");
        close(socket_fd);
        return -1;
    }
    
    /* Receive connect acknowledgment */
    if (receive_message(socket_fd, &msg) < 0) {
        log_error("Failed to receive connect acknowledgment");
        close(socket_fd);
        return -1;
    }
    
    if (msg.header.message_type != MSG_TYPE_CLIENT_CONNECT_ACK) {
        log_error("Unexpected response to connect message: %d", msg.header.message_type);
        close(socket_fd);
        return -1;
    }
    
    /* Update client state */
    pthread_mutex_lock(&g_client.lock);
    g_client.server_socket = socket_fd;
    g_client.is_connected = 1;
    g_client.client_id = msg.body.client_connect_ack.client_id;
    pthread_mutex_unlock(&g_client.lock);
    
    log_info("Connected to server %s:%d, client ID: %d", 
             g_client.config.server_ip, g_client.config.server_port, g_client.client_id);
    
    return 0;
}

int client_disconnect_from_server() {
    pthread_mutex_lock(&g_client.lock);
    
    if (!g_client.is_connected) {
        pthread_mutex_unlock(&g_client.lock);
        return 0;
    }
    
    /* Send disconnect message */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.header.message_type = MSG_TYPE_CLIENT_DISCONNECT;
    msg.header.client_id = g_client.client_id;
    
    send_message(g_client.server_socket, &msg);
    
    /* Wait for acknowledgment (best effort) */
    receive_message(g_client.server_socket, &msg);
    
    /* Close socket */
    close(g_client.server_socket);
    g_client.server_socket = -1;
    g_client.is_connected = 0;
    
    pthread_mutex_unlock(&g_client.lock);
    
    log_info("Disconnected from server");
    
    return 0;
}

int client_submit_job(int job_type, const char *input_data, size_t input_size) {
    if (!input_data && input_size > 0) {
        log_error("Invalid input data");
        return -1;
    }
    
    pthread_mutex_lock(&g_client.lock);
    
    if (!g_client.is_connected) {
        pthread_mutex_unlock(&g_client.lock);
        log_error("Not connected to server");
        return -1;
    }
    
    /* Prepare job submission message */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.header.message_type = MSG_TYPE_SUBMIT_JOB;
    msg.header.client_id = g_client.client_id;
    msg.body.submit_job.job_type = job_type;
    
    /* Copy input data */
    if (input_size > 0) {
        if (input_size > MAX_DATA_SIZE) {
            pthread_mutex_unlock(&g_client.lock);
            log_error("Input data size exceeds maximum allowed size");
            return -1;
        }
        
        memcpy(msg.body.submit_job.data, input_data, input_size);
    }
    
    msg.body.submit_job.data_size = input_size;
    
    int socket = g_client.server_socket;
    
    pthread_mutex_unlock(&g_client.lock);
    
    /* Send job submission message */
    if (send_message(socket, &msg) < 0) {
        log_error("Failed to send job submission message");
        return -1;
    }
    
    /* Receive job submission response */
    if (receive_message(socket, &msg) < 0) {
        log_error("Failed to receive job submission response");
        return -1;
    }
    
    if (msg.header.message_type == MSG_TYPE_ERROR) {
        log_error("Job submission failed: %s (code %d)", 
                 msg.body.error.error_message, msg.body.error.error_code);
        return -1;
    }
    
    if (msg.header.message_type != MSG_TYPE_JOB_SUBMITTED) {
        log_error("Unexpected response to job submission: %d", msg.header.message_type);
        return -1;
    }
    
    int job_id = msg.body.job_submitted.job_id;
    
    log_info("Job submitted successfully, job ID: %d", job_id);
    
    return job_id;
}

int client_get_job_status(int job_id) {
    pthread_mutex_lock(&g_client.lock);
    
    if (!g_client.is_connected) {
        pthread_mutex_unlock(&g_client.lock);
        log_error("Not connected to server");
        return -1;
    }
    
    /* Prepare job status request message */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.header.message_type = MSG_TYPE_GET_JOB_STATUS;
    msg.header.client_id = g_client.client_id;
    msg.body.get_job_status.job_id = job_id;
    
    int socket = g_client.server_socket;
    
    pthread_mutex_unlock(&g_client.lock);
    
    /* Send job status request message */
    if (send_message(socket, &msg) < 0) {
        log_error("Failed to send job status request message");
        return -1;
    }
    
    /* Receive job status response */
    if (receive_message(socket, &msg) < 0) {
        log_error("Failed to receive job status response");
        return -1;
    }
    
    if (msg.header.message_type == MSG_TYPE_ERROR) {
        log_error("Job status request failed: %s (code %d)", 
                 msg.body.error.error_message, msg.body.error.error_code);
        return -1;
    }
    
    if (msg.header.message_type != MSG_TYPE_JOB_STATUS) {
        log_error("Unexpected response to job status request: %d", msg.header.message_type);
        return -1;
    }
    
    int status = msg.body.job_status.status;
    
    log_debug("Job %d status: %d", job_id, status);
    
    return status;
}

int client_get_job_result(int job_id, char **result_data, size_t *result_size) {
    if (!result_data || !result_size) {
        log_error("Invalid result pointers");
        return -1;
    }
    
    /* Initialize result */
    *result_data = NULL;
    *result_size = 0;
    
    pthread_mutex_lock(&g_client.lock);
    
    if (!g_client.is_connected) {
        pthread_mutex_unlock(&g_client.lock);
        log_error("Not connected to server");
        return -1;
    }
    
    /* Prepare job result request message */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.header.message_type = MSG_TYPE_GET_JOB_RESULT;
    msg.header.client_id = g_client.client_id;
    msg.body.get_job_result.job_id = job_id;
    
    int socket = g_client.server_socket;
    
    pthread_mutex_unlock(&g_client.lock);
    
    /* Send job result request message */
    if (send_message(socket, &msg) < 0) {
        log_error("Failed to send job result request message");
        return -1;
    }
    
    /* Receive job result response */
    if (receive_message(socket, &msg) < 0) {
        log_error("Failed to receive job result response");
        return -1;
    }
    
    if (msg.header.message_type == MSG_TYPE_ERROR) {
        log_error("Job result request failed: %s (code %d)", 
                 msg.body.error.error_message, msg.body.error.error_code);
        return -1;
    }
    
    if (msg.header.message_type != MSG_TYPE_JOB_RESULT) {
        log_error("Unexpected response to job result request: %d", msg.header.message_type);
        return -1;
    }
    
    /* Copy result data */
    size_t data_size = msg.body.job_result.result_size;
    if (data_size > 0) {
        char *data_copy = (char *)malloc(data_size);
        if (!data_copy) {
            log_error("Failed to allocate memory for result data");
            return -1;
        }
        
        memcpy(data_copy, msg.body.job_result.result_data, data_size);
        *result_data = data_copy;
        *result_size = data_size;
    }
    
    log_debug("Retrieved result for job %d, size: %zu", job_id, data_size);
    
    return 0;
}

int client_wait_for_job(int job_id, int timeout_seconds) {
    long long start_time = get_current_time_ms();
    long long timeout_ms = timeout_seconds * 1000LL;
    
    while (1) {
        /* Check job status */
        int status = client_get_job_status(job_id);
        
        if (status < 0) {
            log_error("Failed to get job status");
            return -1;
        }
        
        /* Check if job is completed or failed */
        if (status == JOB_STATUS_COMPLETED || status == JOB_STATUS_FAILED || 
            status == JOB_STATUS_TIMEOUT) {
            return status;
        }
        
        /* Check timeout */
        if (timeout_seconds > 0) {
            long long elapsed_ms = get_current_time_ms() - start_time;
            if (elapsed_ms >= timeout_ms) {
                log_error("Timeout waiting for job %d", job_id);
                return -1;
            }
        }
        
        /* Sleep for a while before checking again */
        usleep(100000); /* 100ms */
    }
    
    return 0;
}

int client_get_server_stats(int *active_clients, int *active_workers,
                           int *pending_jobs, int *running_jobs,
                           int *completed_jobs, int *failed_jobs) {
    pthread_mutex_lock(&g_client.lock);
    
    if (!g_client.is_connected) {
        pthread_mutex_unlock(&g_client.lock);
        log_error("Not connected to server");
        return -1;
    }
    
    /* Prepare server stats request message */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.header.message_type = MSG_TYPE_GET_SERVER_STATS;
    msg.header.client_id = g_client.client_id;
    
    int socket = g_client.server_socket;
    
    pthread_mutex_unlock(&g_client.lock);
    
    /* Send server stats request message */
    if (send_message(socket, &msg) < 0) {
        log_error("Failed to send server stats request message");
        return -1;
    }
    
    /* Receive server stats response */
    if (receive_message(socket, &msg) < 0) {
        log_error("Failed to receive server stats response");
        return -1;
    }
    
    if (msg.header.message_type == MSG_TYPE_ERROR) {
        log_error("Server stats request failed: %s (code %d)", 
                 msg.body.error.error_message, msg.body.error.error_code);
        return -1;
    }
    
    if (msg.header.message_type != MSG_TYPE_SERVER_STATS) {
        log_error("Unexpected response to server stats request: %d", msg.header.message_type);
        return -1;
    }
    
    /* Copy statistics */
    if (active_clients) *active_clients = msg.body.server_stats.active_clients;
    if (active_workers) *active_workers = msg.body.server_stats.active_workers;
    if (pending_jobs) *pending_jobs = msg.body.server_stats.pending_jobs;
    if (running_jobs) *running_jobs = msg.body.server_stats.running_jobs;
    if (completed_jobs) *completed_jobs = msg.body.server_stats.completed_jobs;
    if (failed_jobs) *failed_jobs = msg.body.server_stats.failed_jobs;
    
    log_debug("Retrieved server statistics");
    
    return 0;
}

/* Get current time in milliseconds */
static long long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}