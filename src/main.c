#include "buffer.h"
#include <stdio.h>

int main(void)
{
    printf("=== Sensor Logger - Buffer Test ===\n");
    
    // Test 1: Create and destroy
    ring_buffer_t *buf = buffer_create(5);
    buffer_print_debug(buf);
    
    // Test 2: Write one entry
    sensor_reading_t reading = {1000, 0, 25.5};
    buffer_write(buf, &reading);
    buffer_print_debug(buf);
    
    // Test 3: Read it back
    sensor_reading_t output;
    buffer_read(buf, &output);
    printf("Read: time=%u, sensor=%d, value=%.2f\n",
           output.timestamp, output.sensor_id, output.value);
    
    buffer_destroy(buf);
    return 0;
}