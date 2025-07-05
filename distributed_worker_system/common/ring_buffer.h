/**
 * ring_buffer.h - Thread-safe Ring Buffer
 * 
 * This file provides a thread-safe ring buffer implementation for
 * producer-consumer patterns.
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <pthread.h>

/* Ring buffer structure */
typedef struct {
    void **buffer;          /* Array of pointers to elements */
    uint32_t capacity;      /* Maximum number of elements */
    uint32_t size;          /* Current number of elements */
    uint32_t head;          /* Index of the head (read position) */
    uint32_t tail;          /* Index of the tail (write position) */
    pthread_mutex_t mutex;  /* Mutex for thread safety */
    pthread_cond_t not_empty; /* Condition variable for not empty */
    pthread_cond_t not_full;  /* Condition variable for not full */
} ring_buffer_t;

/**
 * Create a new ring buffer
 * 
 * @param capacity Maximum number of elements
 * @return Pointer to the new ring buffer or NULL on error
 */
ring_buffer_t *ring_buffer_create(uint32_t capacity);

/**
 * Destroy a ring buffer and free its resources
 * 
 * @param rb Pointer to the ring buffer
 * @param free_elements Flag indicating whether to free elements (1) or not (0)
 */
void ring_buffer_destroy(ring_buffer_t *rb, int free_elements);

/**
 * Push an element to the ring buffer
 * 
 * @param rb Pointer to the ring buffer
 * @param element Element to push
 * @param timeout_ms Timeout in milliseconds, 0 for non-blocking, -1 for infinite
 * @return 0 on success, -1 on error or timeout
 */
int ring_buffer_push(ring_buffer_t *rb, void *element, int timeout_ms);

/**
 * Pop an element from the ring buffer
 * 
 * @param rb Pointer to the ring buffer
 * @param timeout_ms Timeout in milliseconds, 0 for non-blocking, -1 for infinite
 * @return Pointer to the element or NULL on error or timeout
 */
void *ring_buffer_pop(ring_buffer_t *rb, int timeout_ms);

/**
 * Get the current size of the ring buffer
 * 
 * @param rb Pointer to the ring buffer
 * @return Current number of elements
 */
uint32_t ring_buffer_size(ring_buffer_t *rb);

/**
 * Get the capacity of the ring buffer
 * 
 * @param rb Pointer to the ring buffer
 * @return Maximum number of elements
 */
uint32_t ring_buffer_capacity(ring_buffer_t *rb);

/**
 * Check if the ring buffer is empty
 * 
 * @param rb Pointer to the ring buffer
 * @return 1 if empty, 0 if not empty
 */
int ring_buffer_is_empty(ring_buffer_t *rb);

/**
 * Check if the ring buffer is full
 * 
 * @param rb Pointer to the ring buffer
 * @return 1 if full, 0 if not full
 */
int ring_buffer_is_full(ring_buffer_t *rb);

#endif /* RING_BUFFER_H */