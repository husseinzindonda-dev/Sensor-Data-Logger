/**
 * @file buffer.h
 * @brief Ring buffer implementation for sensor data logging
 *
 * Fixed-size circular buffer for storing sensor readings with timestamps.
 * Designed for manual memory management using pointer arithmetic.
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>     /* fixed-width types    */
#include <stdbool.h>    /* bool type            */
#include <stddef.h>     /* size_t               */

/* ============================================================================
 * CONFIGURATION
 * ========================================================================== */

/** @brief Maximum number of sensors the system supports */
#define MAX_SENSORS         8

/** @brief Default buffer capacity (power of 2 recommended) */
#define DEFAULT_BUFFER_SIZE 256

/* ============================================================================
 * DATA TYPES
 * ========================================================================== */

/**
 * @brief A single sensor reading
 */
typedef struct {
    uint32_t timestamp;     ///< Unix timestamp or milliseconds tick
    uint8_t  sensor_id;     ///< Sensor index (0 .. MAX_SENSORS-1)
    float    value;         ///< Measured value
} sensor_reading_t;

/**
 * @brief Buffer status flags
 *
 * NOTE: `overflow_occurred` is a boolean flag (was a misnamed 1-bit counter).
 *       Use buffer_overflow_count() for the real overflow counter.
 */
typedef struct {
    unsigned int is_full          : 1;  ///< Buffer is completely full
    unsigned int is_empty         : 1;  ///< Buffer is completely empty
    unsigned int overflow_occurred: 1;  ///< At least one write was rejected
    unsigned int reserved         : 5;  ///< Padding — reserved for future use
} buffer_status_t;

/**
 * @brief Ring buffer control structure
 */
typedef struct {
    sensor_reading_t *buffer;       ///< Base pointer to data storage
    sensor_reading_t *head;         ///< Write position (next free slot)
    sensor_reading_t *tail;         ///< Read position  (oldest entry)
    size_t            capacity;     ///< Maximum number of entries
    size_t            count;        ///< Current number of entries
    uint32_t          overflow_count; ///< Total rejected writes (real counter)
    buffer_status_t   status;       ///< Convenience status flags
} ring_buffer_t;

/* ============================================================================
 * PUBLIC API
 * ========================================================================== */

ring_buffer_t* buffer_create(size_t capacity);
void           buffer_destroy(ring_buffer_t *buf);
bool           buffer_write(ring_buffer_t *buf, const sensor_reading_t *reading);
bool           buffer_read(ring_buffer_t *buf, sensor_reading_t *output);
bool           buffer_peek(const ring_buffer_t *buf, sensor_reading_t *output);
bool           buffer_is_empty(const ring_buffer_t *buf);
bool           buffer_is_full(const ring_buffer_t *buf);
size_t         buffer_count(const ring_buffer_t *buf);
size_t         buffer_free_slots(const ring_buffer_t *buf);
void           buffer_clear(ring_buffer_t *buf);
buffer_status_t buffer_get_status(const ring_buffer_t *buf);
uint32_t       buffer_overflow_count(const ring_buffer_t *buf);
void           buffer_print_debug(const ring_buffer_t *buf);

#endif /* BUFFER_H */
