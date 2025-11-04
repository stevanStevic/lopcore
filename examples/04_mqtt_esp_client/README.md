# ESP-MQTT Client Example

This example demonstrates basic MQTT communication using the lopcore ESP-MQTT client wrapper.

## Overview

This example shows how to use the ESP-MQTT client for simple pub/sub messaging with a public MQTT broker.

## Features

- Connect to public MQTT broker
- Subscribe to command topics
- Publish telemetry data periodically
- Handle incoming messages with callbacks
- Connection status monitoring
- Message statistics

## Prerequisites

- ESP32 connected to WiFi
- Access to MQTT broker (uses `test.mosquitto.org` by default)

## Configuration

Edit `main/main.cpp` to customize:

```cpp
const char *brokerUri = "mqtt://test.mosquitto.org:1883";
const char *clientId = "esp32_lopcore_demo";
const char *pubTopic = "lopcore/demo/status";
const char *subTopic = "lopcore/demo/commands";
```

## Building

```bash
cd examples/04_mqtt_esp_client
idf.py build
```

## Running

```bash
idf.py flash monitor
```

## Expected Output

```
I (1234) mqtt_esp_example: ===========================================
I (1235) mqtt_esp_example: ESP-MQTT Client Example
I (1236) mqtt_esp_example: ===========================================
I (1237) mqtt_esp_example: Configuration:
I (1238) mqtt_esp_example:   Broker: mqtt://test.mosquitto.org:1883
I (1239) mqtt_esp_example:   Client ID: esp32_lopcore_demo
I (1240) mqtt_esp_example: Connecting to broker...
I (1250) mqtt_esp_example: ✓ Connected to broker
I (1251) mqtt_esp_example: Subscribing to: lopcore/demo/commands
I (1252) mqtt_esp_example: ✓ Subscribed successfully
I (1253) mqtt_esp_example: Try publishing to this topic from another device:
I (1254) mqtt_esp_example:   mosquitto_pub -h test.mosquitto.org -t lopcore/demo/commands -m "ping"
I (1255) mqtt_esp_example: ✓ Published status to: lopcore/demo/status
I (1256) mqtt_esp_example: Starting periodic publishing (every 15 seconds)...
```

## Testing with Mosquitto

### Monitor Device Messages

```bash
# Watch all messages on demo topics
mosquitto_sub -h test.mosquitto.org -t "lopcore/demo/#" -v
```

You'll see periodic telemetry like:
```
lopcore/demo/status {"counter":0,"uptime":45,"heap":298432}
lopcore/demo/status {"counter":1,"uptime":60,"heap":298432}
```

### Send Commands to Device

```bash
# Send a ping command
mosquitto_pub -h test.mosquitto.org -t "lopcore/demo/commands" -m "ping"
```

The device will receive and log the command.

## Code Structure

### 1. Create Configuration

```cpp
auto mqttConfig = lopcore::mqtt::MqttConfig::builder()
    .broker("mqtt://test.mosquitto.org:1883")
    .clientId("my-device")
    .keepAlive(std::chrono::seconds(60))
    .cleanSession(true)
    .build();
```

### 2. Create Client

```cpp
auto mqttClient = std::make_unique<lopcore::mqtt::EspMqttClient>(mqttConfig);
```

### 3. Set Callbacks

```cpp
mqttClient->setConnectionCallback([](bool connected) {
    // Handle connection state
});

mqttClient->setErrorCallback([](MqttError error, const std::string& msg) {
    // Handle errors
});
```

### 4. Connect and Subscribe

```cpp
mqttClient->connect();

mqttClient->subscribe(topic, [](const std::string& topic,
                                const std::vector<uint8_t>& payload,
                                MqttQos qos, bool retained) {
    // Handle received message
}, lopcore::mqtt::MqttQos::AT_LEAST_ONCE);
```

### 5. Publish Messages

```cpp
mqttClient->publishString(topic, "message", 
                         MqttQos::AT_LEAST_ONCE, 
                         false);
```

## What the Example Does

1. **Connects** to test.mosquitto.org
2. **Subscribes** to commands topic
3. **Publishes** initial online status
4. **Sends telemetry** every 15 seconds:
   - Message counter
   - Device uptime (seconds)
   - Free heap memory (bytes)
5. **Logs** all received messages
6. **Shows statistics** periodically

## When to Use ESP-MQTT

✅ **Use ESP-MQTT when:**
- Connecting to standard MQTT brokers
- Want simple async event handling
- Don't need manual message processing
- Prefer ESP-IDF native solution

❌ **Consider CoreMQTT instead for:**
- AWS IoT Core connections (see example 05)
- Manual message control (see example 06)
- Request-response patterns
- Fleet Provisioning workflows

## Next Steps

- **Example 05**: TLS connection with CoreMQTT
- **Example 06**: Request-response patterns
- **Production**: Add WiFi setup, error recovery, OTA updates

## Further Reading

- [MQTT Client Selection Guide](../../docs/MQTT_CLIENTS.md)
- [ESP-MQTT Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mqtt.html)
