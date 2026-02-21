/**
 * @file test_buffer.c
 * @brief Unit tests for the ring buffer
 *
 * Build:  gcc -Wall -Wextra -Werror -std=c11 -g src/buffer.c tests/test_buffer.c -o build/test_buffer.exe
 * Run:    ./build/test_buffer.exe
 */

#include "../src/buffer.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

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

#define ASSERT_EQ(a, b, msg)   ASSERT((a) == (b), msg)
#define ASSERT_TRUE(x, msg)    ASSERT((x),         msg)
#define ASSERT_FALSE(x, msg)   ASSERT(!(x),        msg)

static void test_header(const char *name)
{
    printf("\n[TEST] %s\n", name);
}

static sensor_reading_t make_reading(uint32_t ts, uint8_t id, float val)
{
    sensor_reading_t r;
    r.timestamp = ts;
    r.sensor_id = id;
    r.value     = val;
    return r;
}

/* ============================================================================
 * TESTS
 * ========================================================================== */

static void test_create_valid(void)
{
    test_header("buffer_create — valid capacity");
    ring_buffer_t *buf = buffer_create(4);

    ASSERT_TRUE(buf != NULL,               "returns non-NULL");
    ASSERT_TRUE(buffer_is_empty(buf),      "starts empty");
    ASSERT_FALSE(buffer_is_full(buf),      "not full at start");
    ASSERT_EQ(buffer_count(buf),      0,   "count == 0");
    ASSERT_EQ(buffer_free_slots(buf), 4,   "free_slots == capacity");

    buffer_destroy(buf);
}

static void test_create_zero(void)
{
    test_header("buffer_create — zero capacity (should fail)");
    ring_buffer_t *buf = buffer_create(0);
    ASSERT_FALSE(buf, "returns NULL for capacity 0");
}

static void test_destroy_null(void)
{
    test_header("buffer_destroy — NULL is safe");
    buffer_destroy(NULL);
    ASSERT_TRUE(1, "no crash on destroy(NULL)");
}

static void test_write_read_single(void)
{
    test_header("buffer_write / buffer_read — single entry");
    ring_buffer_t *buf = buffer_create(4);
    sensor_reading_t r = make_reading(1000, 1, 42.0f);
    sensor_reading_t out;

    ASSERT_TRUE(buffer_write(buf, &r),        "write returns true");
    ASSERT_EQ(buffer_count(buf), 1,           "count == 1 after write");
    ASSERT_TRUE(buffer_read(buf, &out),       "read returns true");
    ASSERT_EQ(out.timestamp, 1000,            "timestamp preserved");
    ASSERT_EQ(out.sensor_id, 1,               "sensor_id preserved");
    ASSERT_TRUE(out.value == 42.0f,           "value preserved");
    ASSERT_EQ(buffer_count(buf), 0,           "count == 0 after read");

    buffer_destroy(buf);
}

static void test_write_null_args(void)
{
    test_header("buffer_write — NULL arguments");
    ring_buffer_t *buf = buffer_create(4);
    sensor_reading_t r = make_reading(0, 0, 0.0f);

    ASSERT_FALSE(buffer_write(NULL, &r),  "NULL buf -> false");
    ASSERT_FALSE(buffer_write(buf, NULL), "NULL reading -> false");

    buffer_destroy(buf);
}

static void test_fifo_order(void)
{
    test_header("FIFO ordering — reads match write order");
    ring_buffer_t *buf = buffer_create(4);

    for (int i = 0; i < 4; i++) {
        sensor_reading_t r = make_reading((uint32_t)(1000 + i), (uint8_t)i, (float)i);
        buffer_write(buf, &r);
    }

    for (int i = 0; i < 4; i++) {
        sensor_reading_t out;
        buffer_read(buf, &out);
        char msg[64];
        snprintf(msg, sizeof(msg), "entry %d correct timestamp", i);
        ASSERT_EQ(out.timestamp, (uint32_t)(1000 + i), msg);
    }

    buffer_destroy(buf);
}

static void test_overflow(void)
{
    test_header("overflow — write to full buffer");
    ring_buffer_t *buf = buffer_create(3);
    sensor_reading_t r = make_reading(0, 0, 0.0f);

    buffer_write(buf, &r);
    buffer_write(buf, &r);
    buffer_write(buf, &r);

    ASSERT_TRUE(buffer_is_full(buf),            "buffer is full");
    ASSERT_FALSE(buffer_write(buf, &r),         "write to full returns false");
    ASSERT_EQ(buffer_overflow_count(buf), 1,    "overflow_count == 1");
    ASSERT_EQ(buffer_count(buf), 3,             "count unchanged");

    buffer_write(buf, &r);
    ASSERT_EQ(buffer_overflow_count(buf), 2,    "overflow_count increments");

    buffer_destroy(buf);
}

static void test_underflow(void)
{
    test_header("underflow — read from empty buffer");
    ring_buffer_t *buf = buffer_create(4);
    sensor_reading_t out;

    ASSERT_FALSE(buffer_read(buf, &out),   "read from empty returns false");
    ASSERT_TRUE(buffer_is_empty(buf),      "still empty after failed read");

    buffer_destroy(buf);
}

static void test_wraparound(void)
{
    test_header("wrap-around — pointers cycle through capacity");
    ring_buffer_t *buf = buffer_create(4);
    sensor_reading_t out;

    for (int i = 0; i < 4; i++) {
        sensor_reading_t r = make_reading((uint32_t)i, 0, (float)i);
        buffer_write(buf, &r);
    }

    buffer_read(buf, &out);
    buffer_read(buf, &out);
    sensor_reading_t r1 = make_reading(100, 0, 100.0f);
    sensor_reading_t r2 = make_reading(101, 0, 101.0f);
    buffer_write(buf, &r1);
    buffer_write(buf, &r2);

    ASSERT_EQ(buffer_count(buf), 4, "count still 4 after wrap");

    sensor_reading_t readings[4];
    for (int i = 0; i < 4; i++)
        buffer_read(buf, &readings[i]);

    ASSERT_EQ(readings[0].timestamp,   2, "slot 0 ts == 2");
    ASSERT_EQ(readings[1].timestamp,   3, "slot 1 ts == 3");
    ASSERT_EQ(readings[2].timestamp, 100, "slot 2 ts == 100");
    ASSERT_EQ(readings[3].timestamp, 101, "slot 3 ts == 101");

    buffer_destroy(buf);
}

static void test_peek(void)
{
    test_header("buffer_peek — does not consume entry");
    ring_buffer_t *buf = buffer_create(4);
    sensor_reading_t r = make_reading(999, 2, 3.14f);
    sensor_reading_t out, out2;

    buffer_write(buf, &r);
    ASSERT_TRUE(buffer_peek(buf, &out),           "peek returns true");
    ASSERT_EQ(out.timestamp, 999,                  "correct timestamp");
    ASSERT_EQ(buffer_count(buf), 1,               "count unchanged after peek");

    buffer_peek(buf, &out2);
    ASSERT_EQ(out2.timestamp, out.timestamp,       "second peek identical");

    buffer_destroy(buf);
}

static void test_clear(void)
{
    test_header("buffer_clear — resets to empty");
    ring_buffer_t *buf = buffer_create(4);
    sensor_reading_t r = make_reading(0, 0, 0.0f);

    buffer_write(buf, &r);
    buffer_write(buf, &r);
    buffer_clear(buf);

    ASSERT_TRUE(buffer_is_empty(buf),       "empty after clear");
    ASSERT_EQ(buffer_count(buf), 0,        "count == 0 after clear");
    ASSERT_EQ(buffer_free_slots(buf), 4,   "all slots free after clear");
    ASSERT_TRUE(buffer_write(buf, &r),     "write works after clear");

    buffer_destroy(buf);
}

static void test_capacity_one(void)
{
    test_header("capacity == 1 — edge case");
    ring_buffer_t *buf = buffer_create(1);
    sensor_reading_t r = make_reading(1, 0, 1.0f);
    sensor_reading_t out;

    ASSERT_TRUE(buffer_write(buf, &r),   "write to size-1 buffer");
    ASSERT_TRUE(buffer_is_full(buf),     "full after one write");
    ASSERT_FALSE(buffer_write(buf, &r),  "second write rejected");
    ASSERT_TRUE(buffer_read(buf, &out),  "read succeeds");
    ASSERT_TRUE(buffer_is_empty(buf),    "empty after read");

    buffer_destroy(buf);
}

/* ============================================================================
 * MAIN
 * ========================================================================== */

int main(void)
{
    printf("==============================\n");
    printf("  Ring Buffer Test Suite\n");
    printf("==============================\n");

    test_create_valid();
    test_create_zero();
    test_destroy_null();
    test_write_read_single();
    test_write_null_args();
    test_fifo_order();
    test_overflow();
    test_underflow();
    test_wraparound();
    test_peek();
    test_clear();
    test_capacity_one();

    printf("\n==============================\n");
    printf("  Results: %d passed, %d failed\n", g_pass, g_fail);
    printf("==============================\n");

    return (g_fail == 0) ? 0 : 1;
}
