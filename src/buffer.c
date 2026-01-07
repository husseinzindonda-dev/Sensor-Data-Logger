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
    if (end >= start)
        distance = (end - start)
    distance = ((buf->capacity - (start - buf->buffer)) + (end - buf->buffer))
    return 0;
}