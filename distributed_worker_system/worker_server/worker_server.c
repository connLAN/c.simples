/**
 * worker_server.c - Worker Server Implementation
 * 
 * This file contains the main function and task processing logic for the worker server.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define _POSIX_C_SOURCE 200809L
#include <time.h>

#include "../lib/include/protocol.h"
#include "../lib/include/net_utils.h"
#include "../lib/include/logger.h"

/* Configuration constants */
#define DEFAULT_SERVER_IP "127.0.0.1"
#define DEFAULT_SERVER_PORT 8888
#define HEARTBEAT_INTERVAL_SEC 10
#define RECONNECT_INTERVAL_SEC 5
#define MAX_RECONNECT_ATTEMPTS 5

/* Global variables for worker state */
static int worker_running = 1;
static int server_socket = -1;
static uint32_t worker_id = 0;
static pthread_t heartbeat_thread_id;
static pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Forward declarations */
void *heartbeat_thread(void *arg);
int process_task(const task_t *task, task_result_t *result);
void signal_handler(int sig);

/* Connect to central server */
int connect_to_server(const char *server_ip, uint16_t server_port) {
    int sockfd;
    int attempts = 0;
    
    while (attempts < MAX_RECONNECT_ATTEMPTS && worker_running) {
        LOG_INFO("Connecting to central server at %s:%d (attempt %d/%d)",
                server_ip, server_port, attempts + 1, MAX_RECONNECT_ATTEMPTS);
        
        sockfd = create_client_socket(server_ip, server_port);
        if (sockfd >= 0) {
            LOG_INFO("Connected to central server");
            return sockfd;
        }
        
        LOG_ERROR("Failed to connect to central server: %s", strerror(errno));
        
        attempts++;
        if (attempts < MAX_RECONNECT_ATTEMPTS) {
            LOG_INFO("Retrying in %d seconds...", RECONNECT_INTERVAL_SEC);
            sleep(RECONNECT_INTERVAL_SEC);
        }
    }
    
    return -1;
}

/* Register with central server */
int register_with_server(int sockfd) {
    MessageHeader header;
    
    LOG_INFO("Registering with central server");
    
    /* Send registration request */
    Message reg_msg = {
        .header = {MSG_TYPE_REGISTER, 0}
    };
    if (send_message(sockfd, &reg_msg) < 0) {
        LOG_ERROR("Failed to send registration request: %s", strerror(errno));
        return -1;
    }
    
    /* Receive registration response with retry */
    int retries = 3;
    while (retries-- > 0) {
        if (recv_header(sockfd, &header) == 0) {
            if (header.message_type == MSG_TYPE_REGISTER_RESPONSE) {
                break;
            }
            LOG_WARNING("Unexpected message type %u, expected REGISTER_RESPONSE", header.message_type);
        } else {
            LOG_WARNING("Failed to receive registration response: %s (retries left: %d)", 
                      strerror(errno), retries);
        }
        
        if (retries > 0) {
            sleep(1);
        }
    }
    
    if (retries <= 0) {
        LOG_ERROR("Failed to get valid registration response after retries");
        return -1;
    }
    
    worker_id = header.client_id;
    LOG_INFO("Registered with central server, assigned worker ID: %u", worker_id);
    
    return 0;
}

/* Send heartbeat to central server */
int send_heartbeat(int sockfd) {
    MessageHeader header;
    
    pthread_mutex_lock(&socket_mutex);
    
    /* Send heartbeat message */
    Message hb_msg = {
        .header = {MSG_TYPE_HEARTBEAT, worker_id}
    };
    if (send_message(sockfd, &hb_msg) < 0) {
        pthread_mutex_unlock(&socket_mutex);
        LOG_ERROR("Failed to send heartbeat: %s", strerror(errno));
        return -1;
    }
    
    /* Receive heartbeat response */
    if (recv_header(sockfd, &header) != 0) {
        pthread_mutex_unlock(&socket_mutex);
        LOG_ERROR("Failed to receive heartbeat response: %s", strerror(errno));
        return -1;
    }
    
    pthread_mutex_unlock(&socket_mutex);
    
    switch (header.message_type) {
        case MSG_TYPE_HEARTBEAT_RESPONSE:
            LOG_DEBUG("Heartbeat acknowledged by central server");
            break;
        case MSG_TYPE_TASK:
            /* Received task assignment during heartbeat */
            pthread_mutex_unlock(&socket_mutex);
            return 1;
        default:
            LOG_ERROR("Expected heartbeat response, got message type %u", header.message_type);
            return -1;
    }
    
    return 0;
}

/* Send task result to central server */
int send_task_result(int sockfd, const task_result_t *result) {
    MessageHeader header;
    
    pthread_mutex_lock(&socket_mutex);
    
    /* Send task result message */
    Message result_msg = {
        .header = {MSG_TYPE_TASK_RESULT, worker_id}
    };
    memcpy(&result_msg.body.job_result, result, sizeof(task_result_t));
    if (send_message(sockfd, &result_msg) < 0) {
        pthread_mutex_unlock(&socket_mutex);
        LOG_ERROR("Failed to send task result: %s", strerror(errno));
        return -1;
    }
    
    /* Receive result acknowledgment */
    if (recv_header(sockfd, &header) != 0) {
        pthread_mutex_unlock(&socket_mutex);
        LOG_ERROR("Failed to receive result acknowledgment: %s", strerror(errno));
        return -1;
    }
    
    pthread_mutex_unlock(&socket_mutex);
    
    if (header.message_type != MSG_TYPE_RESULT_ACK) {
        LOG_ERROR("Expected result acknowledgment, got message type %u", header.message_type);
        return -1;
    }
    
    LOG_INFO("Task result acknowledged by central server");
    
    return 0;
}

/* Process a task */
int process_task(const task_t *task, task_result_t *result) {
    struct timespec start_time, end_time;
    long elapsed_ms;
    
    if (task == NULL || result == NULL) {
        return -1;
    }
    
    LOG_INFO("Processing task %u", task->task_id);
    
    /* Record start time */
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    /* Initialize result */
    result->task_id = task->task_id;
    result->status = STATUS_SUCCESS;
    
    /* Simulate task processing */
    LOG_INFO("Task data: %s", (char *)task->input_data);
    
    /* Sleep for a random time between 1 and 5 seconds */
    sleep(1 + rand() % 5);
    
    /* Record end time and calculate execution time */
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                (end_time.tv_nsec - start_time.tv_nsec) / 1000000;
    
    result->exec_time_ms = (uint32_t)elapsed_ms;
    
    LOG_INFO("Task %u completed in %u ms", task->task_id, result->exec_time_ms);
    
    return 0;
}

/* Heartbeat thread function */
void *heartbeat_thread(void *arg) {
    int sockfd = *((int *)arg);
    
    LOG_INFO("Heartbeat thread started");
    
    while (worker_running) {
        /* Sleep for heartbeat interval */
        sleep(HEARTBEAT_INTERVAL_SEC);
        
        /* Send heartbeat */
        if (send_heartbeat(sockfd) != 0) {
            LOG_ERROR("Heartbeat failed, server may be down");
        }
    }
    
    LOG_INFO("Heartbeat thread stopped");
    return NULL;
}

/* Signal handler function */
void signal_handler(int sig) {
    LOG_INFO("Received signal %d, shutting down...", sig);
    worker_running = 0;
}

/* Main function */
int main(int argc, char *argv[]) {
    char server_ip[16] = DEFAULT_SERVER_IP;
    uint16_t server_port = DEFAULT_SERVER_PORT;
    MessageHeader header;
    task_t task;
    task_result_t result;
    
    /* Initialize random seed */
    srand(time(NULL));
    
    /* Parse command line arguments */
    if (argc >= 2) {
        strncpy(server_ip, argv[1], sizeof(server_ip) - 1);
        server_ip[sizeof(server_ip) - 1] = '\0';
    }
    
    if (argc >= 3) {
        server_port = (uint16_t)atoi(argv[2]);
    }
    
    /* Initialize logger */
    if (logger_init("WorkerServer", "worker_server.log", LOG_INFO) != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return EXIT_FAILURE;
    }
    
    LOG_INFO("Worker server starting...");
    
    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Connect to central server */
    server_socket = connect_to_server(server_ip, server_port);
    if (server_socket < 0) {
        LOG_FATAL("Failed to connect to central server after %d attempts", MAX_RECONNECT_ATTEMPTS);
        logger_close();
        return EXIT_FAILURE;
    }
    
    /* Register with central server */
    if (register_with_server(server_socket) != 0) {
        LOG_FATAL("Failed to register with central server");
        close(server_socket);
        logger_close();
        return EXIT_FAILURE;
    }
    
    /* Start heartbeat thread */
    if (pthread_create(&heartbeat_thread_id, NULL, heartbeat_thread, &server_socket) != 0) {
        LOG_FATAL("Failed to create heartbeat thread");
        close(server_socket);
        logger_close();
        return EXIT_FAILURE;
    }
    
    LOG_INFO("Worker server running, waiting for tasks...");
    
    /* Main task processing loop */
    while (worker_running) {
        /* Receive message header */
        pthread_mutex_lock(&socket_mutex);
        if (recv_header(server_socket, &header) != 0) {
            pthread_mutex_unlock(&socket_mutex);
            
            if (errno == EINTR) {
                /* Interrupted by signal, check if worker is still running */
                continue;
            }
            
            LOG_ERROR("Failed to receive message from server: %s", strerror(errno));
            break;
        }
        
        /* Process message based on type */
        switch (header.message_type) {
            case MSG_TYPE_TASK:
                /* Receive task data */
                if (recv_payload(server_socket, &task, sizeof(task)) != sizeof(task)) {
                    pthread_mutex_unlock(&socket_mutex);
                    LOG_ERROR("Failed to receive task data: %s", strerror(errno));
                    continue;
                }
                
                pthread_mutex_unlock(&socket_mutex);
                
                LOG_INFO("Received task %u from central server", header.client_id);
                
                /* Process the task */
                task.task_id = header.client_id;
                if (process_task(&task, &result) != 0) {
                    LOG_ERROR("Failed to process task %u", task.task_id);
                    result.status = STATUS_ERROR;
                }
                
                /* Send task result back to central server */
                if (send_task_result(server_socket, &result) != 0) {
                    LOG_ERROR("Failed to send result for task %u", task.task_id);
                }
                break;
                
            case MSG_TYPE_SHUTDOWN:
                pthread_mutex_unlock(&socket_mutex);
                LOG_INFO("Received shutdown request from central server");
                worker_running = 0;
                break;
                
            default:
                pthread_mutex_unlock(&socket_mutex);
                LOG_WARNING("Received unknown message type %u", header.message_type);
                break;
        }
    }
    
    /* Wait for heartbeat thread to finish */
    pthread_cancel(heartbeat_thread_id);
    pthread_join(heartbeat_thread_id, NULL);
    
    /* Close connection */
    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }
    
    LOG_INFO("Worker server shutting down");
    logger_close();
    
    return EXIT_SUCCESS;
}
