/**
 * @file main.c
 * @brief Sensor data logger — integration demo
 */

#include "sensors.h"
#include <stdio.h>
#include <stdint.h>

/* Simulated millisecond tick */
static uint32_t tick = 0;
static uint32_t now(void) { return tick += 100; }

int main(void)
{
    printf("=== Sensor Data Logger Demo ===\n\n");

    /* --- Create two sensors ------------------------------------------- */
    sensor_t temp, pressure;

    if (!sensor_init(&temp,     0, "Temperature (C)", 8) ||
        !sensor_init(&pressure, 1, "Pressure (hPa)",  8)) {
        printf("ERROR: sensor init failed\n");
        return 1;
    }

    /* --- Log readings -------------------------------------------------- */
    printf("Logging readings...\n");
    float temp_data[]     = { 21.1f, 21.5f, 22.0f, 21.8f, 22.3f };
    float pressure_data[] = { 1012.1f, 1012.4f, 1011.9f, 1012.7f, 1013.0f };

    for (int i = 0; i < 5; i++) {
        sensor_log(&temp,     temp_data[i],     now());
        sensor_log(&pressure, pressure_data[i], now());
    }

    sensor_print_info(&temp);
    sensor_print_info(&pressure);

    /* --- Pause test ----------------------------------------------------- */
    printf("\nPausing temperature sensor...\n");
    sensor_pause(&temp);
    if (!sensor_log(&temp, 99.9f, now()))
        printf("  Write correctly rejected while paused.\n");
    sensor_resume(&temp);

    /* --- Drain temperature buffer -------------------------------------- */
    printf("\nDraining temperature buffer:\n");
    sensor_reading_t r;
    while (sensor_read(&temp, &r))
        printf("  t=%u  sensor=%u  value=%.1f\n",
               r.timestamp, r.sensor_id, r.value);

    /* --- Overflow demo ------------------------------------------------- */
    printf("\nOverflow demo (10 writes into capacity-8 buffer):\n");
    for (int i = 0; i < 10; i++)
        sensor_log(&pressure, 1000.0f + i, now());
    printf("  Overflow count: %u\n", buffer_overflow_count(pressure.buf));

    /* --- Final state --------------------------------------------------- */
    printf("\nFinal state:\n");
    sensor_print_info(&temp);
    sensor_print_info(&pressure);

    /* --- Cleanup ------------------------------------------------------- */
    sensor_destroy(&temp);
    sensor_destroy(&pressure);

    printf("\n=== Done ===\n");
    return 0;
}
