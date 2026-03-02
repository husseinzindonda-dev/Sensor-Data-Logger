/**
 * @file main.c
 * @brief Predictive maintenance monitor - Phase 2
 *
 * Same three-sensor simulation as Phase 1 but now every reading
 * is written to data/sensor_log.csv so the Python dashboard
 * can visualise it in real time.
 *
 * Run this first, then run dashboard.py in a second terminal.
 */

#include "sensor_manager.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define SENSOR_TEMP 0
#define SENSOR_VIBRATION 1
#define SENSOR_CURRENT 2

/* Simulated millisecond tick */
static uint32_t tick = 0;
static uint32_t now(void) { return tick += 1000; }

/**
 * @brief Log a reading through the manager AND write it to CSV.
 *
 * This wrapper keeps main.c clean - one call does both jobs.
 * It checks thresholds via the manager, then writes the result
 * (including alert level) to the CSV file.
 */
static void log_and_record(manager_t *m, csv_logger_t *logger,
                           uint8_t id, float value, uint32_t timestamp)
{
    /* Check threshold before logging so we capture the alert level */
    alert_level_t alert = manager_check_threshold(m, id, value);

    /* Log through manager (updates buffer + stats) */
    manager_log(m, id, value, timestamp);

    /* Write to CSV with sensor name and alert level */
    sensor_reading_t r = {.timestamp = timestamp,
                          .sensor_id = id,
                          .value = value};
    logger_write(logger, &r, m->sensors[id].name, alert);
}

int main(void)
{
    printf("=== Predictive Maintenance Monitor (Phase 2) ===\n\n");

    /* ----------------------------------------------------------------
     * 1. Create data/ directory and open CSV logger
     * ---------------------------------------------------------------- */
#ifdef _WIN32
    system("if not exist data mkdir data");
#else
    system("mkdir -p data");
#endif

    csv_logger_t logger;
    if (!logger_open(&logger, "data/sensor_log.csv"))
    {
        printf("ERROR: Could not open log file\n");
        return 1;
    }

    /* ----------------------------------------------------------------
     * 2. Create manager and register sensors
     * ---------------------------------------------------------------- */
    manager_t *m = manager_create(3);
    if (m == NULL)
    {
        logger_close(&logger);
        return 1;
    }

    manager_register(m, SENSOR_TEMP, "Temperature (C)", 16);
    manager_register(m, SENSOR_VIBRATION, "Vibration (g)", 16);
    manager_register(m, SENSOR_CURRENT, "Current Draw (A)", 16);

    /* ----------------------------------------------------------------
     * 3. Set thresholds
     * ---------------------------------------------------------------- */
    manager_set_thresholds(m, SENSOR_TEMP, (sensor_threshold_t){.warn_low = 0.0f, .warn_high = 70.0f, .critical_low = -10.0f, .critical_high = 85.0f, .enabled = true});
    manager_set_thresholds(m, SENSOR_VIBRATION, (sensor_threshold_t){.warn_low = 0.0f, .warn_high = 0.5f, .critical_low = 0.0f, .critical_high = 1.0f, .enabled = true});
    manager_set_thresholds(m, SENSOR_CURRENT, (sensor_threshold_t){.warn_low = 0.0f, .warn_high = 8.0f, .critical_low = 0.0f, .critical_high = 10.0f, .enabled = true});

    /* ----------------------------------------------------------------
     * 4. Phase 1: Normal operation
     * ---------------------------------------------------------------- */
    printf("\n--- Phase 1: Normal operation ---\n");
    float normal_temps[] = {42.0f, 43.5f, 44.0f, 43.0f, 44.5f};
    float normal_vibs[] = {0.12f, 0.15f, 0.11f, 0.14f, 0.13f};
    float normal_amps[] = {5.1f, 5.3f, 5.2f, 5.4f, 5.2f};

    for (int i = 0; i < 5; i++)
    {
        log_and_record(m, &logger, SENSOR_TEMP, normal_temps[i], now());
        log_and_record(m, &logger, SENSOR_VIBRATION, normal_vibs[i], now());
        log_and_record(m, &logger, SENSOR_CURRENT, normal_amps[i], now());
    }
    printf("  Written %u rows to CSV\n", logger_rows_written(&logger));

    /* ----------------------------------------------------------------
     * 5. Phase 2: Early warning signs
     * ---------------------------------------------------------------- */
    printf("\n--- Phase 2: Early warning signs ---\n");
    float warn_temps[] = {65.0f, 68.0f, 71.0f};
    float warn_vibs[] = {0.45f, 0.51f, 0.58f};
    float warn_amps[] = {7.5f, 8.1f, 8.4f};

    for (int i = 0; i < 3; i++)
    {
        log_and_record(m, &logger, SENSOR_TEMP, warn_temps[i], now());
        log_and_record(m, &logger, SENSOR_VIBRATION, warn_vibs[i], now());
        log_and_record(m, &logger, SENSOR_CURRENT, warn_amps[i], now());
    }
    printf("  Written %u rows to CSV\n", logger_rows_written(&logger));

    /* ----------------------------------------------------------------
     * 6. Phase 3: Critical failure
     * ---------------------------------------------------------------- */
    printf("\n--- Phase 3: Critical failure ---\n");
    log_and_record(m, &logger, SENSOR_TEMP, 91.0f, now());
    log_and_record(m, &logger, SENSOR_VIBRATION, 1.4f, now());
    log_and_record(m, &logger, SENSOR_CURRENT, 11.2f, now());
    printf("  Written %u rows to CSV\n", logger_rows_written(&logger));

    /* ----------------------------------------------------------------
     * 7. Final report
     * ---------------------------------------------------------------- */
    manager_print_all(m);
    manager_print_stats(m);
    printf("\nTotal CSV rows written: %u\n",
           logger_rows_written(&logger));
    printf("CSV file: data/sensor_log.csv\n");
    printf("Run dashboard: python dashboard/dashboard.py\n");

    /* ----------------------------------------------------------------
     * 8. Cleanup
     * ---------------------------------------------------------------- */
    logger_close(&logger);
    manager_destroy(m);

    printf("\n=== Done ===\n");
    return 0;
}
