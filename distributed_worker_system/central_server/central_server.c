/**
 * central_server.c - Central Server Implementation
 * 
 * This file contains the main function and worker thread handlers for the central server.
 */

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

#include "../common/comm_protocol.h"
#include "../common/net_utils.h"
#include "../common/logger.h"
#include "worker_registry.h"
#include "task_manager.h"

/* Configuration constants */
#define SERVER_PORT 8888
#define MAX_WORKERS 100
#define MAX_TASKS 1000
#define TASK_QUEUE_SIZE 500
#define WORKER_TIMEOUT_SEC 30
#define TASK_TIMEOUT_SEC 60
#define TIMEOUT_CHECK_INTERVAL_SEC 5

/* Global variables for server state */
static int server_running = 1;
static int server_socket = -1;
static worker_registry_t *worker_registry = NULL;
static task_manager_t *task_manager = NULL;

/* Forward declarations */
void *worker_handler_thread(void *arg);
void *timeout_checker_thread(void *arg);
void signal_handler(int sig);

/* Initialize the server */
int initialize_server(void) {
    /* Initialize logger */
    if (logger_init("CentralServer", "central_server.log", LOG_INFO) != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return -1;
    }
    
    LOG_INFO("Central server starting...");
    
    /* Create worker registry */
    worker_registry = worker_registry_create(MAX_WORKERS);
    if (worker_registry == NULL) {
        LOG_ERROR("Failed to create worker registry");
        return -1;
    }
    
    /* Create task manager */
    task_manager = task_manager_create(MAX_TASKS, TASK_QUEUE_SIZE);
    if (task_manager == NULL) {
        LOG_ERROR("Failed to create task manager");
        worker_registry_destroy(worker_registry);
        return -1;
    }
    
    /* Create server socket */
    server_socket = create_server_socket(SERVER_PORT, 10);
    if (server_socket < 0) {
        LOG_ERROR("Failed to create server socket");
        task_manager_destroy(task_manager);
        worker_registry_destroy(worker_registry);
        return -1;
    }
    
    LOG_INFO("Server socket created on port %d", SERVER_PORT);
    
    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    return 0;
}

/* Clean up server resources */
void cleanup_server(void) {
    LOG_INFO("Shutting down central server...");
    
    /* Close server socket */
    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }
    
    /* Clean up task manager */
    if (task_manager != NULL) {
        task_manager_destroy(task_manager);
        task_manager = NULL;
    }
    
    /* Clean up worker registry */
    if (worker_registry != NULL) {
        worker_registry_destroy(worker_registry);
        worker_registry = NULL;
    }
    
    /* Close logger */
    logger_close();
}

/* Handle worker registration */
uint32_t handle_worker_registration(int sockfd, pthread_t thread_id) {
    uint32_t worker_id;
    
    /* Add worker to registry */
    worker_id = worker_registry_add(worker_registry, sockfd, thread_id);
    if (worker_id == 0) {
        LOG_ERROR("Failed to register worker");
        return 0;
    }
    
    LOG_INFO("Worker %u registered", worker_id);
    
    /* Send registration response */
    if (send_message(sockfd, MSG_TYPE_REGISTER_RESPONSE, worker_id, NULL, 0) < 0) {
        LOG_ERROR("Failed to send registration response to worker %u", worker_id);
        worker_registry_remove(worker_registry, worker_id);
        return 0;
    }
    
    return worker_id;
}

/* Handle worker heartbeat */
void handle_worker_heartbeat(uint32_t worker_id, int sockfd) {
    /* Update worker heartbeat time */
    if (worker_registry_update_heartbeat(worker_registry, worker_id) != 0) {
        LOG_WARNING("Failed to update heartbeat for worker %u", worker_id);
        return;
    }
    
    /* Send heartbeat response */
    if (send_message(sockfd, MSG_TYPE_HEARTBEAT_RESPONSE, worker_id, NULL, 0) < 0) {
        LOG_WARNING("Failed to send heartbeat response to worker %u", worker_id);
    }
}

/* Handle task result */
void handle_task_result(uint32_t worker_id, int sockfd, const msg_header_t *header) {
    task_result_t result;
    worker_info_t *worker;
    
    /* Receive task result payload */
    if (recv_payload(sockfd, &result, sizeof(result)) != sizeof(result)) {
        LOG_ERROR("Failed to receive task result from worker %u", worker_id);
        return;
    }
    
    LOG_INFO("Received result for task %u from worker %u: status=%u, exec_time=%u ms",
             result.task_id, worker_id, result.status, result.exec_time_ms);
    
    /* Update task status */
    if (task_manager_process_result(task_manager, result.task_id, 
                                   result.status, result.exec_time_ms) != 0) {
        LOG_WARNING("Failed to process result for task %u", result.task_id);
    }
    
    /* Update worker status */
    worker = worker_registry_get(worker_registry, worker_id);
    if (worker != NULL) {
        worker_registry_update_status(worker_registry, worker_id, WORKER_STATUS_IDLE);
        
        /* Update worker statistics */
        if (result.status == STATUS_SUCCESS) {
            worker->tasks_completed++;
        } else {
            worker->tasks_failed++;
        }
    }
    
    /* Send result acknowledgment */
    if (send_message(sockfd, MSG_TYPE_RESULT_ACK, result.task_id, NULL, 0) < 0) {
        LOG_WARNING("Failed to send result acknowledgment to worker %u", worker_id);
    }
}

/* Assign a task to a worker */
int assign_task_to_worker(uint32_t worker_id) {
    worker_info_t *worker;
    task_t *task;
    
    /* Get worker info */
    worker = worker_registry_get(worker_registry, worker_id);
    if (worker == NULL || worker->status != WORKER_STATUS_IDLE) {
        return -1;
    }
    
    /* Get next task from queue */
    task = task_manager_get_next_task(task_manager, 0);
    if (task == NULL) {
        return -1; /* No tasks available */
    }
    
    LOG_INFO("Assigning task %u to worker %u", task->task_id, worker_id);
    
    /* Mark task as assigned */
    if (task_manager_assign_task(task_manager, task->task_id, worker_id) != 0) {
        LOG_ERROR("Failed to mark task %u as assigned", task->task_id);
        free(task);
        return -1;
    }
    
    /* Update worker status */
    worker_registry_update_status(worker_registry, worker_id, WORKER_STATUS_BUSY);
    
    /* Send task to worker */
    if (send_message(worker->sockfd, MSG_TYPE_TASK, task->task_id, 
                    task->input_data, MAX_DATA_SIZE) < 0) {
        LOG_ERROR("Failed to send task %u to worker %u", task->task_id, worker_id);
        worker_registry_update_status(worker_registry, worker_id, WORKER_STATUS_IDLE);
        free(task);
        return -1;
    }
    
    free(task);
    return 0;
}

/* Worker handler thread function */
void *worker_handler_thread(void *arg) {
    int sockfd = *((int *)arg);
    uint32_t worker_id = 0;
    msg_header_t header;
    int result;
    
    free(arg); /* Free the socket descriptor pointer */
    
    /* Set socket timeout */
    set_socket_timeout(sockfd, WORKER_TIMEOUT_SEC);
    
    LOG_INFO("New worker connection handler started");
    
    /* Wait for worker registration */
    if (recv_header(sockfd, &header) != 0) {
        LOG_ERROR("Failed to receive registration from worker");
        close(sockfd);
        return NULL;
    }
    
    if (header.msg_type != MSG_TYPE_REGISTER) {
        LOG_ERROR("Expected registration message, got type %u", header.msg_type);
        close(sockfd);
        return NULL;
    }
    
    /* Register the worker */
    worker_id = handle_worker_registration(sockfd, pthread_self());
    if (worker_id == 0) {
        close(sockfd);
        return NULL;
    }
    
    /* Main worker communication loop */
    while (server_running) {
        /* Receive message header */
        result = recv_header(sockfd, &header);
        if (result != 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_WARNING("Worker %u timed out", worker_id);
            } else {
                LOG_ERROR("Failed to receive message from worker %u: %s", 
                         worker_id, strerror(errno));
            }
            break;
        }
        
        /* Process message based on type */
        switch (header.msg_type) {
            case MSG_TYPE_HEARTBEAT:
                handle_worker_heartbeat(worker_id, sockfd);
                
                /* Check if there are pending tasks to assign */
                if (task_manager_pending_count(task_manager) > 0) {
                    assign_task_to_worker(worker_id);
                }
                break;
                
            case MSG_TYPE_TASK_RESULT:
                handle_task_result(worker_id, sockfd, &header);
                
                /* Check if there are more tasks to assign */
                if (task_manager_pending_count(task_manager) > 0) {
                    assign_task_to_worker(worker_id);
                }
                break;
                
            default:
                LOG_WARNING("Received unknown message type %u from worker %u", 
                           header.msg_type, worker_id);
                break;
        }
    }
    
    /* Worker disconnected or server shutting down */
    LOG_INFO("Worker %u disconnected", worker_id);
    
    /* Remove worker from registry */
    worker_registry_remove(worker_registry, worker_id);
    
    return NULL;
}

/* Timeout checker thread function */
void *timeout_checker_thread(void *arg) {
    uint32_t worker_timeouts, task_timeouts;
    
    (void)arg; /* Unused parameter */
    
    LOG_INFO("Timeout checker thread started");
    
    while (server_running) {
        /* Sleep for check interval */
        sleep(TIMEOUT_CHECK_INTERVAL_SEC);
        
        /* Check for worker timeouts */
        worker_timeouts = worker_registry_check_timeouts(worker_registry, WORKER_TIMEOUT_SEC);
        if (worker_timeouts > 0) {
            LOG_WARNING("%u workers timed out", worker_timeouts);
        }
        
        /* Check for task timeouts */
        task_timeouts = task_manager_check_timeouts(task_manager, TASK_TIMEOUT_SEC);
        if (task_timeouts > 0) {
            LOG_WARNING("%u tasks timed out", task_timeouts);
        }
    }
    
    LOG_INFO("Timeout checker thread stopped");
    return NULL;
}

/* Signal handler function */
void signal_handler(int sig) {
    LOG_INFO("Received signal %d, shutting down...", sig);
    server_running = 0;
}

/* Create a sample task for testing */
void create_sample_task(void) {
    uint8_t sample_data[MAX_DATA_SIZE];
    uint32_t task_id;
    
    /* Initialize sample data */
    memset(sample_data, 0, MAX_DATA_SIZE);
    strcpy((char *)sample_data, "Sample task data");
    
    /* Create task */
    task_id = task_manager_create_task(task_manager, sample_data, strlen((char *)sample_data) + 1);
    if (task_id > 0) {
        LOG_INFO("Created sample task with ID %u", task_id);
    } else {
        LOG_ERROR("Failed to create sample task");
    }
}

/* Main function */
int main(void) {
    int client_socket;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t timeout_thread;
    int *client_sock_ptr;
    
    /* Initialize server */
    if (initialize_server() != 0) {
        fprintf(stderr, "Failed to initialize server\n");
        return EXIT_FAILURE;
    }
    
    /* Create timeout checker thread */
    if (pthread_create(&timeout_thread, NULL, timeout_checker_thread, NULL) != 0) {
        LOG_ERROR("Failed to create timeout checker thread");
        cleanup_server();
        return EXIT_FAILURE;
    }
    
    /* Create a sample task for testing */
    create_sample_task();
    
    LOG_INFO("Central server running on port %d", SERVER_PORT);
    
    /* Main server loop */
    while (server_running) {
        /* Accept new connection */
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            if (errno == EINTR) {
                /* Interrupted by signal, check if server is still running */
                continue;
            }
            LOG_ERROR("Failed to accept connection: %s", strerror(errno));
            continue;
        }
        
        LOG_INFO("New connection from %s:%d", 
                inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        /* Allocate memory for client socket descriptor */
        client_sock_ptr = (int *)malloc(sizeof(int));
        if (client_sock_ptr == NULL) {
            LOG_ERROR("Failed to allocate memory for client socket");
            close(client_socket);
            continue;
        }
        
        *client_sock_ptr = client_socket;
        
        /* Create worker handler thread */
        pthread_t worker_thread;
        if (pthread_create(&worker_thread, NULL, worker_handler_thread, client_sock_ptr) != 0) {
            LOG_ERROR("Failed to create worker handler thread");
            free(client_sock_ptr);
            close(client_socket);
            continue;
        }
        
        /* Detach thread so its resources are automatically released when it terminates */
        pthread_detach(worker_thread);
    }
    
    /* Wait for timeout checker thread to finish */
    pthread_cancel(timeout_thread);
    pthread_join(timeout_thread, NULL);
    
    /* Clean up server resources */
    cleanup_server();
    
    return EXIT_SUCCESS;
}