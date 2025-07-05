/**
 * @file job_handler.c
 * @brief Job handler implementation for distributed worker system
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#include "job_handler.h"
#include "../common/logger.h"
#include "../common/protocol.h"
#include "../common/common.h"

/* Job array */
static Job *g_jobs = NULL;
static int g_max_jobs = 0;
static int g_job_timeout_seconds = 0;
static int g_next_job_id = 1;

/* Statistics */
static int g_pending_jobs = 0;
static int g_running_jobs = 0;
static int g_completed_jobs = 0;
static int g_failed_jobs = 0;

/* Mutex for thread safety */
static pthread_mutex_t g_job_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Helper functions */
static long long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static Job *find_job_by_id(int job_id) {
    if (job_id <= 0 || job_id >= g_next_job_id) {
        return NULL;
    }
    
    for (int i = 0; i < g_max_jobs; i++) {
        if (g_jobs[i].job_id == job_id) {
            return &g_jobs[i];
        }
    }
    
    return NULL;
}

static int find_free_job_slot() {
    for (int i = 0; i < g_max_jobs; i++) {
        if (g_jobs[i].job_id == 0) {
            return i;
        }
    }
    
    return -1;
}

int job_handler_init(int max_jobs, int job_timeout_seconds) {
    if (max_jobs <= 0) {
        log_error("Invalid max_jobs value: %d", max_jobs);
        return -1;
    }
    
    /* Allocate job array */
    g_jobs = (Job *)calloc(max_jobs, sizeof(Job));
    if (!g_jobs) {
        log_error("Failed to allocate memory for jobs");
        return -1;
    }
    
    g_max_jobs = max_jobs;
    g_job_timeout_seconds = job_timeout_seconds;
    g_next_job_id = 1;
    
    log_info("Job handler initialized with max_jobs=%d, timeout=%d seconds", 
             max_jobs, job_timeout_seconds);
    
    return 0;
}

int job_handler_cleanup() {
    pthread_mutex_lock(&g_job_mutex);
    
    /* Free job data */
    for (int i = 0; i < g_max_jobs; i++) {
        if (g_jobs[i].input_data) {
            free(g_jobs[i].input_data);
        }
        
        if (g_jobs[i].result_data) {
            free(g_jobs[i].result_data);
        }
    }
    
    /* Free job array */
    free(g_jobs);
    g_jobs = NULL;
    g_max_jobs = 0;
    
    pthread_mutex_unlock(&g_job_mutex);
    
    log_info("Job handler cleaned up");
    
    return 0;
}

int job_handler_submit(int client_socket, int job_type, 
                      const char *input_data, size_t input_size) {
    if (!input_data && input_size > 0) {
        log_error("Invalid input data");
        return -1;
    }
    
    pthread_mutex_lock(&g_job_mutex);
    
    /* Find free job slot */
    int slot = find_free_job_slot();
    if (slot < 0) {
        pthread_mutex_unlock(&g_job_mutex);
        log_error("No free job slots available");
        return -1;
    }
    
    /* Allocate memory for input data */
    char *data_copy = NULL;
    if (input_size > 0) {
        data_copy = (char *)malloc(input_size);
        if (!data_copy) {
            pthread_mutex_unlock(&g_job_mutex);
            log_error("Failed to allocate memory for job input data");
            return -1;
        }
        
        memcpy(data_copy, input_data, input_size);
    }
    
    /* Initialize job */
    Job *job = &g_jobs[slot];
    job->job_id = g_next_job_id++;
    job->client_socket = client_socket;
    job->worker_socket = -1;
    job->job_type = job_type;
    job->input_data = data_copy;
    job->input_size = input_size;
    job->result_data = NULL;
    job->result_size = 0;
    job->status = JOB_STATUS_PENDING;
    job->submit_time = get_current_time_ms();
    job->start_time = 0;
    job->end_time = 0;
    job->retry_count = 0;
    
    /* Update statistics */
    g_pending_jobs++;
    
    int job_id = job->job_id;
    
    pthread_mutex_unlock(&g_job_mutex);
    
    log_info("Job %d submitted: type=%d, input_size=%zu", 
             job_id, job_type, input_size);
    
    return job_id;
}

int job_handler_assign(int worker_socket) {
    pthread_mutex_lock(&g_job_mutex);
    
    /* Find oldest pending job */
    Job *oldest_job = NULL;
    long long oldest_time = 0;
    int oldest_index = -1;
    
    for (int i = 0; i < g_max_jobs; i++) {
        if (g_jobs[i].job_id > 0 && g_jobs[i].status == JOB_STATUS_PENDING) {
            if (oldest_job == NULL || g_jobs[i].submit_time < oldest_time) {
                oldest_job = &g_jobs[i];
                oldest_time = g_jobs[i].submit_time;
                oldest_index = i;
            }
        }
    }
    
    if (!oldest_job) {
        pthread_mutex_unlock(&g_job_mutex);
        return -1; /* No pending jobs */
    }
    
    /* Assign job to worker */
    oldest_job->worker_socket = worker_socket;
    oldest_job->status = JOB_STATUS_ASSIGNED;
    oldest_job->start_time = get_current_time_ms();
    
    /* Update statistics */
    g_pending_jobs--;
    g_running_jobs++;
    
    /* Prepare job message */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.header.message_type = MSG_TYPE_JOB_ASSIGNED;
    msg.body.job_assigned.job_id = oldest_job->job_id;
    msg.body.job_assigned.job_type = oldest_job->job_type;
    msg.body.job_assigned.data = oldest_job->input_data;
    msg.body.job_assigned.data_size = oldest_job->input_size;
    
    int job_id = oldest_job->job_id;
    
    pthread_mutex_unlock(&g_job_mutex);
    
    /* Send job to worker */
    if (send_message(worker_socket, &msg) < 0) {
        log_error("Failed to send job %d to worker", job_id);
        
        /* Mark job as pending again */
        pthread_mutex_lock(&g_job_mutex);
        Job *job = find_job_by_id(job_id);
        if (job) {
            job->worker_socket = -1;
            job->status = JOB_STATUS_PENDING;
            job->start_time = 0;
            
            /* Update statistics */
            g_pending_jobs++;
            g_running_jobs--;
        }
        pthread_mutex_unlock(&g_job_mutex);
        
        return -1;
    }
    
    log_info("Job %d assigned to worker on socket %d", job_id, worker_socket);
    
    return job_id;
}

int job_handler_complete(int job_id, const char *result_data, size_t result_size) {
    if (!result_data && result_size > 0) {
        log_error("Invalid result data");
        return -1;
    }
    
    pthread_mutex_lock(&g_job_mutex);
    
    /* Find job */
    Job *job = find_job_by_id(job_id);
    if (!job) {
        pthread_mutex_unlock(&g_job_mutex);
        log_error("Job %d not found", job_id);
        return -1;
    }
    
    /* Check job status */
    if (job->status != JOB_STATUS_ASSIGNED) {
        pthread_mutex_unlock(&g_job_mutex);
        log_error("Job %d is not in assigned state", job_id);
        return -1;
    }
    
    /* Allocate memory for result data */
    char *data_copy = NULL;
    if (result_size > 0) {
        data_copy = (char *)malloc(result_size);
        if (!data_copy) {
            pthread_mutex_unlock(&g_job_mutex);
            log_error("Failed to allocate memory for job result data");
            return -1;
        }
        
        memcpy(data_copy, result_data, result_size);
    }
    
    /* Free previous result data if any */
    if (job->result_data) {
        free(job->result_data);
    }
    
    /* Update job */
    job->result_data = data_copy;
    job->result_size = result_size;
    job->status = JOB_STATUS_COMPLETED;
    job->end_time = get_current_time_ms();
    
    /* Update statistics */
    g_running_jobs--;
    g_completed_jobs++;
    
    /* Prepare result message */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.header.message_type = MSG_TYPE_JOB_COMPLETED;
    msg.body.job_completed.job_id = job_id;
    
    int client_socket = job->client_socket;
    
    pthread_mutex_unlock(&g_job_mutex);
    
    /* Notify client that job is completed */
    if (send_message(client_socket, &msg) < 0) {
        log_error("Failed to notify client about job %d completion", job_id);
        /* Continue anyway, client can poll for job status */
    }
    
    log_info("Job %d completed: result_size=%zu, processing_time=%lld ms", 
             job_id, result_size, job->end_time - job->start_time);
    
    return 0;
}

int job_handler_fail(int job_id, int error_code) {
    pthread_mutex_lock(&g_job_mutex);
    
    /* Find job */
    Job *job = find_job_by_id(job_id);
    if (!job) {
        pthread_mutex_unlock(&g_job_mutex);
        log_error("Job %d not found", job_id);
        return -1;
    }
    
    /* Check job status */
    if (job->status != JOB_STATUS_ASSIGNED) {
        pthread_mutex_unlock(&g_job_mutex);
        log_error("Job %d is not in assigned state", job_id);
        return -1;
    }
    
    /* Update job */
    job->status = JOB_STATUS_FAILED;
    job->end_time = get_current_time_ms();
    job->retry_count++;
    
    /* Check if we should retry */
    if (job->retry_count < MAX_JOB_RETRIES) {
        log_info("Job %d failed, retrying (attempt %d/%d)", 
                job_id, job->retry_count, MAX_JOB_RETRIES);
        
        /* Reset job for retry */
        job->status = JOB_STATUS_PENDING;
        job->worker_socket = -1;
        job->start_time = 0;
        job->end_time = 0;
        
        /* Update statistics */
        g_running_jobs--;
        g_pending_jobs++;
        
        pthread_mutex_unlock(&g_job_mutex);
        return 0;
    }
    
    /* Update statistics */
    g_running_jobs--;
    g_failed_jobs++;
    
    /* Prepare failure message */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.header.message_type = MSG_TYPE_JOB_FAILED;
    msg.body.job_failed.job_id = job_id;
    msg.body.job_failed.error_code = error_code;
    
    int client_socket = job->client_socket;
    
    pthread_mutex_unlock(&g_job_mutex);
    
    /* Notify client that job has failed */
    if (send_message(client_socket, &msg) < 0) {
        log_error("Failed to notify client about job %d failure", job_id);
        /* Continue anyway, client can poll for job status */
    }
    
    log_info("Job %d failed permanently after %d attempts: error_code=%d", 
             job_id, MAX_JOB_RETRIES, error_code);
    
    return 0;
}

int job_handler_get_status(int job_id) {
    pthread_mutex_lock(&g_job_mutex);
    
    /* Find job */
    Job *job = find_job_by_id(job_id);
    if (!job) {
        pthread_mutex_unlock(&g_job_mutex);
        log_error("Job %d not found", job_id);
        return -1;
    }
    
    int status = job->status;
    
    pthread_mutex_unlock(&g_job_mutex);
    
    return status;
}

int job_handler_get_result(int job_id, char **result_data, size_t *result_size) {
    if (!result_data || !result_size) {
        log_error("Invalid result pointers");
        return -1;
    }
    
    pthread_mutex_lock(&g_job_mutex);
    
    /* Find job */
    Job *job = find_job_by_id(job_id);
    if (!job) {
        pthread_mutex_unlock(&g_job_mutex);
        log_error("Job %d not found", job_id);
        return -1;
    }
    
    /* Check job status */
    if (job->status != JOB_STATUS_COMPLETED) {
        pthread_mutex_unlock(&g_job_mutex);
        log_error("Job %d is not completed", job_id);
        return -1;
    }
    
    /* Allocate memory for result data */
    char *data_copy = NULL;
    if (job->result_size > 0) {
        data_copy = (char *)malloc(job->result_size);
        if (!data_copy) {
            pthread_mutex_unlock(&g_job_mutex);
            log_error("Failed to allocate memory for result data copy");
            return -1;
        }
        
        memcpy(data_copy, job->result_data, job->result_size);
    }
    
    *result_data = data_copy;
    *result_size = job->result_size;
    
    pthread_mutex_unlock(&g_job_mutex);
    
    return 0;
}

int job_handler_check_timeouts() {
    long long current_time = get_current_time_ms();
    int timeout_count = 0;
    
    pthread_mutex_lock(&g_job_mutex);
    
    for (int i = 0; i < g_max_jobs; i++) {
        Job *job = &g_jobs[i];
        
        /* Skip inactive or non-running jobs */
        if (job->job_id == 0 || job->status != JOB_STATUS_ASSIGNED) {
            continue;
        }
        
        /* Check if job has timed out */
        long long elapsed_ms = current_time - job->start_time;
        if (elapsed_ms > g_job_timeout_seconds * 1000) {
            log_warning("Job %d timed out after %lld ms", job->job_id, elapsed_ms);
            
            /* Mark job as timed out */
            job->status = JOB_STATUS_TIMEOUT;
            job->end_time = current_time;
            job->retry_count++;
            
            /* Check if we should retry */
            if (job->retry_count < MAX_JOB_RETRIES) {
                log_info("Job %d timed out, retrying (attempt %d/%d)", 
                        job->job_id, job->retry_count, MAX_JOB_RETRIES);
                
                /* Reset job for retry */
                job->status = JOB_STATUS_PENDING;
                job->worker_socket = -1;
                job->start_time = 0;
                job->end_time = 0;
                
                /* Update statistics */
                g_running_jobs--;
                g_pending_jobs++;
            } else {
                /* Update statistics */
                g_running_jobs--;
                g_failed_jobs++;
                
                /* Prepare failure message */
                Message msg;
                memset(&msg, 0, sizeof(Message));
                msg.header.message_type = MSG_TYPE_JOB_FAILED;
                msg.body.job_failed.job_id = job->job_id;
                msg.body.job_failed.error_code = ERR_JOB_TIMEOUT;
                
                int client_socket = job->client_socket;
                int job_id = job->job_id;
                
                /* Notify client that job has failed */
                if (send_message(client_socket, &msg) < 0) {
                    log_error("Failed to notify client about job %d timeout", job_id);
                    /* Continue anyway, client can poll for job status */
                }
                
                log_info("Job %d failed permanently after %d timeout attempts", 
                         job_id, MAX_JOB_RETRIES);
            }
            
            timeout_count++;
        }
    }
    
    pthread_mutex_unlock(&g_job_mutex);
    
    return timeout_count;
}

int job_handler_get_stats(int *pending_jobs, int *running_jobs, 
                         int *completed_jobs, int *failed_jobs) {
    pthread_mutex_lock(&g_job_mutex);
    
    if (pending_jobs) *pending_jobs = g_pending_jobs;
    if (running_jobs) *running_jobs = g_running_jobs;
    if (completed_jobs) *completed_jobs = g_completed_jobs;
    if (failed_jobs) *failed_jobs = g_failed_jobs;
    
    pthread_mutex_unlock(&g_job_mutex);
    
    return 0;
}