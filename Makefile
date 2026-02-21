# Makefile - Sensor Data Logger
# Usage:
#   make          — build everything
#   make test     — build and run tests
#   make run      — build and run main app
#   make clean    — remove build artifacts

CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c11 -g
BUILDDIR = build

# Source files
MAIN_SRC   = src/buffer.c src/sensors.c src/main.c
TEST_BUF   = src/buffer.c tests/test_buffer.c
TEST_SEN   = src/buffer.c src/sensors.c tests/test_sensor.c

# Output binaries
APP          = $(BUILDDIR)/sensor_logger
TEST_BUF_EXE = $(BUILDDIR)/test_buffer
TEST_SEN_EXE = $(BUILDDIR)/test_sensor

# On Windows (mingw) executables need .exe
ifeq ($(OS),Windows_NT)
    APP          := $(APP).exe
    TEST_BUF_EXE := $(TEST_BUF_EXE).exe
    TEST_SEN_EXE := $(TEST_SEN_EXE).exe
endif

# =============================================================================

.PHONY: all test run clean

all: $(BUILDDIR) $(APP) $(TEST_BUF_EXE) $(TEST_SEN_EXE)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(APP): $(MAIN_SRC) | $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@

$(TEST_BUF_EXE): $(TEST_BUF) | $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@

$(TEST_SEN_EXE): $(TEST_SEN) | $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@ -lm

test: $(TEST_BUF_EXE) $(TEST_SEN_EXE)
	@echo "\n--- Ring Buffer Tests ---"
	./$(TEST_BUF_EXE)
	@echo "\n--- Sensor Layer Tests ---"
	./$(TEST_SEN_EXE)

run: $(APP)
	./$(APP)

clean:
	rm -rf $(BUILDDIR)