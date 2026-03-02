/**
 * @file sensor_manager.c
 * @brief Sensor manager implementation
 *
 * Key design decisions explained:
 *
 * 1. The manager uses a FIXED ARRAY of sensor_t, not malloc per sensor.
 *    We know at compile time how many sensors you have.
 *
 * 2. Sensor IDs are used as array indices directly.
 *    sensor[0] = ID 0, sensor[1] = ID 1, etc.
 *    This makes lookup O(1) - no searching needed.
 *    The tradeoff is IDs must be 0..capacity-1.
 *
 * 3. The `registered[]` boolean array tracks which slots are in use.
 *    This lets us distinguish "slot 3 is empty" from
 *    "slot 3 has a sensor with value 0.0".
 */

#include "sensor_manager.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * PRIVATE HELPERS
 * ========================================================================== */

/**
 * @brief Check if an ID is valid and registered.
 *
 * Every public function calls this first. Centralising the check
 * means we only write this logic once.
 */
static bool is_valid(const manager_t *m, uint8_t id)
{
    if (m == NULL)
        return false;
    if (id >= m->capacity)
        return false;
    if (!m->registered[id])
        return false;
    return true;
}

/**
 * @brief Evaluate a value against a sensor's thresholds.
 *
 * Checks critical first (more severe), then warning.
 * Returns ALERT_NONE if thresholds disabled or not configured.
 */
static alert_level_t evaluate_threshold(const sensor_threshold_t *t,
                                        float value)
{
    if (t == NULL || !t->enabled)
        return ALERT_NONE;

    /* Critical check first - it overrides warning */
    if (value >= t->critical_high || value <= t->critical_low)
        return ALERT_CRITICAL;

    if (value >= t->warn_high || value <= t->warn_low)
        return ALERT_WARNING;

    return ALERT_NONE;
}

/**
 * @brief Print a formatted alert message.
 *
 * Kept separate so it's easy to replace with a different
 * output method (e.g. writing to a log file, sending over serial).
 */
static void print_alert(const manager_t *m, uint8_t id,
                        alert_level_t level, float value,
                        uint32_t timestamp)
{
    const char *sensor_name = m->sensors[id].name;
    const char *level_str = (level == ALERT_CRITICAL) ? "CRITICAL" : "WARNING";
    const char *color_start = (level == ALERT_CRITICAL) ? "!!!" : "!";

    printf("\n%s ALERT [%s] Sensor '%s' (id=%u): value=%.2f at t=%u %s\n\n",
           color_start, level_str, sensor_name, id,
           value, timestamp, color_start);
}

/* ============================================================================
 * PUBLIC API
 * ========================================================================== */

manager_t *manager_create(uint8_t capacity)
{
    /* Validate capacity */
    if (capacity == 0 || capacity > MANAGER_MAX_SENSORS)
    {
        printf("[MANAGER] ERROR: capacity must be 1..%d, got %u\n",
               MANAGER_MAX_SENSORS, capacity);
        return NULL;
    }

    manager_t *m = malloc(sizeof(manager_t));
    if (m == NULL)
        return NULL;

    /*
     * Zero the entire struct first.
     * This sets all registered[] to false, all counts to 0,
     * and all pointers inside sensors[] to NULL in one shot.
     * Safer than initialising each field manually.
     */
    memset(m, 0, sizeof(manager_t));

    m->capacity = capacity;
    m->count = 0;
    m->total_logs = 0;
    m->total_alerts = 0;

    printf("[MANAGER] Created manager (capacity=%u)\n", capacity);
    return m;
}

void manager_destroy(manager_t *m)
{
    if (m == NULL)
        return;

    /*
     * Destroy each registered sensor.
     * sensor_destroy() frees the ring buffer each sensor owns.
     * Without this loop we'd leak every sensor's buffer memory.
     */
    for (uint8_t i = 0; i < m->capacity; i++)
    {
        if (m->registered[i])
        {
            sensor_destroy(&m->sensors[i]);
        }
    }

    free(m);
    printf("[MANAGER] Destroyed\n");
}

bool manager_register(manager_t *m, uint8_t id,
                      const char *name, size_t buf_size)
{
    if (m == NULL || name == NULL)
        return false;
    if (id >= m->capacity)
        return false;
    if (m->registered[id])
        return false; /* already taken */
    if (m->count >= m->capacity)
        return false; /* manager full */

    /*
     * sensor_init allocates the ring buffer for this sensor.
     * The sensor lives at m->sensors[id] - no extra malloc needed.
     * The ID in the array and the sensor's own ID match intentionally.
     */
    if (!sensor_init(&m->sensors[id], id, name, buf_size))
    {
        printf("[MANAGER] ERROR: Failed to init sensor id=%u\n", id);
        return false;
    }

    /* Mark slot as in use and set default thresholds (disabled) */
    m->registered[id] = true;
    m->thresholds[id].enabled = false;
    m->count++;

    printf("[MANAGER] Registered sensor id=%u name='%s' buf=%zu\n",
           id, name, buf_size);
    return true;
}

bool manager_set_thresholds(manager_t *m, uint8_t id,
                            sensor_threshold_t thresholds)
{
    if (!is_valid(m, id))
        return false;

    m->thresholds[id] = thresholds;
    m->thresholds[id].enabled = true;

    printf("[MANAGER] Thresholds set for sensor id=%u "
           "warn=[%.1f, %.1f] critical=[%.1f, %.1f]\n",
           id,
           thresholds.warn_low, thresholds.warn_high,
           thresholds.critical_low, thresholds.critical_high);
    return true;
}

bool manager_log(manager_t *m, uint8_t id,
                 float value, uint32_t timestamp)
{
    if (!is_valid(m, id))
        return false;

    /* Check thresholds BEFORE logging so alert fires on every bad value */
    alert_level_t level = evaluate_threshold(&m->thresholds[id], value);
    if (level != ALERT_NONE)
    {
        print_alert(m, id, level, value, timestamp);
        m->total_alerts++;
    }

    /* Log the value through the sensor layer */
    if (!sensor_log(&m->sensors[id], value, timestamp))
        return false;

    m->total_logs++;
    return true;
}

bool manager_read(manager_t *m, uint8_t id, sensor_reading_t *output)
{
    if (!is_valid(m, id) || output == NULL)
        return false;

    return sensor_read(&m->sensors[id], output);
}

bool manager_pause_sensor(manager_t *m, uint8_t id)
{
    if (!is_valid(m, id))
        return false;

    return sensor_pause(&m->sensors[id]);
}

bool manager_resume_sensor(manager_t *m, uint8_t id)
{
    if (!is_valid(m, id))
        return false;

    return sensor_resume(&m->sensors[id]);
}

void manager_flush_sensor(manager_t *m, uint8_t id)
{
    if (!is_valid(m, id))
        return;

    sensor_flush(&m->sensors[id]);
}

void manager_flush_all(manager_t *m)
{
    if (m == NULL)
        return;

    for (uint8_t i = 0; i < m->capacity; i++)
    {
        if (m->registered[i])
            sensor_flush(&m->sensors[i]);
    }

    printf("[MANAGER] All sensors flushed\n");
}

alert_level_t manager_check_threshold(const manager_t *m,
                                      uint8_t id, float value)
{
    if (!is_valid(m, id))
        return ALERT_NONE;

    return evaluate_threshold(&m->thresholds[id], value);
}

void manager_print_all(const manager_t *m)
{
    if (m == NULL)
    {
        printf("Manager: NULL\n");
        return;
    }

    printf("\n========================================\n");
    printf("  SENSOR MANAGER  (%u / %u sensors active)\n",
           m->count, m->capacity);
    printf("========================================\n");

    bool any = false;
    for (uint8_t i = 0; i < m->capacity; i++)
    {
        if (m->registered[i])
        {
            sensor_print_info(&m->sensors[i]);
            any = true;
        }
    }

    if (!any)
        printf("  No sensors registered yet.\n");

    printf("========================================\n");
}

void manager_print_stats(const manager_t *m)
{
    if (m == NULL)
        return;

    printf("\n--- Manager Statistics ---\n");
    printf("  Sensors registered : %u / %u\n", m->count, m->capacity);
    printf("  Total readings     : %" PRIu32 "\n", m->total_logs);
    printf("  Total alerts fired : %" PRIu32 "\n", m->total_alerts);

    /* Per-sensor overflow summary */
    printf("  Overflow summary   :\n");
    for (uint8_t i = 0; i < m->capacity; i++)
    {
        if (m->registered[i])
        {
            uint32_t ov = buffer_overflow_count(m->sensors[i].buf);
            if (ov > 0)
                printf("    Sensor '%s' (id=%u): %" PRIu32 " overflows\n",
                       m->sensors[i].name, i, ov);
        }
    }
    printf("--------------------------\n");
}
