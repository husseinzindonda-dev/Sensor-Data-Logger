# Sensor Data Logger

A circular buffer implementation for logging sensor readings, written in C.
Built as a hands-on learning project to practice pointer arithmetic, manual memory management, and embedded-style systems programming.

---

## What I learned building this

- How pointers actually work — arithmetic, dereferencing, and wrap-around logic
- Manual heap allocation with `malloc` / `free` and why defensive cleanup matters
- Why a circular (ring) buffer is useful in memory-constrained environments
- How to layer abstractions — a generic buffer underneath a domain-specific sensor API
- Writing a test suite from scratch without any external framework

---

## How the ring buffer works

A ring buffer (also called a circular buffer) is a fixed-size data structure that treats its memory as if it wraps around in a circle. Instead of shifting data when something is consumed, two pointers — `head` and `tail` — move through the array independently.

```
Capacity = 5

Initial state:
 [ ][ ][ ][ ][ ]
  ^
  head & tail (both at index 0)

After writing A, B, C:
 [A][B][C][ ][ ]
  ^        ^
 tail     head   (tail = oldest, head = next free slot)

After reading A:
 [A][B][C][ ][ ]
     ^    ^
    tail  head   (A is gone logically; tail advanced)

After writing D, E, F (wraps around):
 [F][B][C][D][E]
  ^  ^
 head tail       (head wrapped to index 0)
```

Key properties:

- **Write** copies data to `head`, then advances `head`
- **Read** copies data from `tail`, then advances `tail`
- When either pointer reaches the end of the array, it wraps back to index 0
- The buffer is **full** when `count == capacity` — write is rejected
- The buffer is **empty** when `count == 0` — read is rejected

This avoids any memory allocation during normal operation, making it suitable for embedded systems where `malloc` is expensive or forbidden.

---

## Project structure

```
sensor-data-logger/
├── src/
│   ├── buffer.h        # Ring buffer type definitions and API declarations
│   ├── buffer.c        # Ring buffer implementation
│   ├── sensors.h       # Sensor layer type definitions and API declarations
│   ├── sensors.c       # Sensor layer implementation
│   └── main.c          # Integration demo
├── tests/
│   ├── test_buffer.c   # Unit tests for the ring buffer (45 assertions)
│   └── test_sensor.c   # Unit tests for the sensor layer (39 assertions)
├── build/              # Compiled binaries (generated, not committed)
├── .vscode/
│   ├── launch.json     # Debug configurations (F5 support for each binary)
│   ├── settings.json   # IntelliSense paths and editor preferences
│   └── tasks.json      # Pre-launch build tasks
├── build.ps1           # Windows build + run script
├── Makefile            # Cross-platform build targets
└── README.md
```

---

## Building and running

### Windows (PowerShell)

```powershell
# Build everything and run all tests + main app
.\build.ps1

# Build and run tests only
.\build.ps1 -TestOnly

# Build without running
.\build.ps1 -NoRun
```

### Any platform (Make)

```bash
make          # Build everything
make test     # Build and run tests only
make run      # Build and run main app
make clean    # Remove build directory
```

### Manual (gcc)

```bash
# Main application
gcc src/buffer.c src/sensors.c src/main.c -o build/sensor_logger.exe -Wall -Wextra -Werror -std=c11 -g

# Buffer tests
gcc src/buffer.c tests/test_buffer.c -o build/test_buffer.exe -Wall -Wextra -Werror -std=c11 -g

# Sensor tests
gcc src/buffer.c src/sensors.c tests/test_sensor.c -o build/test_sensor.exe -Wall -Wextra -Werror -std=c11 -g -lm
```

### Debugging in VS Code

Open the Run & Debug panel (`Ctrl+Shift+D`), select a configuration, and press `F5`. The project will compile automatically before launching with the debugger attached.

---

## API reference

### Buffer layer (`buffer.h`)

The core ring buffer. Works with any `sensor_reading_t` data — knows nothing about sensors specifically.

| Function                     | Description                                                                 |
| ---------------------------- | --------------------------------------------------------------------------- |
| `buffer_create(capacity)`    | Allocate a new ring buffer. Returns `NULL` on failure or if `capacity == 0` |
| `buffer_destroy(buf)`        | Free all memory. Safe to call with `NULL`                                   |
| `buffer_write(buf, reading)` | Write one entry. Returns `false` if full                                    |
| `buffer_read(buf, output)`   | Read and remove the oldest entry. Returns `false` if empty                  |
| `buffer_peek(buf, output)`   | Read the oldest entry without removing it                                   |
| `buffer_is_empty(buf)`       | Returns `true` if no entries are stored                                     |
| `buffer_is_full(buf)`        | Returns `true` if no free slots remain                                      |
| `buffer_count(buf)`          | Number of entries currently stored                                          |
| `buffer_free_slots(buf)`     | Number of available slots                                                   |
| `buffer_clear(buf)`          | Reset to empty state without deallocating memory                            |
| `buffer_overflow_count(buf)` | Total number of rejected writes since creation                              |
| `buffer_get_status(buf)`     | Snapshot of status flags (`is_full`, `is_empty`, `overflow_occurred`)       |
| `buffer_print_debug(buf)`    | Print internal state to stdout — for debugging only                         |

---

### Sensor layer (`sensors.h`)

Sits on top of the buffer. Each `sensor_t` owns its own ring buffer and tracks running statistics automatically.

| Function                                  | Description                                             |
| ----------------------------------------- | ------------------------------------------------------- |
| `sensor_init(sensor, id, name, capacity)` | Initialise a sensor and allocate its buffer             |
| `sensor_destroy(sensor)`                  | Free the sensor's buffer and reset state                |
| `sensor_log(sensor, value, timestamp)`    | Record a new reading (stamps `sensor_id` automatically) |
| `sensor_read(sensor, output)`             | Read and remove the oldest reading                      |
| `sensor_peek(sensor, output)`             | Read the oldest reading without removing it             |
| `sensor_get_stats(sensor, stats)`         | Get a snapshot of min, max, sum, and sample count       |
| `sensor_get_mean(sensor, mean)`           | Compute mean from running stats                         |
| `sensor_pause(sensor)`                    | Stop accepting new readings (buffer preserved)          |
| `sensor_resume(sensor)`                   | Resume after a pause                                    |
| `sensor_flush(sensor)`                    | Clear buffer and reset statistics                       |
| `sensor_print_info(sensor)`               | Print sensor state and statistics to stdout             |

**Sensor states:**

```
UNINIT ──► ACTIVE ◄──► PAUSED
                └──► ERROR
```

---

## Roadmap

Ideas for future development:

- [ ] **Multi-sensor manager** — an array of `sensor_t` instances addressable by ID, with a unified `manager_log(id, value)` interface
- [ ] **Batch read / write** — read or write N entries in one call to reduce per-entry overhead
- [ ] **Timestamps from a real clock** — replace the manual `timestamp` parameter with a callback so the sensor layer can stamp automatically
- [ ] **CSV export** — drain a sensor buffer to a `.csv` file for analysis in spreadsheet tools
- [ ] **Moving average** — add a rolling window average alongside the existing running mean
- [ ] **Persistent storage** — serialize buffer contents to a binary file and reload on startup

---

## Requirements

- GCC (mingw-w64 on Windows, or any GCC on Linux/macOS)
- C11 standard (`-std=c11`)
- GNU Make (optional, for `make` commands)
- VS Code with the C/C++ extension (optional, for debugging)
