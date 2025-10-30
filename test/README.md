# LopCore Testing Guide

This directory contains the complete testing infrastructure for the LopCore middleware framework.

## Location

Tests are located inside the LopCore component:

```
components/lopcore/test/
```

This structure keeps tests with the component they're testing, making it easier to:

-   Package tests with the component
-   Run tests as part of component development
-   Maintain tests alongside source code
-   Publish tests with the component to the registry

## Directory Structure

```
components/lopcore/test/
├── CMakeLists.txt           # Google Test configuration
├── README.md                # This file
├── unit/                    # Unit tests (Google Test)
│   ├── test_dummy.cpp       # Dummy test to verify setup
│   ├── test_logger.cpp      # Logger tests
│   ├── test_file_sink.cpp   # File sink tests
│   ├── test_storage_factory.cpp  # Storage tests
│   ├── mqtt/                # MQTT component tests
│   │   ├── test_mqtt_types.cpp
│   │   ├── test_mqtt_budget.cpp
│   │   ├── test_mqtt_operations.cpp
│   │   └── ...
│   └── tls/                 # TLS component tests
│       └── test_mock_tls_transport.cpp
└── mocks/                   # Mock headers for host testing
    ├── esp32/
    │   ├── esp_log.h        # Mock ESP_LOG
    │   ├── esp_err.h        # Mock error codes
    │   └── ...
    └── tls/
        └── mock_tls_transport.hpp

```

## Running Tests

### Prerequisites

-   CMake 3.16+
-   C++17 compiler (g++ or clang++)
-   Internet connection (first build downloads Google Test)

### Building and Running

```bash
# From components/lopcore/test/ directory
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# Run all tests
ctest --verbose

# Or run specific test
./test_logger
./test_mqtt_operations
```

### Test Coverage Goals

-   **Overall Target**: 80%+ line coverage
-   **Per Component**: Minimum 70% coverage
-   **Critical Paths**: 100% coverage (error handling, state transitions)

### Writing Unit Tests

Example test structure:

```cpp
#include <gtest/gtest.h>
#include "logging/logger.hpp"

TEST(LoggerTest, InitializationSucceeds) {
    auto logger = Logger::getInstance();
    EXPECT_NE(logger, nullptr);
}

TEST(LoggerTest, LogLevelFiltering) {
    auto logger = Logger::getInstance();
    logger->setLevel(LogLevel::WARNING);

    // This should log
    EXPECT_NO_THROW(logger->warn("Test", "Warning message"));

    // This should not log (below threshold)
    EXPECT_NO_THROW(logger->debug("Test", "Debug message"));
}
```

### Mock Strategy

For ESP-IDF APIs that cannot run on host:

1. Create mock header in `test/mocks/esp32/`
2. Provide minimal implementation for compilation
3. Use dependency injection in production code to enable testing

## Hardware Tests (Python)

### Prerequisites

```bash
# Install dependencies
pip3 install pyserial pytest

# For BLE tests (Week 5-6)
pip3 install bleak

# For HTTP tests
pip3 install requests
```

### Running Hardware Tests

```bash
# From scripts/hardware_tests/ directory

# Test device connection
python3 test_template.py --port /dev/ttyUSB0 --test dummy

# Run specific component test
python3 test_logging.py --port /dev/ttyUSB0

# Run all hardware tests (Week 12)
pytest test_*.py --port /dev/ttyUSB0 -v
```

### Writing Hardware Tests

Example test structure:

```python
from test_template import HardwareTestBase

class LoggingTest(HardwareTestBase):
    """Test logging functionality"""

    def run(self) -> bool:
        # Trigger a log action
        self.send_command("test_log")

        # Wait for expected log output
        if not self.wait_for_log("TEST_LOG", timeout=5.0):
            return False

        self.logger.info("Logging test passed")
        return True

# Usage
test = LoggingTest(port="/dev/ttyUSB0")
test.execute()
```

## Continuous Integration

### Pre-commit Checks

```bash
# Run before committing
./scripts/run_tests.sh

# This will:
# 1. Build unit tests
# 2. Run all unit tests
# 3. Generate coverage report
# 4. Check for 80%+ coverage
```

### CI Pipeline (Future)

1. **Build Stage**: Compile for ESP32-S3 target
2. **Unit Test Stage**: Run all Google Tests on host
3. **Coverage Stage**: Generate and upload coverage report
4. **Hardware Test Stage**: Run smoke tests on real device
5. **Integration Stage**: Full end-to-end test

## Debugging Tests

### Unit Tests

```bash
# Run with GDB
gdb ./test_logger
(gdb) run
(gdb) backtrace

# Verbose output
./test_logger --gtest_filter=LoggerTest.* --gtest_verbose
```

### Hardware Tests

```bash
# Enable debug logging
python3 test_logging.py --port /dev/ttyUSB0 --log-level DEBUG

# Monitor serial output separately
screen /dev/ttyUSB0 115200
```
