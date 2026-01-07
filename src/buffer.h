/**
 * @file buffer.h
 * @brief Ring buffer implementation for sensor data logging
 * 
 * This module provides a fixed-size circular buffer for storing
 * sensor readings with timestamp. Designed for embedded-style
 * memory management using pointers.
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>    //fixed-width types
#include <stdbool.h>   //bool type
#include <stddef.h>    //size_t

/* ============================================================================
 * CONFIGURATION SECTION
 * ========================================================================== */
/**
 * @brief Maximum number of sensors the system supports
 */
#define MAX_SENSORS     8

/**
 * @brief Default buffer size in number of entries
 * 
 * Number of sensor readings should be power of 2 for efficient wrap-around.
 */
#define DEFAULT_BUFFER_SIZE  256

/* ============================================================================
 * DATA TYPE DEFINITIONS
 * ========================================================================== */

/**
 * @brief Sensor reading data structure
    */

typedef struct {
    uint32_t timestamp;     ///< Unix timestamp or milliseconds
    uint8_t  sensor_id;     ///< Which sensor (0 to MAX_SENSORS-1)
    float    value;         ///< Sensor reading value
} sensor_reading_t;

/**
 * @brief Buffer status flags 
 */
typedef struct {
    unsigned int is_full    : 1;  ///< Buffer is completely full
    unsigned int is_empty   : 1;  ///< Buffer is completely empty
    unsigned int overflows  : 1;  ///< Data was lost due to overflow
    unsigned int reserved   : 5;  ///< For future use
} buffer_status_t;

/**
 * @brief Ring buffer control structure
 * 
 * Contains all metadata needed to manage the circular buffer.
 */
typedef struct {
    sensor_reading_t *buffer;     ///< Pointer to start of data storage
    sensor_reading_t *head;       ///< Write position (next free slot)
    sensor_reading_t *tail;       ///< Read position (oldest data)
    size_t capacity;              ///< Maximum number of entries
    size_t count;                 ///< Current number of entries
    buffer_status_t status;       ///< Current buffer status
} ring_buffer_t;

/* ============================================================================
 * PUBLIC FUNCTION DECLARATIONS
 * ========================================================================== */

/**
 * @brief Initialize a ring buffer
 * 
 * @param capacity  Number of entries the buffer should hold
 * @return          Pointer to initialized buffer, NULL on failure
 */
ring_buffer_t* buffer_create(size_t capacity);

/**
 * @brief Destroy a ring buffer and free its memory
 * 
 * Safely deallocates all memory associated with the buffer.
 * 
 * @param buf Pointer to buffer to destroy
 */
void buffer_destroy(ring_buffer_t *buf);

/**
 * @brief Add a sensor reading to the buffer
 * 
 * Copies the reading to the next available slot and updates
 * buffer pointers. Handles wrap-around automatically.
 * 
 * @param buf Pointer to buffer
 * @param reading Sensor reading to add
 * @return true if successful, false if buffer full
 */
bool buffer_write(ring_buffer_t *buf, const sensor_reading_t *reading);

/**
 * @brief Read the oldest sensor reading from buffer
 * 
 * Retrieves and removes the oldest reading. Copies it to
 * the provided location.
 * 
 * @param buf Pointer to buffer
 * @param output Where to store the read data
 * @return true if successful, false if buffer empty
 */
bool buffer_read(ring_buffer_t *buf, sensor_reading_t *output);

/**
 * @brief Peek at the oldest reading without removing it
 * 
 * Allows inspection of the next item to be read without
 * modifying buffer pointers.
 * 
 * @param buf Pointer to buffer
 * @param output Where to store the peeked data
 * @return true if successful, false if buffer empty
 */
bool buffer_peek(const ring_buffer_t *buf, sensor_reading_t *output);

/**
 * @brief Check if buffer is empty
 * 
 * @param buf Pointer to buffer
 * @return true if buffer contains no data
 */
bool buffer_is_empty(const ring_buffer_t *buf);

/**
 * @brief Check if buffer is full
 * 
 * @param buf Pointer to buffer
 * @return true if buffer has no free slots
 */
bool buffer_is_full(const ring_buffer_t *buf);

/**
 * @brief Get number of entries currently in buffer
 * 
 * @param buf Pointer to buffer
 * @return Number of entries stored
 */
size_t buffer_count(const ring_buffer_t *buf);

/**
 * @brief Get number of free slots in buffer
 * 
 * @param buf Pointer to buffer
 * @return Number of available slots
 */
size_t buffer_free(const ring_buffer_t *buf);

/**
 * @brief Remove all entries from buffer
 * 
 * Resets buffer to empty state without deallocating memory.
 * 
 * @param buf Pointer to buffer
 */
void buffer_clear(ring_buffer_t *buf);

/**
 * @brief Get buffer status information
 * 
 * @param buf Pointer to buffer
 * @return Current status flags
 */
buffer_status_t buffer_get_status(const ring_buffer_t *buf);

/**
 * @brief Diagnostic function to print buffer state
 * 
 * For debugging purposes only. Prints pointers, counts, etc.
 * 
 * @param buf Pointer to buffer
 */
void buffer_print_debug(const ring_buffer_t *buf);

#endif /* BUFFER_H */