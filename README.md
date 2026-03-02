# Predictive Maintenance Monitor

A low-cost industrial sensor monitoring system built in C, deployed to Arduino hardware, with a live Python dashboard. Detects early warning signs of equipment failure — overheating, abnormal vibration, and excessive current draw — before a breakdown occurs.

---

## Demo

> **Tip for GitHub:** Replace this section with a GIF of your dashboard running.
> Record with Xbox Game Bar (`Win + G`) or ShareX, then drag the file into your README on GitHub.

```
[Insert dashboard screenshot or GIF here]
```

---

## The problem it solves

Small workshops and factories run motors, compressors, and pumps that fail unexpectedly. Large companies solve this with expensive industrial IoT systems costing thousands. This project provides the same early-warning capability for under $70 in hardware.

A healthy motor runs cool, vibrates smoothly, and draws consistent current. A failing motor runs hot, vibrates irregularly, and draws more power. This system detects those deviations and raises an alert before failure occurs.

---

## Architecture

```
Physical Layer          Embedded Layer (C)        PC Layer (Python)
──────────────          ──────────────────        ─────────────────
DHT11 ──────────────►   Arduino reads sensors     dashboard.py plots
MPU6050 ────────────►   every 2 seconds      ──►  live charts with
ACS712 (coming) ────►   stores in ring buffer      colour-coded alerts
                        checks thresholds          refreshes every 5s
                        sends CSV over USB
```

### Software layers

```
sensor_manager.c   ←  coordinates all sensors, fires threshold alerts
sensors.c          ←  per-sensor logic, running stats, state machine
buffer.c           ←  circular ring buffer, pointer arithmetic, memory
logger.c           ←  CSV file export for Python dashboard
```

Each layer knows nothing about the one above it. The buffer does not know what a sensor is. The sensor does not know about the manager. This makes each layer independently testable and reusable.

---

## Project Structure

```
Sensor Data Logger/
├── src/
│   ├── buffer.h / buffer.c           Ring buffer implementation
│   ├── sensors.h / sensors.c         Sensor abstraction layer
│   ├── sensor_manager.h / .c         Multi-sensor coordinator + alerts
│   ├── logger.h / logger.c           CSV file logger
│   └── main.c                        PC simulation demo
├── tests/
│   ├── test_buffer.c                 45 assertions
│   ├── test_sensor.c                 39 assertions
│   └── test_manager.c                43 assertions
├── arduino/
│   └── predictive_monitor/
│       └── predictive_monitor.ino    Arduino sketch
├── dashboard/
│   └── dashboard.py                  Live Python dashboard
├── data/                             CSV output (generated at runtime)
├── build/                            Compiled binaries (generated)
├── build.ps1                         Windows build script
└── Makefile                          Cross-platform build
```

---

## Hardware

| Part                      | Purpose                      |
| ------------------------- | ---------------------------- |
| Arduino Uno               | Microcontroller              |
| DHT11                     | Temperature + humidity       |
| MPU6050 (GY-521)          | Vibration / accelerometer    |
| ACS712 5A                 | Current draw _(coming soon)_ |
| Breadboard + jumper wires | Wiring                       |

### Wiring

```
DHT11                    MPU6050 (GY-521)
VCC  → Arduino 5V        VCC → Arduino 3.3V   (NOT 5V)
GND  → Arduino GND       GND → Arduino GND
DATA → Arduino Pin 2     SDA → Arduino A4
                         SCL → Arduino A5
```

---

## Getting Started

### Requirements

- Windows with GCC / MinGW for the C code
- Python 3.x for the dashboard
- Arduino IDE for flashing the sketch

### 1. Build the C project

```powershell
.\build.ps1
```

This compiles all targets, runs 127 unit tests, and runs the PC simulation:

```
Tests (buffer)  -> build/test_buffer.exe    45 passed, 0 failed
Tests (sensor)  -> build/test_sensor.exe    39 passed, 0 failed
Tests (manager) -> build/test_manager.exe   43 passed, 0 failed
Main app        -> build/sensor_logger.exe
```

### 2. Install Python dependencies

```powershell
pip install matplotlib pandas pyserial
```

### 3. Flash the Arduino

Open `arduino/predictive_monitor/predictive_monitor.ino` in Arduino IDE, then install the following libraries via Tools → Manage Libraries:

- DHT sensor library (Adafruit)
- Adafruit Unified Sensor (Adafruit)
- MPU6050 (Electronic Cats)

Select Tools → Board → Arduino Uno, select your COM port, then click Upload.

### 4. Launch the live dashboard

Close Arduino IDE Serial Monitor first, then run:

```powershell
python dashboard/dashboard.py --serial COM5
```

Replace `COM5` with your actual port shown in Arduino IDE under Tools → Port.

### PC simulation only (no Arduino needed)

```powershell
.\build.ps1
python dashboard/dashboard.py
```

The C program writes `data/sensor_log.csv` and the dashboard reads it.

---

## Dashboard

The dashboard shows one chart per sensor, refreshing every 5 seconds:

- **Blue** — normal reading
- **Orange** — warning threshold crossed
- **Red** — critical threshold crossed
- **Dashed lines** — threshold markers drawn on each chart

### Alert thresholds

| Sensor             | Warning        | Critical |
| ------------------ | -------------- | -------- |
| Temperature        | > 70 C         | > 85 C   |
| Humidity           | < 20% or > 80% | —        |
| Vibration          | > 0.5g         | > 1.0g   |
| Current _(coming)_ | > 8A           | > 10A    |

---

## How the Ring Buffer Works

The core data structure is a circular buffer — a fixed-size array where a `head` pointer tracks the next write position and a `tail` pointer tracks the next read position. When either pointer reaches the end of the array it wraps back to the start.

```
Initial state (capacity = 4):
┌────┬────┬────┬────┐
│    │    │    │    │
└────┴────┴────┴────┘
 ▲
head/tail (both at 0, buffer empty)

After 3 writes:
┌────┬────┬────┬────┐
│ A  │ B  │ C  │    │
└────┴────┴────┴────┘
 ▲              ▲
tail           head

After 1 read (A consumed) + 2 more writes (wraps around):
┌────┬────┬────┬────┐
│ E  │ B  │ C  │ D  │
└────┴────┴────┴────┘
      ▲    ▲
     tail head (wrapped)
```

If a write is attempted on a full buffer it is rejected and an overflow counter is incremented. No data is corrupted and the program never crashes.

---

## API Reference

### Buffer layer

| Function                     | Description                               |
| ---------------------------- | ----------------------------------------- |
| `buffer_create(capacity)`    | Allocate a new ring buffer                |
| `buffer_write(buf, reading)` | Write a reading, returns false if full    |
| `buffer_read(buf, output)`   | Read oldest entry, returns false if empty |
| `buffer_peek(buf, output)`   | Read without consuming                    |
| `buffer_clear(buf)`          | Reset to empty                            |
| `buffer_destroy(buf)`        | Free all memory                           |

### Sensor layer

| Function                               | Description                  |
| -------------------------------------- | ---------------------------- |
| `sensor_init(s, id, name, capacity)`   | Initialise a sensor          |
| `sensor_log(s, value, timestamp)`      | Log a reading                |
| `sensor_read(s, output)`               | Read oldest entry            |
| `sensor_get_stats(s, stats)`           | Get min, max, mean, count    |
| `sensor_pause(s)` / `sensor_resume(s)` | Pause and resume logging     |
| `sensor_flush(s)`                      | Clear buffer and reset stats |

### Sensor Manager

| Function                                    | Description                    |
| ------------------------------------------- | ------------------------------ |
| `manager_create(capacity)`                  | Create manager for N sensors   |
| `manager_register(m, id, name, buf_size)`   | Register a sensor              |
| `manager_set_thresholds(m, id, thresholds)` | Configure alert thresholds     |
| `manager_log(m, id, value, timestamp)`      | Log and check thresholds       |
| `manager_print_all(m)`                      | Print all sensor summaries     |
| `manager_print_stats(m)`                    | Print manager-level statistics |

---

## Test Suite

```
127 assertions across 3 test files, 0 failures
```

Run tests only (no main app):

```powershell
.\build.ps1 -TestOnly
```

Build without running:

```powershell
.\build.ps1 -NoRun
```

---

## What I Learned

- **Pointer arithmetic** — implementing wrap-around logic in the ring buffer using raw pointer manipulation rather than array indices
- **Manual memory management** — two-stage malloc with defensive cleanup on failure, zero memory leaks
- **Layered architecture** — separating buffer, sensor, and manager concerns so each layer is independently testable
- **Embedded C patterns** — static memory allocation and fixed-size buffers used on Arduino where malloc is avoided
- **I2C communication** — reading the MPU6050 accelerometer over the I2C protocol using SDA and SCL lines
- **Serial communication** — streaming structured CSV data from Arduino to PC over USB at 9600 baud
- **Cross-language integration** — Python reading live serial data and rendering it with matplotlib animations

---

## Roadmap

- [ ] Add ACS712 current sensor (hardware arriving)
- [ ] Moving average filter to smooth noisy vibration readings
- [ ] Persistent alert log saved to SD card on Arduino
- [ ] Anomaly detection using statistical baseline (standard deviation threshold)
- [ ] Multi-machine support — monitor several motors simultaneously

---

## Built With

- C11 (GCC / MinGW)
- Arduino (C++ / AVR)
- Python 3 — matplotlib, pandas, pyserial
- VS Code + Arduino IDE
