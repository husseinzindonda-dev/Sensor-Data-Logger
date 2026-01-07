/**
 * @file buffer.c
 * @brief Ring buffer implementation for sensor data logging
 * 
 * Implementation of circular buffer using pointer arithmetic.
 * Memory is managed manually (no malloc hidden inside functions).
 */

#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*=============================================================
* PRIVATE HELPER FUNCTIONS(not exposed in header)
*=============================================================*/

/**
 * @brief Advance pointer within circular buffer bounds
 * 
 * Moves pointer forward by one entry, wrapping around if needed.
 * Does NOT check for full/empty conditions.
 * @param ptr Current pointer
 * @param buf Pointer to buffer structure
 * @return Advanced pointer(wrapped if necessary)
 */
static sensor_reading_t* advance_pointer(sensor_reading_t *ptr, const ring_buffer_t *buf)
{
    ptr++;
    if (ptr ==(buf->buffer + buf->capacity))
        ptr = (buf->buffer)
    return ptr;
}

/**
 * @brief Calculate number of entries between two pointers
 * 
 * Accounts for wrap-around in circular buffer.
 * 
 * @param start Starting pointer (earlier in sequence)
 * @param end Ending pointer (later in sequence)
 * @param buf The buffer context
 * @return Number of entries from start to end
 */
static size_t pointer_distance(const sensor_reading_t *start,
                              const sensor_reading_t *end,
                              const ring_buffer_t *buf)
{
    if (end >= start){
        return (size_t)(end - start)
    }
    return (buf->capacity - (size_t)(start - buf->buffer)) +(size_t)(end - buf->buffer)
        
}
/*PUBLIC FUNCTION IMPLEMENTATION*/

ring_buffer_t* buffer_create(size_t capacity)
{
    printf("[DEBUG] buffer_create(%zu) called\n", capacity)

    // allocating ring buffer control structure
    ring_buffer_t *buf = malloc(sizeof(ring_buffer_t));
    if (buf == NULL){
        return NULL;
    }
    // allocating 
    buf->buffer = malloc(capacity * sizeof(sensor_reading_t));
    if (buf->buffer == NULL){
        free(buf);
        return NULL;
    }
    buf->head = buf->buffer; // pointer initialization
    buf->tail = buf->buffer; // pointer initialization
    buf->capacity = capacity; // metadata initialization
    buf->count = 0; //metadata initialization
    //Initializing status flags
    buf->status.is_full = 0
    buf->status.is_empty = 1;
    buf->status.overflows = 0;
    buf->status.reserved = 0;

    memset(buf->buffer, 0, capacity * sizeof(sensor_reading_t)); // clearing buffer memory
    return buf;

}
void buffer_destroy(ring_buffer_t *buf)
{
    // Check if buf is NULL 
    if (buf == NULL) {
        return;
    }

    printf("[DEBUG] buffer_destroy() called\n");

    // Free the data storage 
    free(buf->buffer);

    // defensive cleanup 
    buf->buffer = NULL;
    buf->head   = NULL;
    buf->tail   = NULL;

    // Free the control structure 
    free(buf);
}
bool buffer_write(ring_buffer_t *buf, const sensor_reading_t *reading){
    if (buf == NULL || reading == NULL){
        return false;
    }
    printf("[DEBUG] buffer_write() called\n");
    // Check if buffer is full
    if (buf->count == buf->capacity){
        buf->status.is_full   = 1;
        buf->status.overflows = 1;
        return false;
    }
    // Copying data into current head position
    *(buf->head) = *reading
    buf->head++;
    if (buf->head >= buf->buffer + buf->capacity) {
        buf->head = buf->buffer;   // wrap-around
    }
    buf->count++; // updating counter
    buf->status.is_empty = 0; // false if 0
    buf->status.is_full  = (buf->count == buf->capacity); //true if count = capacity of ring buffer
}
