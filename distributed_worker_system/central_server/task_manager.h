/**
 * task_manager.h - Task Manager
 * 
 * This file provides functions for managing tasks in the central server.
 */

#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include "../lib/include/ring_buffer.h"
#include "../lib/include/protocol.h"

/* Task status */
typedef enum {
    TASK_STATUS_PENDING = 0,  /* Task is in queue, not assigned yet */
    TASK_STATUS_ASSIGNED,     /* Task is assigned to a worker */
    TASK_STATUS_COMPLETED,    /* Task is completed successfully */
    TASK_STATUS_FAILED,       /* Task failed */
    TASK_STATUS_TIMEOUT       /* Task timed out */
} task_status_t;

/* Task information structure */
typedef struct {
    task_t task;              /* Task data */
    uint32_t worker_id;       /* Worker ID assigned to this task (0 if none) */
    task_status_t status;     /* Current task status */
    time_t created_time;      /* Time when task was created */
    time_t assigned_time;     /* Time when task was assigned */
    time_t completed_time;    /* Time when task was completed */
    uint32_t exec_time_ms;    /* Execution time in milliseconds */
} task_info_t;

/* Task manager structure */
typedef struct {
    task_info_t *tasks;       /* Array of task information */
    uint32_t capacity;        /* Maximum number of tasks */
    uint32_t count;           /* Current number of tasks */
    uint32_t next_task_id;    /* Next task ID to assign */
    ring_buffer_t *task_queue; /* Queue of pending tasks */
    pthread_mutex_t mutex;    /* Mutex for thread safety */
} task_manager_t;

/**
 * Create a new task manager
 * 
 * @param capacity Maximum number of tasks
 * @param queue_size Size of the task queue
 * @return Pointer to the new task manager or NULL on error
 */
task_manager_t *task_manager_create(uint32_t capacity, uint32_t queue_size);

/**
 * Destroy a task manager and free its resources
 * 
 * @param manager Pointer to the task manager
 */
void task_manager_destroy(task_manager_t *manager);

/**
 * Create a new task
 * 
 * @param manager Pointer to the task manager
 * @param input_data Input data for the task
 * @param input_size Size of input data
 * @return Task ID on success, 0 on error
 */
uint32_t task_manager_create_task(task_manager_t *manager, 
                                 const uint8_t *input_data, 
                                 uint32_t input_size);

/**
 * Get the next pending task from the queue
 * 
 * @param manager Pointer to the task manager
 * @param timeout_ms Timeout in milliseconds, 0 for non-blocking, -1 for infinite
 * @return Pointer to task or NULL if no tasks available
 */
task_t *task_manager_get_next_task(task_manager_t *manager, int timeout_ms);

/**
 * Mark a task as assigned to a worker
 * 
 * @param manager Pointer to the task manager
 * @param task_id Task ID
 * @param worker_id Worker ID
 * @return 0 on success, -1 if task not found
 */
int task_manager_assign_task(task_manager_t *manager, 
                            uint32_t task_id, 
                            uint32_t worker_id);

/**
 * Process task result from a worker
 * 
 * @param manager Pointer to the task manager
 * @param task_id Task ID
 * @param status Result status
 * @param exec_time_ms Execution time in milliseconds
 * @return 0 on success, -1 if task not found
 */
int task_manager_process_result(task_manager_t *manager, 
                               uint32_t task_id, 
                               uint8_t status, 
                               uint32_t exec_time_ms);

/**
 * Get task information by ID
 * 
 * @param manager Pointer to the task manager
 * @param task_id Task ID
 * @return Pointer to task info or NULL if not found
 */
task_info_t *task_manager_get_task(task_manager_t *manager, uint32_t task_id);

/**
 * Get the number of tasks in the manager
 * 
 * @param manager Pointer to the task manager
 * @return Number of tasks
 */
uint32_t task_manager_count(task_manager_t *manager);

/**
 * Get the number of pending tasks in the queue
 * 
 * @param manager Pointer to the task manager
 * @return Number of pending tasks
 */
uint32_t task_manager_pending_count(task_manager_t *manager);

/**
 * Check for timed out tasks and mark them as timed out
 * 
 * @param manager Pointer to the task manager
 * @param timeout_sec Timeout in seconds
 * @return Number of tasks marked as timed out
 */
uint32_t task_manager_check_timeouts(task_manager_t *manager, uint32_t timeout_sec);

#endif /* TASK_MANAGER_H */
