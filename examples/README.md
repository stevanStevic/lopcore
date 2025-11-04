# LopCore Examples

This directory contains example applications demonstrating LopCore middleware features.

## Available Examples

| Example                                                     | Description                                          | Components Used                     |
| ----------------------------------------------------------- | ---------------------------------------------------- | ----------------------------------- |
| [01_basic_logging](01_basic_logging/)                       | Console logging with different log levels            | Logger, ConsoleSink                 |
| [02_storage_basics](02_storage_basics/)                     | NVS and SPIFFS storage operations                    | StorageFactory, NVS, SPIFFS         |
| [03_state_machine](03_state_machine/)                       | Type-safe hierarchical state machine                 | StateMachine, IState                |
| [04_mqtt_esp_client](04_mqtt_esp_client/)                   | ESP-MQTT client with subscriptions and publishing    | EspMqttClient                       |
| [05_mqtt_coremqtt_async](05_mqtt_coremqtt_async/)           | CoreMQTT async mode with AWS IoT Device Shadow       | CoreMqttClient, TLS, PKCS#11        |
| [06_mqtt_coremqtt_sync](06_mqtt_coremqtt_sync/)             | CoreMQTT manual mode for Fleet Provisioning pattern  | CoreMqttClient, TLS, PKCS#11        |

### Coming Soon

-   Full application combining multiple components
-   Advanced state machine patterns

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
4. **04_mqtt_esp_client** - Simple MQTT with ESP-MQTT (standard brokers)
5. **05_mqtt_coremqtt_async** - AWS IoT with CoreMQTT async mode
6. **06_mqtt_coremqtt_sync** - Fleet Provisioning with CoreMQTT manual mode

### MQTT Client Selection

Not sure which MQTT example to use? See [MQTT Client Selection Guide](../docs/MQTT_CLIENTS.md).

**Quick decision:**
- **Standard MQTT broker** (Mosquitto, HiveMQ, etc.) → Use example 04 (ESP-MQTT)
- **AWS IoT Core** with Device Shadow/Jobs → Use example 05 (CoreMQTT async)
- **AWS IoT Fleet Provisioning** → Use example 06 (CoreMQTT sync)

## Common Patterns

### Include Headers

```cpp
#include "lopcore/logging/logger.hpp"
#include "lopcore/storage/storage_factory.hpp"
#include "lopcore/mqtt/mqtt_factory.hpp"
#include "lopcore/mqtt/esp_mqtt_client.hpp"
#include "lopcore/mqtt/coremqtt_client.hpp"
#include "lopcore/tls/tls_transport.hpp"
#include "lopcore/tls/pkcs11_provider.hpp"
#include "lopcore/state_machine/state_machine.hpp"
```

### Initialize Components

```cpp
// Logger
auto &logger = lopcore::Logger::getInstance();
logger.addSink(std::make_unique<lopcore::ConsoleSink>());

// Storage
auto storage = lopcore::StorageFactory::createNvs("config");

// MQTT (ESP-MQTT for standard brokers)
auto espMqttConfig = lopcore::mqtt::MqttConfig::builder()
    .broker("mqtt://test.mosquitto.org:1883")
    .clientId("my-device")
    .build();
auto espMqtt = std::make_unique<lopcore::mqtt::EspMqttClient>(espMqttConfig);

// MQTT (CoreMQTT for AWS IoT)
auto tlsTransport = std::make_shared<lopcore::tls::MbedtlsTransport>();
tlsTransport->connect(tlsConfig);

auto coreMqttConfig = lopcore::mqtt::MqttConfig::builder()
    .broker(awsEndpoint)
    .clientId("my-device")
    .autoStartProcessLoop(true)  // Async mode
    .build();
auto coreMqtt = std::make_unique<lopcore::mqtt::CoreMqttClient>(
    coreMqttConfig, tlsTransport);

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
