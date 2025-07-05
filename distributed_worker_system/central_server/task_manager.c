/**
 * task_manager.c - Task Manager Implementation
 */

#include "task_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Create a new task manager */
task_manager_t *task_manager_create(uint32_t capacity, uint32_t queue_size) {
    task_manager_t *manager;
    
    if (capacity == 0 || queue_size == 0) {
        return NULL;
    }
    
    manager = (task_manager_t *)malloc(sizeof(task_manager_t));
    if (manager == NULL) {
        return NULL;
    }
    
    manager->tasks = (task_info_t *)malloc(capacity * sizeof(task_info_t));
    if (manager->tasks == NULL) {
        free(manager);
        return NULL;
    }
    
    manager->task_queue = ring_buffer_create(queue_size);
    if (manager->task_queue == NULL) {
        free(manager->tasks);
        free(manager);
        return NULL;
    }
    
    manager->capacity = capacity;
    manager->count = 0;
    manager->next_task_id = 1; /* Task IDs start from 1 */
    
    if (pthread_mutex_init(&manager->mutex, NULL) != 0) {
        ring_buffer_destroy(manager->task_queue, 0);
        free(manager->tasks);
        free(manager);
        return NULL;
    }
    
    return manager;
}

/* Destroy a task manager and free its resources */
void task_manager_destroy(task_manager_t *manager) {
    if (manager == NULL) {
        return;
    }
    
    pthread_mutex_lock(&manager->mutex);
    
    /* Free task queue */
    if (manager->task_queue != NULL) {
        ring_buffer_destroy(manager->task_queue, 1); /* Free elements in queue */
    }
    
    pthread_mutex_unlock(&manager->mutex);
    
    pthread_mutex_destroy(&manager->mutex);
    free(manager->tasks);
    free(manager);
}

/* Create a new task */
uint32_t task_manager_create_task(task_manager_t *manager, 
                                 const uint8_t *input_data, 
                                 uint32_t input_size) {
    task_info_t *task_info;
    task_t *task;
    uint32_t task_id;
    
    if (manager == NULL || input_data == NULL || 
        input_size == 0 || input_size > MAX_DATA_SIZE) {
        return 0;
    }
    
    pthread_mutex_lock(&manager->mutex);
    
    /* Check if manager is full */
    if (manager->count >= manager->capacity) {
        pthread_mutex_unlock(&manager->mutex);
        return 0;
    }
    
    /* Allocate a new task */
    task = (task_t *)malloc(sizeof(task_t));
    if (task == NULL) {
        pthread_mutex_unlock(&manager->mutex);
        return 0;
    }
    
    /* Initialize task */
    task_id = manager->next_task_id++;
    task->task_id = task_id;
    memcpy(task->input_data, input_data, input_size);
    memset(task->output_data, 0, MAX_DATA_SIZE);
    
    /* Add task to the queue */
    if (ring_buffer_push(manager->task_queue, task, 0) != 0) {
        free(task);
        pthread_mutex_unlock(&manager->mutex);
        return 0;
    }
    
    /* Initialize task info */
    task_info = &manager->tasks[manager->count];
    memcpy(&task_info->task, task, sizeof(task_t));
    task_info->worker_id = 0;
    task_info->status = TASK_STATUS_PENDING;
    task_info->created_time = time(NULL);
    task_info->assigned_time = 0;
    task_info->completed_time = 0;
    task_info->exec_time_ms = 0;
    
    manager->count++;
    
    pthread_mutex_unlock(&manager->mutex);
    return task_id;
}

/* Get the next pending task from the queue */
task_t *task_manager_get_next_task(task_manager_t *manager, int timeout_ms) {
    task_t *task;
    
    if (manager == NULL) {
        return NULL;
    }
    
    /* Get task from queue (no need for mutex, ring buffer is thread-safe) */
    task = (task_t *)ring_buffer_pop(manager->task_queue, timeout_ms);
    
    return task;
}

/* Mark a task as assigned to a worker */
int task_manager_assign_task(task_manager_t *manager, 
                            uint32_t task_id, 
                            uint32_t worker_id) {
    uint32_t i;
    int found = 0;
    
    if (manager == NULL || task_id == 0 || worker_id == 0) {
        return -1;
    }
    
    pthread_mutex_lock(&manager->mutex);
    
    /* Find the task */
    for (i = 0; i < manager->count; i++) {
        if (manager->tasks[i].task.task_id == task_id) {
            manager->tasks[i].worker_id = worker_id;
            manager->tasks[i].status = TASK_STATUS_ASSIGNED;
            manager->tasks[i].assigned_time = time(NULL);
            found = 1;
            break;
        }
    }
    
    pthread_mutex_unlock(&manager->mutex);
    
    return found ? 0 : -1;
}

/* Process task result from a worker */
int task_manager_process_result(task_manager_t *manager, 
                               uint32_t task_id, 
                               uint8_t status, 
                               uint32_t exec_time_ms) {
    uint32_t i;
    int found = 0;
    
    if (manager == NULL || task_id == 0) {
        return -1;
    }
    
    pthread_mutex_lock(&manager->mutex);
    
    /* Find the task */
    for (i = 0; i < manager->count; i++) {
        if (manager->tasks[i].task.task_id == task_id) {
            manager->tasks[i].status = (status == STATUS_SUCCESS) ? 
                                      TASK_STATUS_COMPLETED : TASK_STATUS_FAILED;
            manager->tasks[i].completed_time = time(NULL);
            manager->tasks[i].exec_time_ms = exec_time_ms;
            found = 1;
            break;
        }
    }
    
    pthread_mutex_unlock(&manager->mutex);
    
    return found ? 0 : -1;
}

/* Get task information by ID */
task_info_t *task_manager_get_task(task_manager_t *manager, uint32_t task_id) {
    uint32_t i;
    task_info_t *task_info = NULL;
    
    if (manager == NULL || task_id == 0) {
        return NULL;
    }
    
    pthread_mutex_lock(&manager->mutex);
    
    /* Find the task */
    for (i = 0; i < manager->count; i++) {
        if (manager->tasks[i].task.task_id == task_id) {
            task_info = &manager->tasks[i];
            break;
        }
    }
    
    pthread_mutex_unlock(&manager->mutex);
    
    return task_info;
}

/* Get the number of tasks in the manager */
uint32_t task_manager_count(task_manager_t *manager) {
    uint32_t count;
    
    if (manager == NULL) {
        return 0;
    }
    
    pthread_mutex_lock(&manager->mutex);
    count = manager->count;
    pthread_mutex_unlock(&manager->mutex);
    
    return count;
}

/* Get the number of pending tasks in the queue */
uint32_t task_manager_pending_count(task_manager_t *manager) {
    if (manager == NULL || manager->task_queue == NULL) {
        return 0;
    }
    
    return ring_buffer_size(manager->task_queue);
}

/* Check for timed out tasks and mark them as timed out */
uint32_t task_manager_check_timeouts(task_manager_t *manager, uint32_t timeout_sec) {
    uint32_t i;
    uint32_t count = 0;
    time_t now;
    
    if (manager == NULL) {
        return 0;
    }
    
    now = time(NULL);
    
    pthread_mutex_lock(&manager->mutex);
    
    /* Check each assigned task for timeout */
    for (i = 0; i < manager->count; i++) {
        if (manager->tasks[i].status == TASK_STATUS_ASSIGNED &&
            manager->tasks[i].assigned_time > 0 &&
            (now - manager->tasks[i].assigned_time) > timeout_sec) {
            manager->tasks[i].status = TASK_STATUS_TIMEOUT;
            count++;
        }
    }
    
    pthread_mutex_unlock(&manager->mutex);
    
    return count;
}