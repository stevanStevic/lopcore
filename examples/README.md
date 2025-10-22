# LopCore Examples

This directory contains example applications demonstrating LopCore middleware features.

## Available Examples

### Basic Features

| Example                                 | Description                               | Components Used             |
| --------------------------------------- | ----------------------------------------- | --------------------------- |
| [01_basic_logging](01_basic_logging/)   | Console logging with different log levels | Logger, ConsoleSink         |
| [02_storage_basics](02_storage_basics/) | NVS and SPIFFS storage operations         | StorageFactory, NVS, SPIFFS |

### Advanced Features

More examples coming soon:

-   `03_mqtt_basics` - MQTT client connection and pub/sub
-   `04_mqtt_aws_iot` - AWS IoT Core integration
-   `05_tls_certificates` - TLS with PKCS#11 certificates
-   `06_combined_app` - Full application combining multiple components

## How to Use Examples

### Method 1: Standalone Build

Each example is a complete ESP-IDF project:

```bash
cd examples/01_basic_logging
idf.py build flash monitor
```

### Method 2: Copy to Your Project

Copy the main.cpp content into your own project and add lopcore to requirements:

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

Each example contains:

-   `main.cpp` - Example application code
-   `CMakeLists.txt` - Build configuration
-   `README.md` - Detailed documentation and explanation

## Learning Path

**Recommended order for beginners:**

1. **01_basic_logging** - Start here to understand logging basics
2. **02_storage_basics** - Learn persistent storage (NVS + SPIFFS)
3. **03_mqtt_basics** - Connect to MQTT broker
4. **04_mqtt_aws_iot** - AWS IoT Core integration
5. **05_tls_certificates** - Secure connections with certificates
6. **06_combined_app** - See everything working together

## Common Patterns

### Include Headers

```cpp
#include "lopcore/logging/logger.hpp"
#include "lopcore/storage/storage_factory.hpp"
#include "lopcore/mqtt/mqtt_client_factory.hpp"
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
