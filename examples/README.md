# LopCore Examples

This directory contains example applications demonstrating LopCore middleware features.

## Available Examples

| Example                                     | Description                                          | Components Used              |
| ------------------------------------------- | ---------------------------------------------------- | ---------------------------- |
| [01_basic_logging](01_basic_logging/)       | Console logging with different log levels            | Logger, ConsoleSink          |
| [02_storage_basics](02_storage_basics/)     | NVS and SPIFFS storage operations                    | StorageFactory, NVS, SPIFFS  |
| [03_state_machine](03_state_machine/)       | Type-safe hierarchical state machine                 | StateMachine, IState         |

### Coming Soon

-   MQTT client examples (basic and AWS IoT Core)
-   TLS with PKCS#11 certificates
-   Full application combining multiple components

## How to Use Examples

### Method 1: Standalone Build

Each example is a complete ESP-IDF project:

```bash
cd examples/01_basic_logging
idf.py build flash monitor
```

### Method 2: Copy to Your Project

Copy the `main/main.cpp` content into your own project and add lopcore to requirements:

```cmake
idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES lopcore nvs_flash
)
```

## Requirements

All examples require:

-   ESP-IDF v5.2.0 or later
-   LopCore component in your `components/` directory
-   ESP32/ESP32-S2/ESP32-S3/ESP32-C3 development board

## Example Structure

Each example is a complete ESP-IDF project with:

-   `main/main.cpp` - Example application code
-   `main/CMakeLists.txt` - Component registration
-   `CMakeLists.txt` - Project configuration
-   `README.md` - Detailed documentation and explanation

## Learning Path

**Recommended order:**

1. **01_basic_logging** - Start here to understand logging basics
2. **02_storage_basics** - Learn persistent storage (NVS + SPIFFS)
3. **03_state_machine** - Build type-safe state machines for complex logic

## Common Patterns

### Include Headers

```cpp
#include "lopcore/logging/logger.hpp"
#include "lopcore/storage/storage_factory.hpp"
#include "lopcore/mqtt/mqtt_client_factory.hpp"
#include "lopcore/state_machine/state_machine.hpp"
```

### Initialize Components

```cpp
// Logger
auto &logger = lopcore::Logger::getInstance();
logger.addSink(std::make_unique<lopcore::ConsoleSink>());

// Storage
auto storage = lopcore::StorageFactory::createNvs("config");

// MQTT
auto mqtt = lopcore::mqtt::MqttClientFactory::create(
    lopcore::mqtt::MqttClientType::AUTO, config);

// State Machine
enum class MyState { INIT, RUNNING, STOPPED };
lopcore::StateMachine<MyState> stateMachine;
```

## Troubleshooting

### "Component 'lopcore' not found"

Ensure LopCore is in your components directory:

```bash
cp -r path/to/lopcore YourProject/components/
```

### Build Errors

Clean build directory and rebuild:

```bash
idf.py fullclean
idf.py build
```

### Monitor Not Working

Check USB port permissions:

```bash
sudo usermod -a -G dialout $USER  # Linux
# Then logout and login again
```

## Getting Help

-   Check individual example README files for detailed documentation
-   Review the main [LopCore README](../README.md)
-   See [Architecture Documentation](../../docs/middleware_architecture.md)

## Contributing Examples

Want to add an example? Guidelines:

1. Keep it focused on one feature
2. Add comprehensive comments
3. Include a detailed README
4. Test on real hardware
5. Follow existing example structure

## License

All examples are provided under the MIT License and can be freely used in your projects.
