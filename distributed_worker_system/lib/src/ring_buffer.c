/**
 * ring_buffer.c - Thread-safe Ring Buffer Implementation
 */

#include "ring_buffer.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

/* Create a new ring buffer */
ring_buffer_t *ring_buffer_create(uint32_t capacity) {
    ring_buffer_t *rb;
    
    if (capacity == 0) {
        return NULL;
    }
    
    /* Allocate ring buffer structure */
    rb = (ring_buffer_t *)malloc(sizeof(ring_buffer_t));
    if (rb == NULL) {
        return NULL;
    }
    
    /* Allocate buffer array */
    rb->buffer = (void **)malloc(capacity * sizeof(void *));
    if (rb->buffer == NULL) {
        free(rb);
        return NULL;
    }
    
    /* Initialize ring buffer */
    rb->capacity = capacity;
    rb->size = 0;
    rb->head = 0;
    rb->tail = 0;
    
    /* Initialize synchronization primitives */
    if (pthread_mutex_init(&rb->mutex, NULL) != 0) {
        free(rb->buffer);
        free(rb);
        return NULL;
    }
    
    if (pthread_cond_init(&rb->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&rb->mutex);
        free(rb->buffer);
        free(rb);
        return NULL;
    }
    
    if (pthread_cond_init(&rb->not_full, NULL) != 0) {
        pthread_cond_destroy(&rb->not_empty);
        pthread_mutex_destroy(&rb->mutex);
        free(rb->buffer);
        free(rb);
        return NULL;
    }
    
    return rb;
}

/* Destroy a ring buffer and free its resources */
void ring_buffer_destroy(ring_buffer_t *rb, int free_elements) {
    uint32_t i;
    
    if (rb == NULL) {
        return;
    }
    
    pthread_mutex_lock(&rb->mutex);
    
    /* Free elements if requested */
    if (free_elements && rb->size > 0) {
        for (i = 0; i < rb->size; i++) {
            uint32_t idx = (rb->head + i) % rb->capacity;
            free(rb->buffer[idx]);
            rb->buffer[idx] = NULL;
        }
    }
    
    /* Free buffer array */
    free(rb->buffer);
    rb->buffer = NULL;
    
    pthread_mutex_unlock(&rb->mutex);
    
    /* Destroy synchronization primitives */
    pthread_cond_destroy(&rb->not_empty);
    pthread_cond_destroy(&rb->not_full);
    pthread_mutex_destroy(&rb->mutex);
    
    /* Free ring buffer structure */
    free(rb);
}

/* Helper function to get current time plus timeout */
static int get_timeout_time(struct timespec *ts, int timeout_ms) {
    struct timeval tv;
    
    if (gettimeofday(&tv, NULL) != 0) {
        return -1;
    }
    
    ts->tv_sec = tv.tv_sec + (timeout_ms / 1000);
    ts->tv_nsec = (tv.tv_usec + (timeout_ms % 1000) * 1000) * 1000;
    
    /* Handle nanosecond overflow */
    if (ts->tv_nsec >= 1000000000) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000;
    }
    
    return 0;
}

/* Push an element to the ring buffer */
int ring_buffer_push(ring_buffer_t *rb, void *element, int timeout_ms) {
    struct timespec ts;
    int result = 0;
    
    if (rb == NULL || element == NULL) {
        return -1;
    }
    
    /* Lock the mutex */
    pthread_mutex_lock(&rb->mutex);
    
    /* Wait until there is space or timeout */
    if (rb->size == rb->capacity) {
        if (timeout_ms == 0) {
            /* Non-blocking mode */
            pthread_mutex_unlock(&rb->mutex);
            return -1;
        } else if (timeout_ms > 0) {
            /* Timed wait */
            if (get_timeout_time(&ts, timeout_ms) != 0) {
                pthread_mutex_unlock(&rb->mutex);
                return -1;
            }
            
            while (rb->size == rb->capacity && result == 0) {
                result = pthread_cond_timedwait(&rb->not_full, &rb->mutex, &ts);
            }
            
            if (result != 0) {
                pthread_mutex_unlock(&rb->mutex);
                return -1;
            }
        } else {
            /* Infinite wait */
            while (rb->size == rb->capacity) {
                pthread_cond_wait(&rb->not_full, &rb->mutex);
            }
        }
    }
    
    /* Add element to the buffer */
    rb->buffer[rb->tail] = element;
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->size++;
    
    /* Signal that the buffer is not empty */
    pthread_cond_signal(&rb->not_empty);
    
    /* Unlock the mutex */
    pthread_mutex_unlock(&rb->mutex);
    
    return 0;
}

/* Pop an element from the ring buffer */
void *ring_buffer_pop(ring_buffer_t *rb, int timeout_ms) {
    struct timespec ts;
    void *element;
    int result = 0;
    
    if (rb == NULL) {
        return NULL;
    }
    
    /* Lock the mutex */
    pthread_mutex_lock(&rb->mutex);
    
    /* Wait until there is an element or timeout */
    if (rb->size == 0) {
        if (timeout_ms == 0) {
            /* Non-blocking mode */
            pthread_mutex_unlock(&rb->mutex);
            return NULL;
        } else if (timeout_ms > 0) {
            /* Timed wait */
            if (get_timeout_time(&ts, timeout_ms) != 0) {
                pthread_mutex_unlock(&rb->mutex);
                return NULL;
            }
            
            while (rb->size == 0 && result == 0) {
                result = pthread_cond_timedwait(&rb->not_empty, &rb->mutex, &ts);
            }
            
            if (result != 0) {
                pthread_mutex_unlock(&rb->mutex);
                return NULL;
            }
        } else {
            /* Infinite wait */
            while (rb->size == 0) {
                pthread_cond_wait(&rb->not_empty, &rb->mutex);
            }
        }
    }
    
    /* Get element from the buffer */
    element = rb->buffer[rb->head];
    rb->head = (rb->head + 1) % rb->capacity;
    rb->size--;
    
    /* Signal that the buffer is not full */
    pthread_cond_signal(&rb->not_full);
    
    /* Unlock the mutex */
    pthread_mutex_unlock(&rb->mutex);
    
    return element;
}

/* Get the current size of the ring buffer */
uint32_t ring_buffer_size(ring_buffer_t *rb) {
    uint32_t size;
    
    if (rb == NULL) {
        return 0;
    }
    
    pthread_mutex_lock(&rb->mutex);
    size = rb->size;
    pthread_mutex_unlock(&rb->mutex);
    
    return size;
}

/* Get the capacity of the ring buffer */
uint32_t ring_buffer_capacity(ring_buffer_t *rb) {
    uint32_t capacity;
    
    if (rb == NULL) {
        return 0;
    }
    
    pthread_mutex_lock(&rb->mutex);
    capacity = rb->capacity;
    pthread_mutex_unlock(&rb->mutex);
    
    return capacity;
}

/* Check if the ring buffer is empty */
int ring_buffer_is_empty(ring_buffer_t *rb) {
    int is_empty;
    
    if (rb == NULL) {
        return 1;
    }
    
    pthread_mutex_lock(&rb->mutex);
    is_empty = (rb->size == 0);
    pthread_mutex_unlock(&rb->mutex);
    
    return is_empty;
}

/* Check if the ring buffer is full */
int ring_buffer_is_full(ring_buffer_t *rb) {
    int is_full;
    
    if (rb == NULL) {
        return 0;
    }
    
    pthread_mutex_lock(&rb->mutex);
    is_full = (rb->size == rb->capacity);
    pthread_mutex_unlock(&rb->mutex);
    
    return is_full;
}