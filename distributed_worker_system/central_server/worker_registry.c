/**
 * worker_registry.c - Worker Registry Implementation
 */

#include "worker_registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

/* Create a new worker registry */
worker_registry_t *worker_registry_create(uint32_t capacity) {
    worker_registry_t *registry;
    
    if (capacity == 0) {
        return NULL;
    }
    
    registry = (worker_registry_t *)malloc(sizeof(worker_registry_t));
    if (registry == NULL) {
        return NULL;
    }
    
    registry->workers = (worker_info_t *)malloc(capacity * sizeof(worker_info_t));
    if (registry->workers == NULL) {
        free(registry);
        return NULL;
    }
    
    registry->capacity = capacity;
    registry->count = 0;
    
    if (pthread_mutex_init(&registry->mutex, NULL) != 0) {
        free(registry->workers);
        free(registry);
        return NULL;
    }
    
    return registry;
}

/* Destroy a worker registry and free its resources */
void worker_registry_destroy(worker_registry_t *registry) {
    uint32_t i;
    
    if (registry == NULL) {
        return;
    }
    
    pthread_mutex_lock(&registry->mutex);
    
    /* Close all worker sockets */
    for (i = 0; i < registry->count; i++) {
        if (registry->workers[i].sockfd >= 0) {
            close(registry->workers[i].sockfd);
        }
    }
    
    pthread_mutex_unlock(&registry->mutex);
    
    pthread_mutex_destroy(&registry->mutex);
    free(registry->workers);
    free(registry);
}

/* Add a new worker to the registry */
uint32_t worker_registry_add(worker_registry_t *registry, int sockfd, pthread_t thread_id) {
    uint32_t worker_id;
    
    if (registry == NULL || sockfd < 0) {
        return 0;
    }
    
    pthread_mutex_lock(&registry->mutex);
    
    /* Check if registry is full */
    if (registry->count >= registry->capacity) {
        pthread_mutex_unlock(&registry->mutex);
        return 0;
    }
    
    /* Generate worker ID (1-based, 0 is reserved for error) */
    worker_id = registry->count + 1;
    
    /* Initialize worker info */
    registry->workers[registry->count].worker_id = worker_id;
    registry->workers[registry->count].sockfd = sockfd;
    registry->workers[registry->count].status = WORKER_STATUS_IDLE;
    registry->workers[registry->count].last_heartbeat = time(NULL);
    registry->workers[registry->count].tasks_completed = 0;
    registry->workers[registry->count].tasks_failed = 0;
    registry->workers[registry->count].thread_id = thread_id;
    
    registry->count++;
    
    pthread_mutex_unlock(&registry->mutex);
    return worker_id;
}

/* Remove a worker from the registry */
int worker_registry_remove(worker_registry_t *registry, uint32_t worker_id) {
    uint32_t i;
    int found = 0;
    
    if (registry == NULL || worker_id == 0) {
        return -1;
    }
    
    pthread_mutex_lock(&registry->mutex);
    
    /* Find the worker */
    for (i = 0; i < registry->count; i++) {
        if (registry->workers[i].worker_id == worker_id) {
            found = 1;
            break;
        }
    }
    
    if (!found) {
        pthread_mutex_unlock(&registry->mutex);
        return -1;
    }
    
    /* Close socket */
    if (registry->workers[i].sockfd >= 0) {
        close(registry->workers[i].sockfd);
    }
    
    /* Remove worker by shifting remaining workers */
    if (i < registry->count - 1) {
        memmove(&registry->workers[i], &registry->workers[i + 1], 
                (registry->count - i - 1) * sizeof(worker_info_t));
    }
    
    registry->count--;
    
    pthread_mutex_unlock(&registry->mutex);
    return 0;
}

/* Get worker information by ID */
worker_info_t *worker_registry_get(worker_registry_t *registry, uint32_t worker_id) {
    uint32_t i;
    worker_info_t *worker = NULL;
    
    if (registry == NULL || worker_id == 0) {
        return NULL;
    }
    
    pthread_mutex_lock(&registry->mutex);
    
    /* Find the worker */
    for (i = 0; i < registry->count; i++) {
        if (registry->workers[i].worker_id == worker_id) {
            worker = &registry->workers[i];
            break;
        }
    }
    
    pthread_mutex_unlock(&registry->mutex);
    return worker;
}

/* Update worker status */
int worker_registry_update_status(worker_registry_t *registry, 
                                 uint32_t worker_id, 
                                 worker_status_t status) {
    uint32_t i;
    int found = 0;
    
    if (registry == NULL || worker_id == 0) {
        return -1;
    }
    
    pthread_mutex_lock(&registry->mutex);
    
    /* Find the worker */
    for (i = 0; i < registry->count; i++) {
        if (registry->workers[i].worker_id == worker_id) {
            registry->workers[i].status = status;
            found = 1;
            break;
        }
    }
    
    pthread_mutex_unlock(&registry->mutex);
    
    return found ? 0 : -1;
}

/* Update worker heartbeat time */
int worker_registry_update_heartbeat(worker_registry_t *registry, uint32_t worker_id) {
    uint32_t i;
    int found = 0;
    
    if (registry == NULL || worker_id == 0) {
        return -1;
    }
    
    pthread_mutex_lock(&registry->mutex);
    
    /* Find the worker */
    for (i = 0; i < registry->count; i++) {
        if (registry->workers[i].worker_id == worker_id) {
            registry->workers[i].last_heartbeat = time(NULL);
            found = 1;
            break;
        }
    }
    
    pthread_mutex_unlock(&registry->mutex);
    
    return found ? 0 : -1;
}

/* Find an idle worker */
uint32_t worker_registry_find_idle(worker_registry_t *registry) {
    uint32_t i;
    uint32_t worker_id = 0;
    
    if (registry == NULL) {
        return 0;
    }
    
    pthread_mutex_lock(&registry->mutex);
    
    /* Find an idle worker */
    for (i = 0; i < registry->count; i++) {
        if (registry->workers[i].status == WORKER_STATUS_IDLE) {
            worker_id = registry->workers[i].worker_id;
            break;
        }
    }
    
    pthread_mutex_unlock(&registry->mutex);
    
    return worker_id;
}

/* Get the number of workers in the registry */
uint32_t worker_registry_count(worker_registry_t *registry) {
    uint32_t count;
    
    if (registry == NULL) {
        return 0;
    }
    
    pthread_mutex_lock(&registry->mutex);
    count = registry->count;
    pthread_mutex_unlock(&registry->mutex);
    
    return count;
}

/* Check for timed out workers and mark them as disconnected */
uint32_t worker_registry_check_timeouts(worker_registry_t *registry, uint32_t timeout_sec) {
    uint32_t i;
    uint32_t count = 0;
    time_t now;
    
    if (registry == NULL) {
        return 0;
    }
    
    now = time(NULL);
    
    pthread_mutex_lock(&registry->mutex);
    
    /* Check each worker's last heartbeat time */
    for (i = 0; i < registry->count; i++) {
        if (registry->workers[i].status != WORKER_STATUS_DISCONNECTED &&
            (now - registry->workers[i].last_heartbeat) > timeout_sec) {
            registry->workers[i].status = WORKER_STATUS_DISCONNECTED;
            count++;
        }
    }
    
    pthread_mutex_unlock(&registry->mutex);
    
    return count;
}