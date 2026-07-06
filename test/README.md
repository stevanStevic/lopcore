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
│   ├── test_logger.cpp      # Logger tests
│   ├── test_file_sink.cpp   # File sink tests
│   ├── test_nvs_storage.cpp # Storage tests (plus test_spiffs_storage.cpp, storage/)
│   ├── mqtt/                # MQTT component tests
│   │   ├── test_mqtt_types.cpp
│   │   ├── test_mqtt_config.cpp
│   │   ├── test_mqtt_traits.cpp
│   │   └── ...
│   ├── prov/                # Provisioning component tests
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
-   C++20 compiler (g++ or clang++)
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
./test_mqtt_config
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

## Continuous Integration

### Pre-commit Checks

```bash
# Run before committing (from components/lopcore/test/build/)
cmake .. && make -j$(nproc) && ctest --output-on-failure
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
