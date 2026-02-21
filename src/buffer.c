/**
 * @file buffer.c
 * @brief Ring buffer implementation for sensor data logging
 *
 * Fixes applied from code review:
 *  1. advance_pointer() and pointer_distance() are now used (no dead code /
 *     no -Wunused-function build error under -Werror).
 *  2. `overflows` 1-bit flag renamed to `overflow_occurred`; a real
 *     uint32_t overflow_count field tracks rejected write count.
 *  3. buffer_create(0) rejected — malloc(0) is implementation-defined.
 *  4. %ld -> %td for ptrdiff_t in buffer_print_debug.
 *  5. sensor_id printed with PRIu8 to match its uint8_t type.
 *  6. buffer_free() renamed buffer_free_slots() — avoids shadowing free().
 *  7. Debug printfs removed from hot paths (write/read).
 */

#include "buffer.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * PRIVATE HELPERS
 * ========================================================================== */

static sensor_reading_t *advance_pointer(sensor_reading_t *ptr,
                                         const ring_buffer_t *buf)
{
    ptr++;
    if (ptr == buf->buffer + buf->capacity)
        ptr = buf->buffer;
    return ptr;
}

static size_t pointer_distance(const sensor_reading_t *start,
                                const sensor_reading_t *end,
                                const ring_buffer_t    *buf)
{
    if (end >= start)
        return (size_t)(end - start);

    return (buf->capacity - (size_t)(start - buf->buffer))
         + (size_t)(end - buf->buffer);
}

/* ============================================================================
 * PUBLIC API
 * ========================================================================== */

ring_buffer_t *buffer_create(size_t capacity)
{
    if (capacity == 0)
        return NULL;

    ring_buffer_t *buf = malloc(sizeof(ring_buffer_t));
    if (buf == NULL)
        return NULL;

    buf->buffer = malloc(capacity * sizeof(sensor_reading_t));
    if (buf->buffer == NULL) {
        free(buf);
        return NULL;
    }

    buf->head           = buf->buffer;
    buf->tail           = buf->buffer;
    buf->capacity       = capacity;
    buf->count          = 0;
    buf->overflow_count = 0;

    buf->status.is_full           = 0;
    buf->status.is_empty          = 1;
    buf->status.overflow_occurred = 0;
    buf->status.reserved          = 0;

    memset(buf->buffer, 0, capacity * sizeof(sensor_reading_t));

    printf("[DEBUG] buffer_create(%zu) — OK\n", capacity);
    return buf;
}

void buffer_destroy(ring_buffer_t *buf)
{
    if (buf == NULL)
        return;

    printf("[DEBUG] buffer_destroy() called\n");

    free(buf->buffer);
    buf->buffer = NULL;
    buf->head   = NULL;
    buf->tail   = NULL;
    free(buf);
}

bool buffer_write(ring_buffer_t *buf, const sensor_reading_t *reading)
{
    if (buf == NULL || reading == NULL)
        return false;

    if (buf->count == buf->capacity) {
        buf->overflow_count++;
        buf->status.is_full           = 1;
        buf->status.overflow_occurred = 1;
        return false;
    }

    *(buf->head) = *reading;
    buf->head    = advance_pointer(buf->head, buf);
    buf->count++;

    buf->status.is_empty = 0;
    buf->status.is_full  = (buf->count == buf->capacity);

    return true;
}

bool buffer_read(ring_buffer_t *buf, sensor_reading_t *output)
{
    if (buf == NULL || output == NULL)
        return false;

    if (buf->count == 0) {
        buf->status.is_empty = 1;
        return false;
    }

    *output   = *(buf->tail);
    buf->tail = advance_pointer(buf->tail, buf);
    buf->count--;

    buf->status.is_full  = 0;
    buf->status.is_empty = (buf->count == 0);

    return true;
}

bool buffer_peek(const ring_buffer_t *buf, sensor_reading_t *output)
{
    if (buffer_is_empty(buf) || output == NULL)
        return false;

    *output = *(buf->tail);
    return true;
}

bool buffer_is_empty(const ring_buffer_t *buf)
{
    return (buf == NULL) || (buf->count == 0);
}

bool buffer_is_full(const ring_buffer_t *buf)
{
    return (buf != NULL) && (buf->count == buf->capacity);
}

size_t buffer_count(const ring_buffer_t *buf)
{
    return (buf == NULL) ? 0 : buf->count;
}

size_t buffer_free_slots(const ring_buffer_t *buf)
{
    return (buf == NULL) ? 0 : (buf->capacity - buf->count);
}

void buffer_clear(ring_buffer_t *buf)
{
    if (buf == NULL)
        return;

    buf->head  = buf->buffer;
    buf->tail  = buf->buffer;
    buf->count = 0;
    /* overflow_count preserved across clear — reset manually if needed */
    buf->status.is_full           = 0;
    buf->status.is_empty          = 1;
    buf->status.overflow_occurred = (buf->overflow_count > 0);
}

buffer_status_t buffer_get_status(const ring_buffer_t *buf)
{
    buffer_status_t empty = {0};
    return (buf != NULL) ? buf->status : empty;
}

uint32_t buffer_overflow_count(const ring_buffer_t *buf)
{
    return (buf == NULL) ? 0 : buf->overflow_count;
}

void buffer_print_debug(const ring_buffer_t *buf)
{
    if (buf == NULL) {
        printf("Buffer: NULL\n");
        return;
    }

    printf("=== Buffer Debug Info ===\n");
    printf("Base:     %p\n",               (void *)buf->buffer);
    printf("Head:     %p (offset: %td)\n", (void *)buf->head, buf->head - buf->buffer);
    printf("Tail:     %p (offset: %td)\n", (void *)buf->tail, buf->tail - buf->buffer);
    printf("Capacity: %zu entries\n",      buf->capacity);
    printf("Count:    %zu entries\n",      buf->count);
    printf("Free:     %zu entries\n",      buf->capacity - buf->count);
    printf("Overflows:%"PRIu32"\n",        buf->overflow_count);
    printf("Status:   %s | %s | %s\n",
           buf->status.is_full           ? "FULL"       : "NOT_FULL",
           buf->status.is_empty          ? "EMPTY"      : "NOT_EMPTY",
           buf->status.overflow_occurred ? "OVERFLOWED" : "NO_OVERFLOW");

    /* Use pointer_distance to sanity-check internal state */
    size_t computed = pointer_distance(buf->tail, buf->head, buf);
    if (computed != buf->count)
        printf("WARNING: count(%zu) != pointer_distance(%zu) — state corrupt!\n",
               buf->count, computed);

    printf("First %d entries (if any):\n", buf->count < 3 ? (int)buf->count : 3);
    for (size_t i = 0; i < 3 && i < buf->count; i++) {
        size_t idx = ((size_t)(buf->tail - buf->buffer) + i) % buf->capacity;
        printf("  [%zu] time=%"PRIu32"  sensor=%"PRIu8"  value=%.2f\n",
               idx,
               buf->buffer[idx].timestamp,
               buf->buffer[idx].sensor_id,
               buf->buffer[idx].value);
    }
    printf("=========================\n");
}

