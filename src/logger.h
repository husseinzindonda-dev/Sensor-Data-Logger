/**
 * @file logger.h
 * @brief CSV file logger for sensor readings
 *
 * Writes sensor readings to a CSV file as they are logged.
 * The file can be read by the Python dashboard in real time.
 *
 * Output format:
 *   timestamp,sensor_id,sensor_name,value,alert_level
 *   1000,0,Temperature (C),42.50,NONE
 *   2000,1,Vibration (g),0.13,NONE
 *   3000,0,Temperature (C),71.00,WARNING
 */

#ifndef LOGGER_H
#define LOGGER_H

#include "sensor_manager.h"
#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * CONFIGURATION
 * ========================================================================== */

/** @brief Maximum file path length */
#define LOGGER_PATH_MAX 256

/* ============================================================================
 * DATA TYPES
 * ========================================================================== */

/**
 * @brief Logger control structure
 *
 * Holds the open file handle and running write count.
 * One logger instance can serve the whole manager.
 */
typedef struct
{
    char filepath[LOGGER_PATH_MAX]; ///< Path to the CSV file
    void *file;                     ///< FILE* handle (void* avoids stdio in header)
    uint32_t rows_written;          ///< Total rows written this session
    bool is_open;                   ///< True if file is open and ready
} csv_logger_t;

/* ============================================================================
 * PUBLIC API
 * ========================================================================== */

/**
 * @brief Open a CSV file for writing.
 *
 * Creates the file if it doesn't exist.
 * Appends to it if it already exists (so data survives restarts).
 * Writes the header row automatically on first creation.
 *
 * @param logger    Pointer to caller-allocated logger struct
 * @param filepath  Path to the CSV file (e.g. "data/sensor_log.csv")
 * @return true on success, false on failure
 */
bool logger_open(csv_logger_t *logger, const char *filepath);

/**
 * @brief Close the CSV file.
 * @param logger  Logger to close (NULL is safe)
 */
void logger_close(csv_logger_t *logger);

/**
 * @brief Write one sensor reading to the CSV file.
 *
 * Call this every time manager_log() succeeds.
 *
 * @param logger       Active logger
 * @param reading      The reading that was just logged
 * @param sensor_name  Name of the sensor (from manager)
 * @param alert        Alert level at time of logging
 * @return true on success
 */
bool logger_write(csv_logger_t *logger,
                  const sensor_reading_t *reading,
                  const char *sensor_name,
                  alert_level_t alert);

/**
 * @brief Flush buffered writes to disk immediately.
 *
 * Call this periodically so the Python dashboard
 * sees new data without waiting for the file to close.
 *
 * @param logger  Active logger
 */
void logger_flush(csv_logger_t *logger);

/**
 * @brief Return total rows written this session.
 */
uint32_t logger_rows_written(const csv_logger_t *logger);

#endif /* LOGGER_H */
