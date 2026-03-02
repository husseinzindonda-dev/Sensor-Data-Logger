/**
 * @file sensor_manager.h
 * @brief Sensor manager - coordinates multiple sensors from one place
 *
 * Sits on top of the sensor layer. Owns an array of sensor_t instances
 * and provides a single entry point for logging, reading, and reporting
 * across all sensors at once.
 *
 * Typical usage:
 *
 *   manager_t *m = manager_create(3);
 *
 *   manager_register(m, 0, "Temperature",  16);
 *   manager_register(m, 1, "Pressure",     16);
 *   manager_register(m, 2, "Vibration",    16);
 *
 *   manager_log(m, 0, 21.5f,   now());
 *   manager_log(m, 1, 1012.4f, now());
 *   manager_log(m, 2, 0.03f,   now());
 *
 *   manager_print_all(m);
 *   manager_destroy(m);
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "sensors.h" /* sensor_t, sensor_reading_t */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 * CONFIGURATION
 * ========================================================================== */

/**
 * @brief Hard upper limit on sensors per manager.
 *
 * Keeps memory predictable. You can raise this if needed.
 * On a real microcontroller you would lower it to match your hardware.
 */
#define MANAGER_MAX_SENSORS 8

/* ============================================================================
 * ALERT SYSTEM
 * ========================================================================== */

/**
 * @brief Alert severity levels
 *
 * NONE    - everything normal
 * WARNING - value approaching a threshold (early warning)
 * CRITICAL- threshold crossed, action required
 */
typedef enum
{
    ALERT_NONE = 0,
    ALERT_WARNING = 1,
    ALERT_CRITICAL = 2
} alert_level_t;

/**
 * @brief Threshold configuration for one sensor
 *
 * Set warn_high / warn_low to 0 to disable warning thresholds.
 * Set critical_high / critical_low to 0 to disable critical thresholds.
 *
 * Example for a temperature sensor (Celsius):
 *   warn_low     = 0.0f    (below 0 is unusual)
 *   warn_high    = 60.0f   (above 60 is getting hot)
 *   critical_low = -10.0f  (below -10 is a fault)
 *   critical_high= 85.0f   (above 85 is dangerous)
 */
typedef struct
{
    float warn_low;      ///< Warning if value drops below this
    float warn_high;     ///< Warning if value exceeds this
    float critical_low;  ///< Critical if value drops below this
    float critical_high; ///< Critical if value exceeds this
    bool enabled;        ///< false = ignore thresholds for this sensor
} sensor_threshold_t;

/**
 * @brief One alert event — what happened and when
 */
typedef struct
{
    uint8_t sensor_id;   ///< Which sensor triggered this
    alert_level_t level; ///< WARNING or CRITICAL
    float value;         ///< The value that triggered it
    uint32_t timestamp;  ///< When it happened
} alert_event_t;

/* ============================================================================
 * MANAGER STRUCTURE
 * ========================================================================== */

/**
 * @brief The sensor manager control structure
 *
 * You never need to access these fields directly.
 * Use the public API functions below.
 */
typedef struct
{
    sensor_t sensors[MANAGER_MAX_SENSORS];              ///< Sensor array
    sensor_threshold_t thresholds[MANAGER_MAX_SENSORS]; ///< Per-sensor thresholds
    bool registered[MANAGER_MAX_SENSORS];               ///< Slot in use?
    uint8_t count;                                      ///< How many registered
    uint8_t capacity;                                   ///< Max allowed (<=MANAGER_MAX_SENSORS)
    uint32_t total_logs;                                ///< Total readings logged
    uint32_t total_alerts;                              ///< Total alerts triggered
} manager_t;

/* ============================================================================
 * PUBLIC API
 * ========================================================================== */

/**
 * @brief Allocate and initialise a sensor manager.
 *
 * @param capacity  Max number of sensors (1 .. MANAGER_MAX_SENSORS)
 * @return          Pointer to manager, NULL on failure
 */
manager_t *manager_create(uint8_t capacity);

/**
 * @brief Free all memory and destroy the manager.
 * @param m  Manager to destroy (NULL is safe)
 */
void manager_destroy(manager_t *m);

/**
 * @brief Register a new sensor with the manager.
 *
 * Must be called before logging data for that sensor.
 *
 * @param m         Manager
 * @param id        Sensor ID (0 .. capacity-1)
 * @param name      Human readable name
 * @param buf_size  Ring buffer capacity for this sensor
 * @return true on success, false if slot taken or manager full
 */
bool manager_register(manager_t *m, uint8_t id,
                      const char *name, size_t buf_size);

/**
 * @brief Set alert thresholds for a sensor.
 *
 * Optional - sensors work without thresholds.
 * Call after manager_register().
 *
 * @param m          Manager
 * @param id         Sensor ID
 * @param thresholds Threshold configuration to apply
 * @return true on success
 */
bool manager_set_thresholds(manager_t *m, uint8_t id,
                            sensor_threshold_t thresholds);

/**
 * @brief Log a reading for a specific sensor.
 *
 * Automatically checks thresholds and prints alerts if configured.
 *
 * @param m          Manager
 * @param id         Sensor ID
 * @param value      Measured value
 * @param timestamp  Timestamp for this reading
 * @return true on success, false if sensor not registered or buffer full
 */
bool manager_log(manager_t *m, uint8_t id,
                 float value, uint32_t timestamp);

/**
 * @brief Read the oldest entry from a specific sensor.
 * @param m       Manager
 * @param id      Sensor ID
 * @param output  Destination for the reading
 * @return true on success, false if empty or not registered
 */
bool manager_read(manager_t *m, uint8_t id, sensor_reading_t *output);

/**
 * @brief Pause a specific sensor (writes rejected, buffer preserved).
 * @return true on success
 */
bool manager_pause_sensor(manager_t *m, uint8_t id);

/**
 * @brief Resume a paused sensor.
 * @return true on success
 */
bool manager_resume_sensor(manager_t *m, uint8_t id);

/**
 * @brief Flush one sensor's buffer and reset its statistics.
 */
void manager_flush_sensor(manager_t *m, uint8_t id);

/**
 * @brief Flush ALL sensors at once.
 */
void manager_flush_all(manager_t *m);

/**
 * @brief Check the current alert level for a sensor.
 *
 * Returns ALERT_NONE if thresholds not configured.
 *
 * @param m      Manager
 * @param id     Sensor ID
 * @param value  Value to check against thresholds
 * @return       Alert level
 */
alert_level_t manager_check_threshold(const manager_t *m,
                                      uint8_t id, float value);

/**
 * @brief Print a summary of all registered sensors.
 */
void manager_print_all(const manager_t *m);

/**
 * @brief Print manager-level statistics (total logs, alerts, etc.)
 */
void manager_print_stats(const manager_t *m);

#endif /* SENSOR_MANAGER_H */
