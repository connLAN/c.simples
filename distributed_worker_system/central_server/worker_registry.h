/**
 * worker_registry.h - Worker Registry
 * 
 * This file provides functions for managing worker connections and status.
 */

#ifndef WORKER_REGISTRY_H
#define WORKER_REGISTRY_H

#include <stdint.h>
#include <pthread.h>
#include <time.h>

/* Worker status */
typedef enum {
    WORKER_STATUS_IDLE = 0,     /* Worker is idle and ready for tasks */
    WORKER_STATUS_BUSY,         /* Worker is processing a task */
    WORKER_STATUS_DISCONNECTED  /* Worker is disconnected */
} worker_status_t;

/* Worker information structure */
typedef struct {
    uint32_t worker_id;         /* Unique worker identifier */
    int sockfd;                 /* Socket file descriptor */
    worker_status_t status;     /* Current worker status */
    time_t last_heartbeat;      /* Timestamp of last heartbeat */
    uint32_t tasks_completed;   /* Number of tasks completed */
    uint32_t tasks_failed;      /* Number of tasks failed */
    pthread_t thread_id;        /* Worker handler thread ID */
} worker_info_t;

/* Worker registry structure */
typedef struct {
    worker_info_t *workers;     /* Array of worker information */
    uint32_t capacity;          /* Maximum number of workers */
    uint32_t count;             /* Current number of workers */
    pthread_mutex_t mutex;      /* Mutex for thread safety */
} worker_registry_t;

/**
 * Create a new worker registry
 * 
 * @param capacity Maximum number of workers
 * @return Pointer to the new worker registry or NULL on error
 */
worker_registry_t *worker_registry_create(uint32_t capacity);

/**
 * Destroy a worker registry and free its resources
 * 
 * @param registry Pointer to the worker registry
 */
void worker_registry_destroy(worker_registry_t *registry);

/**
 * Add a new worker to the registry
 * 
 * @param registry Pointer to the worker registry
 * @param sockfd Socket file descriptor for the worker connection
 * @param thread_id Thread ID handling this worker
 * @return Worker ID on success, 0 on error
 */
uint32_t worker_registry_add(worker_registry_t *registry, int sockfd, pthread_t thread_id);

/**
 * Remove a worker from the registry
 * 
 * @param registry Pointer to the worker registry
 * @param worker_id Worker ID to remove
 * @return 0 on success, -1 if worker not found
 */
int worker_registry_remove(worker_registry_t *registry, uint32_t worker_id);

/**
 * Get worker information by ID
 * 
 * @param registry Pointer to the worker registry
 * @param worker_id Worker ID to find
 * @return Pointer to worker info or NULL if not found
 */
worker_info_t *worker_registry_get(worker_registry_t *registry, uint32_t worker_id);

/**
 * Update worker status
 * 
 * @param registry Pointer to the worker registry
 * @param worker_id Worker ID to update
 * @param status New worker status
 * @return 0 on success, -1 if worker not found
 */
int worker_registry_update_status(worker_registry_t *registry, 
                                 uint32_t worker_id, 
                                 worker_status_t status);

/**
 * Update worker heartbeat time
 * 
 * @param registry Pointer to the worker registry
 * @param worker_id Worker ID to update
 * @return 0 on success, -1 if worker not found
 */
int worker_registry_update_heartbeat(worker_registry_t *registry, uint32_t worker_id);

/**
 * Find an idle worker
 * 
 * @param registry Pointer to the worker registry
 * @return Worker ID of an idle worker, or 0 if none available
 */
uint32_t worker_registry_find_idle(worker_registry_t *registry);

/**
 * Get the number of workers in the registry
 * 
 * @param registry Pointer to the worker registry
 * @return Number of workers
 */
uint32_t worker_registry_count(worker_registry_t *registry);

/**
 * Check for timed out workers and mark them as disconnected
 * 
 * @param registry Pointer to the worker registry
 * @param timeout_sec Timeout in seconds
 * @return Number of workers marked as disconnected
 */
uint32_t worker_registry_check_timeouts(worker_registry_t *registry, uint32_t timeout_sec);

#endif /* WORKER_REGISTRY_H */