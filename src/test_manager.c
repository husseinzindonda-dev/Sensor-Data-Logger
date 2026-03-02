/**
 * @file test_manager.c
 * @brief Unit tests for the sensor manager
 *
 * Build:
 *   gcc src/buffer.c src/sensors.c src/sensor_manager.c tests/test_manager.c
 *       -o build/test_manager.exe -Wall -Wextra -Werror -std=c11 -g -lm
 */

#include "../src/sensor_manager.h"
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * TEST FRAMEWORK
 * ========================================================================== */

static int g_pass = 0;
static int g_fail = 0;

#define ASSERT(cond, msg)                                     \
    do                                                        \
    {                                                         \
        if (cond)                                             \
        {                                                     \
            printf("  PASS  %s\n", msg);                      \
            g_pass++;                                         \
        }                                                     \
        else                                                  \
        {                                                     \
            printf("  FAIL  %s  [line %d]\n", msg, __LINE__); \
            g_fail++;                                         \
        }                                                     \
    } while (0)

#define ASSERT_EQ(a, b, msg) ASSERT((a) == (b), msg)
#define ASSERT_TRUE(x, msg) ASSERT((x), msg)
#define ASSERT_FALSE(x, msg) ASSERT(!(x), msg)
#define ASSERT_NEAR(a, b, eps, msg) ASSERT(fabsf((a) - (b)) < (eps), msg)

static void test_header(const char *name)
{
    printf("\n[TEST] %s\n", name);
}

/* ============================================================================
 * TESTS
 * ========================================================================== */

static void test_create_destroy(void)
{
    test_header("manager_create / manager_destroy");

    manager_t *m = manager_create(3);
    ASSERT_TRUE(m != NULL, "create returns non-NULL");
    ASSERT_EQ(m->capacity, 3, "capacity set correctly");
    ASSERT_EQ(m->count, 0, "starts with 0 registered sensors");
    ASSERT_EQ(m->total_logs, 0, "total_logs starts at 0");
    ASSERT_EQ(m->total_alerts, 0, "total_alerts starts at 0");

    manager_destroy(m);
    ASSERT_TRUE(1, "destroy did not crash");

    /* Edge cases */
    manager_destroy(NULL);
    ASSERT_TRUE(1, "destroy(NULL) is safe");

    manager_t *bad = manager_create(0);
    ASSERT_FALSE(bad, "capacity 0 returns NULL");

    manager_t *over = manager_create(MANAGER_MAX_SENSORS + 1);
    ASSERT_FALSE(over, "over-capacity returns NULL");
}

static void test_register(void)
{
    test_header("manager_register");

    manager_t *m = manager_create(3);

    ASSERT_TRUE(manager_register(m, 0, "Temp", 8), "register id=0 ok");
    ASSERT_TRUE(manager_register(m, 1, "Pressure", 8), "register id=1 ok");
    ASSERT_EQ(m->count, 2, "count == 2");

    /* Duplicate ID */
    ASSERT_FALSE(manager_register(m, 0, "Dup", 8), "duplicate id rejected");

    /* Out of range ID */
    ASSERT_FALSE(manager_register(m, 5, "OOB", 8), "out-of-range id rejected");

    /* NULL args */
    ASSERT_FALSE(manager_register(m, 2, NULL, 8), "NULL name rejected");
    ASSERT_FALSE(manager_register(NULL, 2, "X", 8), "NULL manager rejected");

    manager_destroy(m);
}

static void test_log_and_read(void)
{
    test_header("manager_log / manager_read");

    manager_t *m = manager_create(2);
    manager_register(m, 0, "Temp", 8);
    manager_register(m, 1, "Pressure", 8);

    ASSERT_TRUE(manager_log(m, 0, 25.0f, 1000), "log sensor 0 ok");
    ASSERT_TRUE(manager_log(m, 1, 1013.f, 1001), "log sensor 1 ok");
    ASSERT_EQ(m->total_logs, 2, "total_logs == 2");

    /* Unregistered sensor */
    ASSERT_FALSE(manager_log(m, 5, 0.0f, 0), "unregistered id rejected");

    sensor_reading_t out;
    ASSERT_TRUE(manager_read(m, 0, &out), "read sensor 0 ok");
    ASSERT_NEAR(out.value, 25.0f, 0.001f, "correct value read back");
    ASSERT_EQ(out.sensor_id, 0, "correct sensor_id");

    manager_destroy(m);
}

static void test_thresholds(void)
{
    test_header("threshold alerts");

    manager_t *m = manager_create(1);
    manager_register(m, 0, "Temperature", 16);

    sensor_threshold_t t = {
        .warn_low = 0.0f,
        .warn_high = 70.0f,
        .critical_low = -10.0f,
        .critical_high = 85.0f,
        .enabled = true};
    ASSERT_TRUE(manager_set_thresholds(m, 0, t), "set_thresholds ok");

    /* Values within normal range */
    ASSERT_EQ(manager_check_threshold(m, 0, 50.0f),
              ALERT_NONE, "50C = ALERT_NONE");

    /* Warning range */
    ASSERT_EQ(manager_check_threshold(m, 0, 75.0f),
              ALERT_WARNING, "75C = ALERT_WARNING");

    /* Critical range */
    ASSERT_EQ(manager_check_threshold(m, 0, 90.0f),
              ALERT_CRITICAL, "90C = ALERT_CRITICAL");

    /* Low critical */
    ASSERT_EQ(manager_check_threshold(m, 0, -15.0f),
              ALERT_CRITICAL, "-15C = ALERT_CRITICAL");

    /* Log a critical value - total_alerts should increment */
    manager_log(m, 0, 90.0f, 1000);
    ASSERT_EQ(m->total_alerts, 1, "total_alerts incremented on alert");

    manager_destroy(m);
}

static void test_pause_resume(void)
{
    test_header("manager_pause / manager_resume");

    manager_t *m = manager_create(1);
    manager_register(m, 0, "Vibration", 8);

    ASSERT_TRUE(manager_pause_sensor(m, 0), "pause ok");
    ASSERT_FALSE(manager_log(m, 0, 0.5f, 100), "log rejected while paused");

    ASSERT_TRUE(manager_resume_sensor(m, 0), "resume ok");
    ASSERT_TRUE(manager_log(m, 0, 0.5f, 200), "log accepted after resume");

    /* Unregistered sensor */
    ASSERT_FALSE(manager_pause_sensor(m, 7), "pause unregistered = false");
    ASSERT_FALSE(manager_resume_sensor(m, 7), "resume unregistered = false");

    manager_destroy(m);
}

static void test_flush(void)
{
    test_header("manager_flush_sensor / manager_flush_all");

    manager_t *m = manager_create(2);
    manager_register(m, 0, "Temp", 8);
    manager_register(m, 1, "Current", 8);

    manager_log(m, 0, 55.0f, 100);
    manager_log(m, 0, 56.0f, 200);
    manager_log(m, 1, 5.5f, 100);

    manager_flush_sensor(m, 0);
    ASSERT_TRUE(buffer_is_empty(m->sensors[0].buf), "sensor 0 empty after flush");
    ASSERT_FALSE(buffer_is_empty(m->sensors[1].buf), "sensor 1 unaffected");

    manager_flush_all(m);
    ASSERT_TRUE(buffer_is_empty(m->sensors[1].buf), "sensor 1 empty after flush_all");

    manager_destroy(m);
}

static void test_multiple_sensors(void)
{
    test_header("multiple sensors logged simultaneously");

    manager_t *m = manager_create(3);
    manager_register(m, 0, "Temperature", 8);
    manager_register(m, 1, "Vibration", 8);
    manager_register(m, 2, "Current", 8);

    /* Log 5 readings per sensor */
    for (int i = 0; i < 5; i++)
    {
        manager_log(m, 0, 40.0f + i, (uint32_t)(i * 1000));
        manager_log(m, 1, 0.1f + i * 0.01f, (uint32_t)(i * 1000));
        manager_log(m, 2, 5.0f + i * 0.1f, (uint32_t)(i * 1000));
    }

    ASSERT_EQ(m->total_logs, 15, "total_logs == 15 (5 per sensor)");
    ASSERT_EQ(buffer_count(m->sensors[0].buf), 5, "sensor 0 has 5 entries");
    ASSERT_EQ(buffer_count(m->sensors[1].buf), 5, "sensor 1 has 5 entries");
    ASSERT_EQ(buffer_count(m->sensors[2].buf), 5, "sensor 2 has 5 entries");

    /* Verify FIFO order on sensor 0 */
    sensor_reading_t out;
    manager_read(m, 0, &out);
    ASSERT_NEAR(out.value, 40.0f, 0.001f, "sensor 0 oldest = 40.0");

    manager_destroy(m);
}

/* ============================================================================
 * MAIN
 * ========================================================================== */

int main(void)
{
    printf("==============================\n");
    printf("  Sensor Manager Test Suite\n");
    printf("==============================\n");

    test_create_destroy();
    test_register();
    test_log_and_read();
    test_thresholds();
    test_pause_resume();
    test_flush();
    test_multiple_sensors();

    printf("\n==============================\n");
    printf("  Results: %d passed, %d failed\n", g_pass, g_fail);
    printf("==============================\n");

    return (g_fail == 0) ? 0 : 1;
}
