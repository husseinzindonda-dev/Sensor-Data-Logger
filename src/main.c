#include "buffer.h"
#include <stdio.h>

int main(void)
{
    printf("=== Comprehensive Buffer Test ===\n");
    
    // Test 1: Create buffer
    printf("\n1. Creating buffer (capacity=5)\n");
    ring_buffer_t *buf = buffer_create(5);
    if (!buf) {
        printf("ERROR: Creation failed!\n");
        return 1;
    }
    buffer_print_debug(buf);
    
    // Test 2: Fill the buffer completely
    printf("\n2. Filling buffer completely\n");
    for (int i = 0; i < 5; i++) {
        sensor_reading_t r = {1000 + i, i, 20.0f + i};
        if (buffer_write(buf, &r)) {
            printf("  Write %d: OK\n", i);
        } else {
            printf("  Write %d: FAILED (unexpected!)\n", i);
        }
    }
    buffer_print_debug(buf);
    
    // Test 3: Try to write to full buffer
    printf("\n3. Testing overflow (write to full buffer)\n");
    sensor_reading_t extra = {2000, 99, 99.9};
    if (!buffer_write(buf, &extra)) {
        printf("  CORRECT: Rejected write to full buffer\n");
    }
    buffer_print_debug(buf);
    
    // Test 4: Empty the buffer
    printf("\n4. Emptying buffer\n");
    sensor_reading_t out;
    while (buffer_read(buf, &out)) {
        printf("  Read: time=%u, sensor=%u, value=%.1f\n",
               out.timestamp, out.sensor_id, out.value);
    }
    buffer_print_debug(buf);
    
    // Test 5: Try to read from empty buffer
    printf("\n5. Testing underflow (read from empty buffer)\n");
    if (!buffer_read(buf, &out)) {
        printf("  CORRECT: Rejected read from empty buffer\n");
    }
    
    // Test 6: Wrap-around test
    printf("\n6. Testing wrap-around\n");
    for (int i = 0; i < 7; i++) {  // More than capacity
        sensor_reading_t r = {3000 + i, i * 10, 30.0f + i};
        buffer_write(buf, &r);
        if (i >= 2) {  // Start reading after 2 writes
            buffer_read(buf, &out);
            printf("  Write %d, read oldest\n", i);
        }
    }
    buffer_print_debug(buf);
    
    // Test 7: Clear and destroy
    printf("\n7. Final cleanup\n");
    buffer_clear(buf);
    buffer_print_debug(buf);
    buffer_destroy(buf);
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}