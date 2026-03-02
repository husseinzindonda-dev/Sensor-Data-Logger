/**
 * @file logger.c
 * @brief CSV file logger implementation
 *
 * Key concepts used here:
 *
 * FILE* and stdio:
 *   fopen()   - opens a file, returns a FILE* handle
 *   fprintf() - like printf() but writes to a file instead of terminal
 *   fflush()  - forces buffered data to be written to disk immediately
 *   fclose()  - closes the file and releases the handle
 *
 * Why fflush() matters:
 *   The OS buffers file writes for performance. Without fflush(), data
 *   sits in memory and may not reach the file for seconds. The Python
 *   dashboard needs to see new rows quickly, so we flush after every write.
 *
 * Append mode ("a"):
 *   Opening with "a" means new data is added to the end of the file.
 *   If the file doesn't exist, it's created. If it does, nothing is
 *   overwritten. This lets you restart the C program without losing
 *   previous readings.
 */

#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

/* ============================================================================
 * PRIVATE HELPERS
 * ========================================================================== */

/** Convert alert_level_t to a readable string for the CSV */
static const char *alert_to_str(alert_level_t level)
{
    switch (level)
    {
    case ALERT_WARNING:
        return "WARNING";
    case ALERT_CRITICAL:
        return "CRITICAL";
    default:
        return "NONE";
    }
}

/**
 * @brief Check if a file is empty (needs a header row).
 *
 * We open in append mode so we can't use fseek to check size easily.
 * Instead we open briefly in read mode first to test if it's empty.
 */
static bool file_is_empty(const char *filepath)
{
    FILE *f = fopen(filepath, "r");
    if (f == NULL)
        return true; /* doesn't exist yet = treat as empty */

    int ch = fgetc(f);
    fclose(f);
    return (ch == EOF);
}

/* ============================================================================
 * PUBLIC API
 * ========================================================================== */

bool logger_open(csv_logger_t *logger, const char *filepath)
{
    if (logger == NULL || filepath == NULL)
        return false;

    /* Copy the path so we can check it later */
    strncpy(logger->filepath, filepath, LOGGER_PATH_MAX - 1);
    logger->filepath[LOGGER_PATH_MAX - 1] = '\0';

    bool needs_header = file_is_empty(filepath);

    /*
     * Open in append mode:
     *   "a" = append text, create if not exists
     *
     * We cast to void* in the struct to avoid pulling <stdio.h>
     * into the header. Here we cast back to FILE*.
     */
    FILE *f = fopen(filepath, "a");
    if (f == NULL)
    {
        printf("[LOGGER] ERROR: Could not open file '%s'\n", filepath);
        return false;
    }

    logger->file = (void *)f;
    logger->rows_written = 0;
    logger->is_open = true;

    /* Write CSV header if this is a new/empty file */
    if (needs_header)
    {
        fprintf(f, "timestamp,sensor_id,sensor_name,value,alert_level\n");
        fflush(f);
        printf("[LOGGER] Created '%s' with header\n", filepath);
    }
    else
    {
        printf("[LOGGER] Appending to existing '%s'\n", filepath);
    }

    return true;
}

void logger_close(csv_logger_t *logger)
{
    if (logger == NULL || !logger->is_open)
        return;

    fclose((FILE *)logger->file);
    logger->file = NULL;
    logger->is_open = false;

    printf("[LOGGER] Closed '%s' (%" PRIu32 " rows written)\n",
           logger->filepath, logger->rows_written);
}

bool logger_write(csv_logger_t *logger,
                  const sensor_reading_t *reading,
                  const char *sensor_name,
                  alert_level_t alert)
{
    if (logger == NULL || !logger->is_open || reading == NULL)
        return false;

    FILE *f = (FILE *)logger->file;

    /*
     * Write one CSV row:
     *   timestamp, sensor_id, sensor_name, value, alert_level
     *
     * %.4f gives 4 decimal places - enough precision for
     * temperature (21.1500), vibration (0.1300), current (5.2400)
     */
    fprintf(f, "%" PRIu32 ",%" PRIu8 ",%s,%.4f,%s\n",
            reading->timestamp,
            reading->sensor_id,
            sensor_name,
            reading->value,
            alert_to_str(alert));

    logger->rows_written++;

    /*
     * Flush immediately so Python sees the row right away.
     * On a real embedded system you might flush every N rows
     * to reduce SD card wear, but for a PC demo this is fine.
     */
    fflush(f);

    return true;
}

void logger_flush(csv_logger_t *logger)
{
    if (logger == NULL || !logger->is_open)
        return;

    fflush((FILE *)logger->file);
}

uint32_t logger_rows_written(const csv_logger_t *logger)
{
    return (logger == NULL) ? 0 : logger->rows_written;
}
