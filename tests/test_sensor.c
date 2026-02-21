/**
 * @file test_sensor.c
 * @brief Unit tests for the sensor abstraction layer
 *
 * Build:  gcc -Wall -Wextra -Werror -std=c11 -g src/buffer.c src/sensors.c tests/test_sensor.c -o build/test_sensor.exe -lm
 * Run:    ./build/test_sensor.exe
 */

#include "../src/sensors.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * MINIMAL TEST FRAMEWORK
 * ========================================================================== */

static int g_pass = 0;
static int g_fail = 0;

#define ASSERT(cond, msg)                                                   \
    do {                                                                    \
        if (cond) {                                                         \
            printf("  PASS  %s\n", msg);                                   \
            g_pass++;                                                       \
        } else {                                                            \
            printf("  FAIL  %s  [line %d]\n", msg, __LINE__);              \
            g_fail++;                                                       \
        }                                                                   \
    } while (0)

#define ASSERT_EQ(a, b, msg)        ASSERT((a) == (b), msg)
#define ASSERT_TRUE(x, msg)         ASSERT((x),         msg)
#define ASSERT_FALSE(x, msg)        ASSERT(!(x),        msg)
#define ASSERT_NEAR(a, b, eps, msg) ASSERT(fabsf((a)-(b)) < (eps), msg)

static void test_header(const char *name)
{
    printf("\n[TEST] %s\n", name);
}

/* ============================================================================
 * TESTS
 * ========================================================================== */

static void test_init_destroy(void)
{
    test_header("sensor_init / sensor_destroy");
    sensor_t s;

    ASSERT_TRUE(sensor_init(&s, 0, "Temperature", 16), "init succeeds");
    ASSERT_EQ(s.id, 0,                                  "id set correctly");
    ASSERT_TRUE(strcmp(s.name, "Temperature") == 0,     "name copied");
    ASSERT_EQ(s.state, SENSOR_STATE_ACTIVE,             "starts ACTIVE");
    ASSERT_TRUE(s.buf != NULL,                          "buffer allocated");

    sensor_destroy(&s);
    ASSERT_TRUE(s.buf == NULL,                          "buf NULL after destroy");
    ASSERT_EQ(s.state, SENSOR_STATE_UNINIT,             "state UNINIT after destroy");
}

static void test_init_bad_args(void)
{
    test_header("sensor_init — NULL / bad args");
    sensor_t s;
    ASSERT_FALSE(sensor_init(NULL, 0, "X", 4), "NULL sensor -> false");
    ASSERT_FALSE(sensor_init(&s, 0, NULL,  4), "NULL name -> false");
    ASSERT_FALSE(sensor_init(&s, 0, "X",   0), "capacity 0 -> false");
}

static void test_log_and_read(void)
{
    test_header("sensor_log / sensor_read — basic round trip");
    sensor_t s;
    sensor_init(&s, 1, "Pressure", 8);

    ASSERT_TRUE(sensor_log(&s, 101.3f, 1000), "log returns true");
    ASSERT_TRUE(sensor_log(&s, 102.1f, 1001), "second log ok");

    sensor_reading_t out;
    ASSERT_TRUE(sensor_read(&s, &out),        "read returns true");
    ASSERT_EQ(out.timestamp, 1000,             "oldest entry first (FIFO)");
    ASSERT_EQ(out.sensor_id, 1,               "sensor_id stamped by sensor_log");
    ASSERT_NEAR(out.value, 101.3f, 0.001f,    "value correct");

    sensor_destroy(&s);
}

static void test_stats(void)
{
    test_header("sensor stats — min, max, mean");
    sensor_t s;
    sensor_init(&s, 2, "Humidity", 16);

    float values[] = { 10.0f, 20.0f, 30.0f, 40.0f };
    for (int i = 0; i < 4; i++)
        sensor_log(&s, values[i], (uint32_t)(i * 100));

    sensor_stats_t stats;
    ASSERT_TRUE(sensor_get_stats(&s, &stats), "get_stats returns true");
    ASSERT_NEAR(stats.min, 10.0f, 0.001f,    "min == 10");
    ASSERT_NEAR(stats.max, 40.0f, 0.001f,    "max == 40");
    ASSERT_EQ(stats.sample_count, 4,          "sample_count == 4");

    float mean;
    ASSERT_TRUE(sensor_get_mean(&s, &mean),   "get_mean returns true");
    ASSERT_NEAR(mean, 25.0f, 0.001f,          "mean == 25");

    sensor_destroy(&s);
}

static void test_pause_resume(void)
{
    test_header("sensor_pause / sensor_resume");
    sensor_t s;
    sensor_init(&s, 3, "Voltage", 8);

    ASSERT_TRUE(sensor_pause(&s),             "pause returns true");
    ASSERT_EQ(s.state, SENSOR_STATE_PAUSED,  "state is PAUSED");
    ASSERT_FALSE(sensor_log(&s, 5.0f, 100),  "log rejected while paused");

    ASSERT_TRUE(sensor_resume(&s),            "resume returns true");
    ASSERT_EQ(s.state, SENSOR_STATE_ACTIVE,  "state back to ACTIVE");
    ASSERT_TRUE(sensor_log(&s, 5.0f, 200),   "log accepted after resume");

    sensor_pause(&s);
    ASSERT_FALSE(sensor_pause(&s),            "double-pause returns false");
    sensor_resume(&s);
    ASSERT_FALSE(sensor_resume(&s),           "double-resume returns false");

    sensor_destroy(&s);
}

static void test_flush(void)
{
    test_header("sensor_flush — clears buffer and stats");
    sensor_t s;
    sensor_init(&s, 4, "Current", 8);

    sensor_log(&s, 1.0f, 0);
    sensor_log(&s, 2.0f, 1);
    sensor_flush(&s);

    ASSERT_TRUE(buffer_is_empty(s.buf),      "buffer empty after flush");
    ASSERT_EQ(s.stats.sample_count, 0,       "sample_count reset");

    float mean;
    ASSERT_FALSE(sensor_get_mean(&s, &mean), "no mean with 0 samples");

    sensor_destroy(&s);
}

static void test_name_truncation(void)
{
    test_header("long sensor name — truncated to SENSOR_NAME_MAX");
    sensor_t s;
    const char *long_name = "VeryLongSensorNameThatExceedsTheMaximumAllowedLength_XXXXX";
    sensor_init(&s, 5, long_name, 4);

    ASSERT_EQ(strlen(s.name), (size_t)(SENSOR_NAME_MAX - 1), "name length capped");
    ASSERT_EQ(s.name[SENSOR_NAME_MAX - 1], '\0',              "always null-terminated");

    sensor_destroy(&s);
}

static void test_peek_sensor(void)
{
    test_header("sensor_peek — non-destructive");
    sensor_t s;
    sensor_init(&s, 6, "Light", 4);
    sensor_log(&s, 500.0f, 42);

    sensor_reading_t out1, out2;
    ASSERT_TRUE(sensor_peek(&s, &out1),       "peek returns true");
    ASSERT_TRUE(sensor_peek(&s, &out2),       "second peek also true");
    ASSERT_EQ(out1.timestamp, out2.timestamp, "both peeks same data");
    ASSERT_EQ(buffer_count(s.buf), 1,         "count still 1 after two peeks");

    sensor_destroy(&s);
}

/* ============================================================================
 * MAIN
 * ========================================================================== */

int main(void)
{
    printf("==============================\n");
    printf("  Sensor Layer Test Suite\n");
    printf("==============================\n");

    test_init_destroy();
    test_init_bad_args();
    test_log_and_read();
    test_stats();
    test_pause_resume();
    test_flush();
    test_name_truncation();
    test_peek_sensor();

    printf("\n==============================\n");
    printf("  Results: %d passed, %d failed\n", g_pass, g_fail);
    printf("==============================\n");

    return (g_fail == 0) ? 0 : 1;
}
