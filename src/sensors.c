/**
 * @file sensors.c
 * @brief Sensor abstraction layer implementation
 */

#include "sensors.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <float.h>      /* FLT_MAX */

/* ============================================================================
 * PRIVATE HELPERS
 * ========================================================================== */

static void stats_update(sensor_stats_t *stats, float value)
{
    if (value < stats->min) stats->min = value;
    if (value > stats->max) stats->max = value;
    stats->sum += value;
    stats->sample_count++;
}

static void stats_reset(sensor_stats_t *stats)
{
    stats->min          =  FLT_MAX;
    stats->max          = -FLT_MAX;
    stats->sum          = 0.0f;
    stats->sample_count = 0;
}

/* ============================================================================
 * PUBLIC API
 * ========================================================================== */

bool sensor_init(sensor_t *sensor, uint8_t id, const char *name, size_t capacity)
{
    if (sensor == NULL || name == NULL || capacity == 0)
        return false;

    sensor->id    = id;
    sensor->state = SENSOR_STATE_UNINIT;

    strncpy(sensor->name, name, SENSOR_NAME_MAX - 1);
    sensor->name[SENSOR_NAME_MAX - 1] = '\0';

    sensor->buf = buffer_create(capacity);
    if (sensor->buf == NULL)
        return false;

    stats_reset(&sensor->stats);
    sensor->state = SENSOR_STATE_ACTIVE;
    return true;
}

void sensor_destroy(sensor_t *sensor)
{
    if (sensor == NULL)
        return;

    buffer_destroy(sensor->buf);
    sensor->buf   = NULL;
    sensor->state = SENSOR_STATE_UNINIT;
}

bool sensor_log(sensor_t *sensor, float value, uint32_t timestamp)
{
    if (sensor == NULL || sensor->buf == NULL)
        return false;

    if (sensor->state != SENSOR_STATE_ACTIVE)
        return false;

    sensor_reading_t r = {
        .timestamp = timestamp,
        .sensor_id = sensor->id,
        .value     = value
    };

    if (!buffer_write(sensor->buf, &r))
        return false;

    stats_update(&sensor->stats, value);
    return true;
}

bool sensor_read(sensor_t *sensor, sensor_reading_t *output)
{
    if (sensor == NULL || output == NULL)
        return false;

    return buffer_read(sensor->buf, output);
}

bool sensor_peek(const sensor_t *sensor, sensor_reading_t *output)
{
    if (sensor == NULL || output == NULL)
        return false;

    return buffer_peek(sensor->buf, output);
}

bool sensor_get_stats(const sensor_t *sensor, sensor_stats_t *stats)
{
    if (sensor == NULL || sensor->stats.sample_count == 0)
        return false;

    if (stats != NULL)
        *stats = sensor->stats;

    return true;
}

bool sensor_get_mean(const sensor_t *sensor, float *mean)
{
    if (sensor == NULL || mean == NULL || sensor->stats.sample_count == 0)
        return false;

    *mean = sensor->stats.sum / (float)sensor->stats.sample_count;
    return true;
}

bool sensor_pause(sensor_t *sensor)
{
    if (sensor == NULL || sensor->state != SENSOR_STATE_ACTIVE)
        return false;

    sensor->state = SENSOR_STATE_PAUSED;
    return true;
}

bool sensor_resume(sensor_t *sensor)
{
    if (sensor == NULL || sensor->state != SENSOR_STATE_PAUSED)
        return false;

    sensor->state = SENSOR_STATE_ACTIVE;
    return true;
}

void sensor_flush(sensor_t *sensor)
{
    if (sensor == NULL)
        return;

    buffer_clear(sensor->buf);
    stats_reset(&sensor->stats);
}

void sensor_print_info(const sensor_t *sensor)
{
    if (sensor == NULL) {
        printf("Sensor: NULL\n");
        return;
    }

    static const char *state_names[] = {
        [SENSOR_STATE_UNINIT] = "UNINIT",
        [SENSOR_STATE_ACTIVE] = "ACTIVE",
        [SENSOR_STATE_PAUSED] = "PAUSED",
        [SENSOR_STATE_ERROR]  = "ERROR"
    };

    printf("--- Sensor #%"PRIu8" : %s ---\n", sensor->id, sensor->name);
    printf("  State    : %s\n", state_names[sensor->state]);
    printf("  Buffered : %zu / %zu entries\n",
           buffer_count(sensor->buf),
           sensor->buf ? sensor->buf->capacity : 0);
    printf("  Overflows: %"PRIu32"\n", buffer_overflow_count(sensor->buf));

    if (sensor->stats.sample_count > 0) {
        float mean = sensor->stats.sum / (float)sensor->stats.sample_count;
        printf("  Samples  : %"PRIu32"\n", sensor->stats.sample_count);
        printf("  Min/Max  : %.2f / %.2f\n", sensor->stats.min, sensor->stats.max);
        printf("  Mean     : %.2f\n", mean);
    } else {
        printf("  No samples recorded yet\n");
    }
}
