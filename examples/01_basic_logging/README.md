# Basic Logging Example

This example demonstrates the LopCore logging system with console output.

## What it demonstrates

-   Initializing the logger singleton
-   Adding a console sink for colored output
-   Using different log levels (VERBOSE, DEBUG, INFO, WARN, ERROR)
-   Runtime log level changes
-   Formatted logging with variables
-   Periodic logging in a FreeRTOS task

## Hardware Required

-   Any ESP32/ESP32-S2/ESP32-S3/ESP32-C3 development board

## How to use

### Configure the project

```bash
idf.py menuconfig
```

Navigate to `LopCore Configuration -> Logging System` to configure:

-   Default log level
-   Enable colored console output

### Build and Flash

```bash
idf.py build flash monitor
```

## Example Output

```
I (123) APP: ===========================================
I (124) APP: LopCore Basic Logging Example
I (125) APP: ===========================================
I (126) APP: This is an info message
W (127) APP: This is a warning message
E (128) APP: This is an error message
I (129) SENSOR: Temperature: 25°C, Humidity: 65.5%
I (130) APP: Changing log level to DEBUG...
D (131) APP: Debug messages now visible!
I (132) APP: Starting periodic logging every 5 seconds...
I (5132) LOOP: Iteration 0 - System running normally
D (5133) SENSOR: Sensor reading: 42
I (10132) LOOP: Iteration 1 - System running normally
W (10133) SENSOR: High sensor value detected: 87
```

## Key Concepts

### Singleton Pattern

```cpp
auto &logger = lopcore::Logger::getInstance();
```

The logger uses the singleton pattern - one global instance shared across your application.

### Log Levels (from lowest to highest)

1. **VERBOSE** - Very detailed debug information
2. **DEBUG** - Detailed debugging information
3. **INFO** - General informational messages
4. **WARN** - Warning messages
5. **ERROR** - Error messages

### Macros vs. Methods

```cpp
// Using macros (recommended - more convenient)
LOPCORE_LOGI("TAG", "Message: %d", value);

// Using methods directly
logger.info("TAG", "Message: %d", value);
```

## See Also

-   [02_file_logging](../02_file_logging/) - Add file logging with rotation
-   [03_multiple_sinks](../03_multiple_sinks/) - Multiple output destinations
