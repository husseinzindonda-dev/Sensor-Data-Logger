/**
 * @file sensors.h
 * @brief Sensor abstraction layer
 *
 * Sits on top of the ring buffer. Each logical sensor has an ID, a human
 * readable name, and its own dedicated ring buffer. The layer handles
 * timestamping, min/max tracking, and formatted reporting.
 */

#ifndef SENSORS_H
#define SENSORS_H

#include "buffer.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * CONFIGURATION
 * ========================================================================== */

/** @brief Maximum length of a sensor name string (including null terminator) */
#define SENSOR_NAME_MAX  32

/** @brief Default per-sensor ring buffer capacity */
#define SENSOR_BUFFER_CAPACITY  64

/* ============================================================================
 * DATA TYPES
 * ========================================================================== */

/**
 * @brief Operational state of a sensor
 */
typedef enum {
    SENSOR_STATE_UNINIT = 0,    ///< Not yet initialised
    SENSOR_STATE_ACTIVE,        ///< Logging readings normally
    SENSOR_STATE_PAUSED,        ///< Temporarily stopped (buffer kept)
    SENSOR_STATE_ERROR          ///< Fault condition
} sensor_state_t;

/**
 * @brief Running statistics for a sensor
 */
typedef struct {
    float    min;           ///< Minimum value seen
    float    max;           ///< Maximum value seen
    float    sum;           ///< Sum of all readings (for mean calculation)
    uint32_t sample_count;  ///< Total readings logged
} sensor_stats_t;

/**
 * @brief A logical sensor with its own ring buffer and metadata
 */
typedef struct {
    uint8_t         id;                     ///< Unique sensor index
    char            name[SENSOR_NAME_MAX];  ///< Human-readable label
    sensor_state_t  state;                  ///< Current operational state
    ring_buffer_t  *buf;                    ///< Dedicated ring buffer
    sensor_stats_t  stats;                  ///< Running statistics
} sensor_t;

/* ============================================================================
 * PUBLIC API
 * ========================================================================== */

bool sensor_init(sensor_t *sensor, uint8_t id, const char *name, size_t capacity);
void sensor_destroy(sensor_t *sensor);
bool sensor_log(sensor_t *sensor, float value, uint32_t timestamp);
bool sensor_read(sensor_t *sensor, sensor_reading_t *output);
bool sensor_peek(const sensor_t *sensor, sensor_reading_t *output);
bool sensor_get_stats(const sensor_t *sensor, sensor_stats_t *stats);
bool sensor_get_mean(const sensor_t *sensor, float *mean);
bool sensor_pause(sensor_t *sensor);
bool sensor_resume(sensor_t *sensor);
void sensor_flush(sensor_t *sensor);
void sensor_print_info(const sensor_t *sensor);

#endif /* SENSORS_H */
