# CoreMQTT Async Example with TLS

This example demonstrates CoreMQTT client with TLS for secure MQTT communication.

## Overview

Shows how to use CoreMQTT with automatic message processing and TLS encryption for sensor data publishing and
command handling.

## Features

-   **TLS/SSL** encrypted connection
-   **Automatic processing** - messages handled asynchronously
-   **Sensor data** publishing with simulated temperature/humidity
-   **Command subscriptions** for device control
-   **Secure authentication** with certificates

## Prerequisites

-   ESP32 connected to WiFi
-   TLS-enabled MQTT broker
-   Root CA certificate for broker verification

The example uses `test.mosquitto.org:8883` by default.

## Configuration

Edit `main/main.cpp`:

```cpp
const char *brokerHost = "test.mosquitto.org";
const uint16_t brokerPort = 8883;
const char *sensorTopic = "lopcore/sensors/temperature";
const char *commandTopic = "lopcore/commands";
```

## Building

```bash
cd examples/05_mqtt_coremqtt_async
idf.py build
idf.py flash monitor
```

## Expected Output

```
I (1234) mqtt_coremqtt_async: ===========================================
I (1235) mqtt_coremqtt_async: CoreMQTT Async Client Example (TLS)
I (1236) mqtt_coremqtt_async: ===========================================
I (1240) mqtt_coremqtt_async: Setting up TLS transport to test.mosquitto.org:8883...
I (1450) mqtt_coremqtt_async: ✓ TLS connection established
I (1451) mqtt_coremqtt_async: Creating CoreMQTT client...
I (1452) mqtt_coremqtt_async: Connecting to broker...
I (1650) mqtt_coremqtt_async: ✓ Connected to broker
I (1651) mqtt_coremqtt_async: Subscribing to topics...
I (1652) mqtt_coremqtt_async: ✓ Subscribed to command and response topics
I (1653) mqtt_coremqtt_async: ✓ Published initial status
I (1654) mqtt_coremqtt_async: Starting periodic publishing (every 20 seconds)...
```

## Testing

### Monitor Sensor Data

```bash
mosquitto_sub -h test.mosquitto.org -p 8883 \
    --cafile /etc/ssl/certs/ca-certificates.crt \
    -t "lopcore/sensors/#" -v
```

You'll see periodic messages like:

```
lopcore/sensors/temperature {"temperature":22.5,"humidity":45.0,"counter":0}
lopcore/sensors/temperature {"temperature":23.0,"humidity":46.0,"counter":1}
```

### Send Commands

```bash
mosquitto_pub -h test.mosquitto.org -p 8883 \
    --cafile /etc/ssl/certs/ca-certificates.crt \
    -t "lopcore/commands" -m "status"
```

## What the Example Does

1. **Initializes** security provider for certificate management
2. **Establishes** TLS connection to broker
3. **Creates** CoreMQTT client with TLS transport
4. **Subscribes** to command and response topics
5. **Publishes** initial online status
6. **Sends** sensor data every 20 seconds:
    - Simulated temperature (22.5°C - 27.0°C)
    - Simulated humidity (45% - 65%)
    - Message counter
7. **Handles** incoming commands asynchronously
8. **Shows** statistics periodically

## Code Structure

### 1. Setup TLS Transport

```cpp
auto tlsConfig = lopcore::tls::TlsConfig::builder()
    .serverCertificate(securityProvider->getRootCertificate())
    .serverName(brokerHost)
    .verifyPeer(true)
    .build();

auto tlsTransport = std::make_shared<lopcore::tls::MbedtlsTransport>();
tlsTransport->connect(brokerHost, brokerPort, tlsConfig);
```

### 2. Create MQTT Client

```cpp
auto mqttConfig = lopcore::mqtt::MqttConfig::builder()
    .clientId(clientId)
    .keepAlive(std::chrono::seconds(60))
    .cleanSession(true)
    .build();

auto mqttClient = std::make_unique<lopcore::mqtt::CoreMqttClient>(
    mqttConfig, tlsTransport);
```

### 3. Subscribe and Publish

```cpp
// Subscribe
mqttClient->subscribe(commandTopic, onCommandReceived,
                     lopcore::mqtt::MqttQos::AT_LEAST_ONCE);

// Publish
mqttClient->publishString(sensorTopic, jsonData,
                         lopcore::mqtt::MqttQos::AT_LEAST_ONCE, false);
```

## TLS Certificate Setup

### For Test Brokers

The example uses the security provider's root certificate. For test.mosquitto.org, you need the root CA from
their certificate chain.

### For Production

1. **Store certificates securely**:

    ```cpp
    // In NVS or secure element
    securityProvider->storeRootCertificate(caCert);
    securityProvider->storeClientCertificate(deviceCert);
    securityProvider->storePrivateKey(deviceKey);
    ```

2. **Enable mutual TLS**:
    ```cpp
    auto tlsConfig = lopcore::tls::TlsConfig::builder()
        .serverCertificate(rootCA)
        .clientCertificate(deviceCert)  // Add client cert
        .clientKey(deviceKey)            // Add private key
        .serverName(brokerHost)
        .verifyPeer(true)
        .build();
    ```

## When to Use CoreMQTT Async

✅ **Use CoreMQTT Async when:**

-   Need TLS/SSL encryption
-   Want automatic message processing
-   Working with IoT platforms requiring TLS
-   Need quality of service guarantees

❌ **Consider alternatives:**

-   **ESP-MQTT** (Example 04) - Simpler for non-TLS brokers
-   **CoreMQTT Sync** (Example 06) - Need manual message control

## Troubleshooting

### TLS Connection Fails

-   Verify broker supports TLS on port 8883
-   Check root CA certificate is correct
-   Ensure system time is set (required for cert validation)
-   Try disabling `verifyPeer` for debugging (not recommended for production)

### No Messages Received

-   Check subscriptions succeeded
-   Verify topic names match
-   Monitor with mosquitto_sub to confirm broker is working
-   Check firewall allows port 8883

### Memory Issues

-   TLS uses more memory than non-TLS
-   Monitor heap with `esp_get_free_heap_size()`
-   Reduce buffer sizes if needed
-   Limit concurrent subscriptions

## Next Steps

-   **Example 06**: Manual message processing for request-response patterns
-   **Production**: Add OTA updates, error recovery, persistent sessions
-   **AWS IoT**: Adapt for AWS IoT Core with device certificates

## Further Reading

-   [MQTT Client Selection Guide](../../docs/MQTT_CLIENTS.md)
-   [CoreMQTT Documentation](https://github.com/FreeRTOS/coreMQTT)
