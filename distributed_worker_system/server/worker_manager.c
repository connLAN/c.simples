/**
 * @file worker_manager.c
 * @brief Worker manager implementation for distributed worker system
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#include "worker_manager.h"
#include "../common/logger.h"
#include "../common/protocol.h"
#include "../common/common.h"

/* Worker array */
static Worker *g_workers = NULL;
static int g_max_workers = 0;
static int g_next_worker_id = 1;

/* Statistics */
static int g_total_workers = 0;
static int g_idle_workers = 0;
static int g_busy_workers = 0;

/* Mutex for thread safety */
static pthread_mutex_t g_worker_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Helper functions */
static long long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static Worker *find_worker_by_id(int worker_id) {
    if (worker_id <= 0 || worker_id >= g_next_worker_id) {
        return NULL;
    }
    
    for (int i = 0; i < g_max_workers; i++) {
        if (g_workers[i].worker_id == worker_id) {
            return &g_workers[i];
        }
    }
    
    return NULL;
}

static int find_free_worker_slot() {
    for (int i = 0; i < g_max_workers; i++) {
        if (g_workers[i].worker_id == 0) {
            return i;
        }
    }
    
    return -1;
}

int worker_manager_init(int max_workers) {
    if (max_workers <= 0) {
        log_error("Invalid max_workers value: %d", max_workers);
        return -1;
    }
    
    /* Allocate worker array */
    g_workers = (Worker *)calloc(max_workers, sizeof(Worker));
    if (!g_workers) {
        log_error("Failed to allocate memory for workers");
        return -1;
    }
    
    g_max_workers = max_workers;
    g_next_worker_id = 1;
    
    log_info("Worker manager initialized with max_workers=%d", max_workers);
    
    return 0;
}

int worker_manager_cleanup() {
    pthread_mutex_lock(&g_worker_mutex);
    
    /* Free worker array */
    free(g_workers);
    g_workers = NULL;
    g_max_workers = 0;
    
    pthread_mutex_unlock(&g_worker_mutex);
    
    log_info("Worker manager cleaned up");
    
    return 0;
}

int worker_manager_register(int socket, const char *ip_address, int port,
                           const int *job_types_supported, int num_job_types) {
    if (!ip_address || !job_types_supported || num_job_types <= 0 || 
        num_job_types > MAX_JOB_TYPES) {
        log_error("Invalid worker registration parameters");
        return -1;
    }
    
    pthread_mutex_lock(&g_worker_mutex);
    
    /* Find free worker slot */
    int slot = find_free_worker_slot();
    if (slot < 0) {
        pthread_mutex_unlock(&g_worker_mutex);
        log_error("No free worker slots available");
        return -1;
    }
    
    /* Initialize worker */
    Worker *worker = &g_workers[slot];
    worker->worker_id = g_next_worker_id++;
    worker->socket = socket;
    worker->status = WORKER_STATUS_IDLE;
    worker->current_job_id = -1;
    strncpy(worker->ip_address, ip_address, sizeof(worker->ip_address) - 1);
    worker->port = port;
    worker->last_heartbeat = get_current_time_ms();
    
    /* Copy supported job types */
    memset(worker->job_types_supported, 0, sizeof(worker->job_types_supported));
    for (int i = 0; i < num_job_types && i < MAX_JOB_TYPES; i++) {
        worker->job_types_supported[i] = job_types_supported[i];
    }
    
    worker->job_count = 0;
    worker->failed_job_count = 0;
    worker->total_processing_time = 0;
    
    /* Update statistics */
    g_total_workers++;
    g_idle_workers++;
    
    int worker_id = worker->worker_id;
    
    pthread_mutex_unlock(&g_worker_mutex);
    
    log_info("Worker %d registered: %s:%d, supports %d job types", 
             worker_id, ip_address, port, num_job_types);
    
    return worker_id;
}

int worker_manager_unregister(int worker_id) {
    pthread_mutex_lock(&g_worker_mutex);
    
    /* Find worker */
    Worker *worker = find_worker_by_id(worker_id);
    if (!worker) {
        pthread_mutex_unlock(&g_worker_mutex);
        log_error("Worker %d not found", worker_id);
        return -1;
    }
    
    /* Update statistics */
    g_total_workers--;
    if (worker->status == WORKER_STATUS_IDLE) {
        g_idle_workers--;
    } else if (worker->status == WORKER_STATUS_BUSY) {
        g_busy_workers--;
    }
    
    /* Clear worker slot */
    memset(worker, 0, sizeof(Worker));
    
    pthread_mutex_unlock(&g_worker_mutex);
    
    log_info("Worker %d unregistered", worker_id);
    
    return 0;
}

int worker_manager_find_by_socket(int socket) {
    pthread_mutex_lock(&g_worker_mutex);
    
    for (int i = 0; i < g_max_workers; i++) {
        if (g_workers[i].worker_id > 0 && g_workers[i].socket == socket) {
            int worker_id = g_workers[i].worker_id;
            pthread_mutex_unlock(&g_worker_mutex);
            return worker_id;
        }
    }
    
    pthread_mutex_unlock(&g_worker_mutex);
    
    return -1;
}

int worker_manager_update_status(int worker_id, int status) {
    pthread_mutex_lock(&g_worker_mutex);
    
    /* Find worker */
    Worker *worker = find_worker_by_id(worker_id);
    if (!worker) {
        pthread_mutex_unlock(&g_worker_mutex);
        log_error("Worker %d not found", worker_id);
        return -1;
    }
    
    /* Update statistics based on status change */
    if (worker->status != status) {
        if (worker->status == WORKER_STATUS_IDLE) {
            g_idle_workers--;
        } else if (worker->status == WORKER_STATUS_BUSY) {
            g_busy_workers--;
        }
        
        if (status == WORKER_STATUS_IDLE) {
            g_idle_workers++;
        } else if (status == WORKER_STATUS_BUSY) {
            g_busy_workers++;
        }
    }
    
    /* Update worker status */
    worker->status = status;
    
    pthread_mutex_unlock(&g_worker_mutex);
    
    log_debug("Worker %d status updated to %d", worker_id, status);
    
    return 0;
}

int worker_manager_update_heartbeat(int worker_id) {
    pthread_mutex_lock(&g_worker_mutex);
    
    /* Find worker */
    Worker *worker = find_worker_by_id(worker_id);
    if (!worker) {
        pthread_mutex_unlock(&g_worker_mutex);
        log_error("Worker %d not found", worker_id);
        return -1;
    }
    
    /* Update heartbeat timestamp */
    worker->last_heartbeat = get_current_time_ms();
    
    pthread_mutex_unlock(&g_worker_mutex);
    
    return 0;
}

int worker_manager_assign_job(int worker_id, int job_id) {
    pthread_mutex_lock(&g_worker_mutex);
    
    /* Find worker */
    Worker *worker = find_worker_by_id(worker_id);
    if (!worker) {
        pthread_mutex_unlock(&g_worker_mutex);
        log_error("Worker %d not found", worker_id);
        return -1;
    }
    
    /* Update worker */
    worker->status = WORKER_STATUS_BUSY;
    worker->current_job_id = job_id;
    
    /* Update statistics */
    g_idle_workers--;
    g_busy_workers++;
    
    pthread_mutex_unlock(&g_worker_mutex);
    
    log_debug("Worker %d assigned job %d", worker_id, job_id);
    
    return 0;
}

int worker_manager_complete_job(int worker_id, long long processing_time_ms) {
    pthread_mutex_lock(&g_worker_mutex);
    
    /* Find worker */
    Worker *worker = find_worker_by_id(worker_id);
    if (!worker) {
        pthread_mutex_unlock(&g_worker_mutex);
        log_error("Worker %d not found", worker_id);
        return -1;
    }
    
    /* Update worker */
    worker->status = WORKER_STATUS_IDLE;
    worker->current_job_id = -1;
    worker->job_count++;
    worker->total_processing_time += processing_time_ms;
    
    /* Update statistics */
    g_busy_workers--;
    g_idle_workers++;
    
    pthread_mutex_unlock(&g_worker_mutex);
    
    log_debug("Worker %d completed job, processing_time=%lld ms", worker_id, processing_time_ms);
    
    return 0;
}

int worker_manager_fail_job(int worker_id) {
    pthread_mutex_lock(&g_worker_mutex);
    
    /* Find worker */
    Worker *worker = find_worker_by_id(worker_id);
    if (!worker) {
        pthread_mutex_unlock(&g_worker_mutex);
        log_error("Worker %d not found", worker_id);
        return -1;
    }
    
    /* Update worker */
    worker->status = WORKER_STATUS_IDLE;
    worker->current_job_id = -1;
    worker->failed_job_count++;
    
    /* Update statistics */
    g_busy_workers--;
    g_idle_workers++;
    
    pthread_mutex_unlock(&g_worker_mutex);
    
    log_debug("Worker %d failed job", worker_id);
    
    return 0;
}

int worker_manager_find_available(int job_type) {
    pthread_mutex_lock(&g_worker_mutex);
    
    int best_worker_id = -1;
    int min_job_count = -1;
    
    for (int i = 0; i < g_max_workers; i++) {
        Worker *worker = &g_workers[i];
        
        /* Skip inactive or busy workers */
        if (worker->worker_id == 0 || worker->status != WORKER_STATUS_IDLE) {
            continue;
        }
        
        /* Check if worker supports this job type */
        int supports_job_type = 0;
        for (int j = 0; j < MAX_JOB_TYPES; j++) {
            if (worker->job_types_supported[j] == job_type) {
                supports_job_type = 1;
                break;
            }
        }
        
        if (!supports_job_type) {
            continue;
        }
        
        /* Find worker with least jobs processed (simple load balancing) */
        if (best_worker_id < 0 || worker->job_count < min_job_count) {
            best_worker_id = worker->worker_id;
            min_job_count = worker->job_count;
        }
    }
    
    pthread_mutex_unlock(&g_worker_mutex);
    
    return best_worker_id;
}

int worker_manager_check_inactive(int timeout_seconds) {
    long long current_time = get_current_time_ms();
    long long timeout_ms = timeout_seconds * 1000;
    int inactive_count = 0;
    
    pthread_mutex_lock(&g_worker_mutex);
    
    for (int i = 0; i < g_max_workers; i++) {
        Worker *worker = &g_workers[i];
        
        /* Skip inactive workers */
        if (worker->worker_id == 0) {
            continue;
        }
        
        /* Check if worker has timed out */
        long long elapsed_ms = current_time - worker->last_heartbeat;
        if (elapsed_ms > timeout_ms) {
            log_warning("Worker %d inactive for %lld ms, marking as offline", 
                       worker->worker_id, elapsed_ms);
            
            /* Update statistics */
            if (worker->status == WORKER_STATUS_IDLE) {
                g_idle_workers--;
            } else if (worker->status == WORKER_STATUS_BUSY) {
                g_busy_workers--;
            }
            
            /* Mark worker as offline */
            worker->status = WORKER_STATUS_OFFLINE;
            
            inactive_count++;
        }
    }
    
    pthread_mutex_unlock(&g_worker_mutex);
    
    return inactive_count;
}

int worker_manager_get_stats(int *total_workers, int *idle_workers, int *busy_workers) {
    pthread_mutex_lock(&g_worker_mutex);
    
    if (total_workers) *total_workers = g_total_workers;
    if (idle_workers) *idle_workers = g_idle_workers;
    if (busy_workers) *busy_workers = g_busy_workers;
    
    pthread_mutex_unlock(&g_worker_mutex);
    
    return 0;
}