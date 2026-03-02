/*
 * predictive_monitor.ino
 * ======================
 * Predictive Maintenance Monitor - Phase 3 (Arduino)
 *
 * Reads DHT11 (temperature/humidity) and MPU6050 (vibration)
 * every 2 seconds and sends CSV data over serial (USB) to PC.
 *
 * Wiring:
 *   DHT11  VCC  -> 5V
 *   DHT11  GND  -> GND
 *   DHT11  DATA -> Pin 2
 *
 *   MPU6050 VCC -> 3.3V  (NOT 5V)
 *   MPU6050 GND -> GND
 *   MPU6050 SDA -> A4
 *   MPU6050 SCL -> A5
 *
 * Libraries needed (Tools -> Manage Libraries):
 *   - DHT sensor library      (Adafruit)
 *   - Adafruit Unified Sensor (Adafruit)
 *   - MPU6050                 (Electronic Cats)
 */

#include <DHT.h>
#include <MPU6050.h>
#include <Wire.h>

/* ============================================================
 * CONFIGURATION
 * ============================================================ */

#define DHT_PIN 2
#define DHT_TYPE DHT11
#define READ_INTERVAL_MS 2000

#define SENSOR_TEMP 0
#define SENSOR_HUMIDITY 1
#define SENSOR_VIBRATION 2

/* Thresholds */
#define TEMP_WARN_HIGH 70.0f
#define TEMP_CRITICAL_HIGH 85.0f
#define HUMID_WARN_LOW 20.0f
#define HUMID_WARN_HIGH 80.0f
#define VIB_WARN_HIGH 0.5f
#define VIB_CRITICAL_HIGH 1.0f

/* ============================================================
 * RING BUFFER
 * Same circular buffer concept as buffer.c on your PC.
 * Uses static arrays instead of malloc (Arduino has 2KB RAM).
 * ============================================================ */

#define BUFFER_SIZE 8

typedef struct
{
    float values[BUFFER_SIZE];
    uint32_t timestamps[BUFFER_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} ring_buffer_t;

static ring_buffer_t buf_temp;
static ring_buffer_t buf_humid;
static ring_buffer_t buf_vib;

void buffer_init(ring_buffer_t *buf)
{
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
    memset(buf->values, 0, sizeof(buf->values));
    memset(buf->timestamps, 0, sizeof(buf->timestamps));
}

bool buffer_write(ring_buffer_t *buf, float value, uint32_t timestamp)
{
    if (buf->count == BUFFER_SIZE)
        return false;
    buf->values[buf->head] = value;
    buf->timestamps[buf->head] = timestamp;
    buf->head = (buf->head + 1) % BUFFER_SIZE;
    buf->count++;
    return true;
}

/* ============================================================
 * SENSOR OBJECTS
 * ============================================================ */

DHT dht(DHT_PIN, DHT_TYPE);
MPU6050 mpu;

/* ============================================================
 * THRESHOLD CHECK
 * ============================================================ */

const char *check_threshold(uint8_t sensor_id, float value)
{
    switch (sensor_id)
    {
    case SENSOR_TEMP:
        if (value >= TEMP_CRITICAL_HIGH)
            return "CRITICAL";
        if (value >= TEMP_WARN_HIGH)
            return "WARNING";
        break;
    case SENSOR_HUMIDITY:
        if (value >= HUMID_WARN_HIGH)
            return "WARNING";
        if (value <= HUMID_WARN_LOW)
            return "WARNING";
        break;
    case SENSOR_VIBRATION:
        if (value >= VIB_CRITICAL_HIGH)
            return "CRITICAL";
        if (value >= VIB_WARN_HIGH)
            return "WARNING";
        break;
    }
    return "NONE";
}

/* ============================================================
 * SEND CSV ROW OVER SERIAL
 * ============================================================ */

void send_csv_row(uint32_t timestamp, uint8_t sensor_id,
                  const char *sensor_name, float value,
                  const char *alert_level)
{
    Serial.print(timestamp);
    Serial.print(",");
    Serial.print(sensor_id);
    Serial.print(",");
    Serial.print(sensor_name);
    Serial.print(",");
    Serial.print(value, 4);
    Serial.print(",");
    Serial.println(alert_level);
}

/* ============================================================
 * COMPUTE VIBRATION MAGNITUDE
 * ============================================================ */

float compute_vibration(int16_t ax, int16_t ay, int16_t az)
{
    float gx = ax / 16384.0f;
    float gy = ay / 16384.0f;
    float gz = az / 16384.0f;

    float magnitude = sqrt(gx * gx + gy * gy + gz * gz);
    float vibration = magnitude - 1.0f;
    if (vibration < 0.0f)
        vibration = -vibration;

    return vibration;
}

/* ============================================================
 * SETUP
 * ============================================================ */

void setup()
{
    Serial.begin(9600);
    while (!Serial)
    {
        delay(10);
    }

    Serial.println("timestamp,sensor_id,sensor_name,value,alert_level");
    Serial.println("# Predictive Maintenance Monitor ready");
    Serial.println("# Sensors: DHT11 (temp/humidity) + MPU6050 (vibration)");

    dht.begin();

    Wire.begin();
    mpu.initialize();

    buffer_init(&buf_temp);
    buffer_init(&buf_humid);
    buffer_init(&buf_vib);
}

/* ============================================================
 * LOOP
 * ============================================================ */

void loop()
{
    uint32_t timestamp = millis();

    /* --- DHT11 --- */
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (!isnan(temperature) && !isnan(humidity))
    {
        buffer_write(&buf_temp, temperature, timestamp);
        buffer_write(&buf_humid, humidity, timestamp);

        send_csv_row(timestamp, SENSOR_TEMP,
                     "Temperature (C)", temperature,
                     check_threshold(SENSOR_TEMP, temperature));

        send_csv_row(timestamp, SENSOR_HUMIDITY,
                     "Humidity (%)", humidity,
                     check_threshold(SENSOR_HUMIDITY, humidity));
    }
    else
    {
        Serial.println("# WARNING: DHT11 read failed - check wiring");
    }

    /* --- MPU6050 --- */
    int16_t ax, ay, az, gx, gy, gz;
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    float vibration = compute_vibration(ax, ay, az);
    buffer_write(&buf_vib, vibration, timestamp);

    send_csv_row(timestamp, SENSOR_VIBRATION,
                 "Vibration (g)", vibration,
                 check_threshold(SENSOR_VIBRATION, vibration));

    delay(READ_INTERVAL_MS);
}
