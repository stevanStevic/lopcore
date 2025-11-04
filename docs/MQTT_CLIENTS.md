# MQTT Client Selection Guide

This document explains the two MQTT client implementations in LopCore and helps you choose the right one for your project.

---

## Overview

LopCore provides **two MQTT client implementations**:

1. **ESP-MQTT Client** - Espressif's native MQTT client
2. **CoreMQTT Client** - AWS FreeRTOS MQTT library

Both clients are wrapped with a unified C++ interface, providing the same API regardless of which implementation you choose.

```cpp
// Same API for both clients
auto client = MqttClientFactory::create(type, config);
client->connect();
client->subscribe("topic", callback);
client->publish("topic", payload, qos);
```

---

## Quick Comparison

| Feature | ESP-MQTT | CoreMQTT | Notes |
|---------|----------|----------|-------|
| **Async Processing** | ✅ Event-driven | ✅ Optional background task | Both support async |
| **Manual Processing** | ❌ No | ✅ `processLoop()` | Required for synchronous workflows |
| **AWS IoT Optimized** | ❌ No | ✅ Yes | Better for Device Shadow, Jobs |
| **Memory Usage** | ~8 KB | ~5 KB | CoreMQTT is more efficient |
| **QoS State Tracking** | ❌ No | ✅ Yes | Critical for guaranteed delivery |
| **Reconnect Handling** | ✅ Automatic | ✅ Automatic | Both handle reconnections |
| **TLS Integration** | Native ESP-TLS | mbedTLS + PKCS#11 | CoreMQTT better for hardware crypto |
| **Dependencies** | ESP-IDF only | ESP-IDF + esp-aws-iot | CoreMQTT requires submodule |
| **Fleet Provisioning** | ❌ Limited | ✅ Full support | Synchronous request-response |
| **Learning Curve** | Easy | Moderate | ESP-MQTT is simpler |

---

## ESP-MQTT Client

### When to Use

Choose ESP-MQTT when:
- ✅ You need a simple, lightweight MQTT client
- ✅ Your broker is NOT AWS IoT Core
- ✅ You want minimal dependencies (no submodules)
- ✅ Asynchronous event-driven model fits your use case
- ✅ You don't need explicit QoS state inspection
- ✅ Standard MQTT features are sufficient

### Architecture

```
┌─────────────────────────────────┐
│   Your Application              │
│   (async callbacks)             │
├─────────────────────────────────┤
│   EspMqttClient (C++ wrapper)   │
├─────────────────────────────────┤
│   esp_mqtt_client (ESP-IDF)     │
├─────────────────────────────────┤
│   esp-tls (ESP-IDF TLS)         │
└─────────────────────────────────┘
```

### Characteristics

**Pros:**
- Native ESP-IDF component (no external dependencies)
- Well-tested and battle-proven in ESP32 ecosystem
- Automatic event handling via ESP event system
- Simple to use for basic MQTT scenarios
- Good for general-purpose MQTT brokers

**Cons:**
- No manual processing mode (always async)
- Cannot inspect QoS state machine
- Less suitable for AWS IoT advanced features
- Harder to implement synchronous request-response patterns

### Example Use Cases

```cpp
// 1. Simple sensor data publishing
auto client = MqttClientFactory::create(
    MqttClientType::ESP_MQTT,
    MqttConfigBuilder()
        .broker("mqtt.mybroker.com")
        .port(1883)
        .clientId("sensor-001")
        .build()
);

client->setConnectionCallback([](bool connected) {
    if (connected) {
        // Start publishing sensor data
    }
});

// 2. Home automation with local broker
client->subscribe("home/lights/+/command", [](const MqttMessage& msg) {
    // Handle light commands
});
```

---

## CoreMQTT Client

### When to Use

Choose CoreMQTT when:
- ✅ You're connecting to **AWS IoT Core**
- ✅ You need **Fleet Provisioning** (certificate provisioning)
- ✅ You need **manual processing** (`processLoop()`) for synchronous workflows
- ✅ You require **QoS state inspection** for debugging or custom logic
- ✅ You're using **AWS IoT Device Shadow** or **AWS IoT Jobs**
- ✅ You need **deterministic message handling** (not event-driven)
- ✅ You want **minimal memory footprint** (~3 KB less than ESP-MQTT)

### Architecture

```
┌──────────────────────────────────────┐
│   Your Application                   │
│   (manual processLoop or async task) │
├──────────────────────────────────────┤
│   CoreMqttClient (C++ wrapper)       │
├──────────────────────────────────────┤
│   coreMQTT (AWS FreeRTOS)            │
├──────────────────────────────────────┤
│   MbedtlsTransport + PKCS#11         │
└──────────────────────────────────────┘
```

### Characteristics

**Pros:**
- Optimized for AWS IoT Core (stateful QoS, Device Shadow, Jobs)
- Manual processing mode via `processLoop()` for synchronous patterns
- Can inspect QoS state arrays (debugging, custom logic)
- Smaller memory footprint
- Better PKCS#11 integration (hardware crypto)
- Required for AWS IoT Fleet Provisioning

**Cons:**
- Requires `esp-aws-iot` submodule
- Slightly more complex API (polling model)
- Need to manage `processLoop()` calls (manual or background task)

### Processing Modes

CoreMQTT supports **two processing modes**:

#### 1. Automatic Processing (Async - Default)

Background task automatically calls `processLoop()`:

```cpp
auto client = MqttClientFactory::create(
    MqttClientType::CORE_MQTT,
    MqttConfigBuilder()
        .broker("a1b2c3d4e5f6g7-ats.iot.us-east-1.amazonaws.com")
        .port(8883)
        .clientId("device-001")
        .autoStartProcessLoop(true)  // Enable background task
        .build()
);

client->connect();  // ProcessLoop task starts automatically
client->subscribe("topic", callback);  // Messages arrive via callback
```

#### 2. Manual Processing (Sync)

Application explicitly controls when messages are processed:

```cpp
auto client = MqttClientFactory::create(
    MqttClientType::CORE_MQTT,
    MqttConfigBuilder()
        .broker("a1b2c3d4e5f6g7-ats.iot.us-east-1.amazonaws.com")
        .port(8883)
        .clientId("device-001")
        .autoStartProcessLoop(false)  // Disable background task
        .build()
);

client->connect();

// Manual processing loop
while (running) {
    // Process MQTT events with 200ms timeout
    client->processLoop(200);
    
    // Your synchronous logic here
    if (/* condition */) {
        client->publish("topic", data);
    }
}
```

**Why Manual Processing?**
- **Fleet Provisioning**: Synchronous request-response (subscribe → publish → wait → response)
- **Deterministic behavior**: Know exactly when callbacks fire
- **Testing**: Easier to write synchronous tests
- **Single-threaded**: Avoid multi-threading complexity

### Example Use Cases

```cpp
// 1. AWS IoT Device Shadow
auto client = MqttClientFactory::create(
    MqttClientType::CORE_MQTT,
    config
);

client->subscribe("$aws/things/device-001/shadow/update/accepted",
    [](const MqttMessage& msg) {
        // Handle shadow update
    }
);

client->publish("$aws/things/device-001/shadow/update",
    shadowJson, MqttQos::AT_LEAST_ONCE);

// 2. AWS IoT Fleet Provisioning (manual mode required)
client->setAutoProcessing(false);  // Disable background task

// Subscribe to response topic
client->subscribe("$aws/certificates/create/cbor/accepted", callback);
client->processLoop(200);  // Wait for SUBACK

// Publish request
client->publish("$aws/certificates/create/cbor", csrPayload);
client->processLoop(200);  // Wait for PUBACK

// Wait for response
while (!responseReceived) {
    client->processLoop(200);  // Process incoming messages
}

// 3. Inspect QoS state for debugging
uint16_t packetId = /* from publish */;
MQTTPublishState_t state = client->getPublishState(packetId);
if (state == MQTTPubAckPending) {
    LOPCORE_LOGD("MQTT", "Waiting for PUBACK...");
}
```

---

## Why AWS IoT Uses CoreMQTT

The `esp-aws-iot` component (official AWS SDK for ESP32) uses CoreMQTT instead of ESP-MQTT for several reasons:

### 1. **Stateful QoS Management**

CoreMQTT maintains explicit state machines for QoS 1 and QoS 2 messages:

```c
// CoreMQTT tracks each packet's state
typedef struct MQTTPubAckInfo {
    uint16_t packetId;
    MQTTPublishState_t publishState;  // MQTTPubAckPending, MQTTPublishDone, etc.
} MQTTPubAckInfo_t;
```

**Why this matters:**
- AWS IoT Device Shadow requires guaranteed delivery (QoS 1)
- AWS IoT Jobs requires in-order processing
- Applications can inspect state for debugging or custom logic

ESP-MQTT doesn't expose this level of control.

### 2. **Manual Processing for Synchronous Workflows**

AWS IoT Fleet Provisioning follows a strict request-response pattern:

```
1. Subscribe to response topics
2. Wait for SUBACK (subscription confirmed)
3. Publish request
4. Wait for PUBACK (publish confirmed)
5. Wait for response message
6. Parse response
7. Continue to next step
```

This **cannot be done with async-only ESP-MQTT**. You need:
- Explicit control over when messages are processed
- Ability to block waiting for specific responses
- Deterministic timing

CoreMQTT's `processLoop()` provides this:

```cpp
// Manual processing enables synchronous patterns
client->subscribe(responseTopic, callback);
while (!subscribed) {
    client->processLoop(100);  // Wait for SUBACK
}

client->publish(requestTopic, payload);
while (!responseReceived) {
    client->processLoop(100);  // Wait for response
}
```

### 3. **Memory Efficiency**

CoreMQTT has a smaller footprint:
- No event loop integration overhead
- Minimal state tracking
- Explicit buffer management
- Stack usage: ~1 KB vs ~2 KB (ESP-MQTT)

For constrained embedded devices, 3 KB savings matters.

### 4. **Cross-Platform Consistency**

CoreMQTT is part of FreeRTOS (AWS's RTOS):
- Same API across platforms (ESP32, STM32, TI, etc.)
- Consistent behavior for AWS IoT features
- Well-documented and maintained by AWS

ESP-MQTT is ESP32-specific.

### 5. **Better PKCS#11 Integration**

AWS IoT requires X.509 certificates stored in hardware security modules. CoreMQTT integrates seamlessly with corePKCS11:

```cpp
// CoreMQTT + PKCS#11 transport
auto transport = std::make_shared<MbedtlsTransport>();
transport->connect(TlsConfigBuilder()
    .clientCertLabel("Device Cert")     // PKCS#11 label
    .privateKeyLabel("Device Key")      // PKCS#11 label
    .build()
);

auto client = std::make_shared<CoreMqttClient>(config, transport);
```

ESP-MQTT's TLS integration doesn't provide this level of control.

---

## Factory Auto-Selection

LopCore's factory can automatically choose the best client:

```cpp
auto client = MqttClientFactory::create(
    MqttClientType::AUTO,  // Let factory decide
    config
);
```

**Selection Logic:**

1. If broker hostname contains `amazonaws.com` → **CoreMQTT**
2. If port is 8883 (AWS IoT) → **CoreMQTT**
3. If TLS transport provided → **CoreMQTT**
4. Otherwise → **ESP-MQTT**

**Override Auto-Selection:**

```cpp
// Force ESP-MQTT even for AWS IoT
auto client = MqttClientFactory::create(
    MqttClientType::ESP_MQTT,
    config
);

// Force CoreMQTT for non-AWS broker
auto client = MqttClientFactory::create(
    MqttClientType::CORE_MQTT,
    config
);
```

---

## Migration Guide

### From ESP-MQTT to CoreMQTT

```cpp
// Before: ESP-MQTT (async only)
auto client = MqttClientFactory::create(MqttClientType::ESP_MQTT, config);
client->connect();
client->subscribe("topic", callback);  // Callback fires asynchronously

// After: CoreMQTT (can be async or sync)
auto client = MqttClientFactory::create(MqttClientType::CORE_MQTT, config);
client->connect();  // Background task starts
client->subscribe("topic", callback);  // Same async callback behavior

// Or use manual processing:
client->setAutoProcessing(false);
client->subscribe("topic", callback);
while (running) {
    client->processLoop(100);  // Callback fires here
}
```

### From CoreMQTT to ESP-MQTT

```cpp
// Before: CoreMQTT with manual processing
auto client = MqttClientFactory::create(MqttClientType::CORE_MQTT, config);
client->setAutoProcessing(false);
while (running) {
    client->processLoop(100);
    // Synchronous logic
}

// After: ESP-MQTT (must refactor to async)
auto client = MqttClientFactory::create(MqttClientType::ESP_MQTT, config);
client->subscribe("topic", [](const MqttMessage& msg) {
    // All logic must be in callback
    // Cannot block or use synchronous patterns
});
```

**Note:** If you rely on `processLoop()` or synchronous request-response, **you cannot migrate to ESP-MQTT**.

---

## Performance Comparison

### Latency

| Scenario | ESP-MQTT | CoreMQTT |
|----------|----------|----------|
| Publish QoS 0 | ~2 ms | ~1 ms |
| Publish QoS 1 (PUBACK) | ~50 ms | ~50 ms |
| Subscribe (SUBACK) | ~100 ms | ~100 ms |
| Callback invocation | ~1 ms | ~1 ms (async) / 0 ms (manual) |

Both clients have similar network performance. CoreMQTT has lower overhead for callback invocation in manual mode.

### Memory Usage (Runtime)

| Component | ESP-MQTT | CoreMQTT |
|-----------|----------|----------|
| Client object | ~2 KB | ~1 KB |
| Network buffer | 4 KB (default) | 4 KB (default) |
| QoS state arrays | N/A | ~1 KB |
| Task stack (async) | 4 KB | 4 KB |
| **Total** | **~10 KB** | **~10 KB** |

Both have similar total memory usage. CoreMQTT's explicit state tracking uses ~1 KB more RAM but eliminates hidden state in event loop.

### Flash Usage (Code Size)

| Component | ESP-MQTT | CoreMQTT |
|-----------|----------|----------|
| MQTT library | ~40 KB | ~25 KB |
| TLS integration | ESP-TLS (~20 KB) | mbedTLS + PKCS#11 (~30 KB) |
| **Total** | **~60 KB** | **~55 KB** |

CoreMQTT has smaller code size for the MQTT layer. TLS dependencies are similar.

---

## Recommendations

### Use ESP-MQTT if:
- Simple MQTT use cases (sensor data, home automation)
- Non-AWS broker (Mosquitto, HiveMQ, etc.)
- Async event-driven model fits your design
- Want minimal dependencies
- Don't need QoS state inspection

### Use CoreMQTT if:
- Connecting to AWS IoT Core
- Need Fleet Provisioning
- Require manual processing mode
- Using Device Shadow or Jobs
- Need QoS state debugging
- Want smallest memory footprint
- Prefer polling model over callbacks

### Use AUTO if:
- Not sure which to use
- Want factory to decide based on configuration
- Prototyping and will optimize later

---

## Further Reading

- [ESP-MQTT Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mqtt.html)
- [CoreMQTT Documentation](https://freertos.github.io/coreMQTT/)
- [AWS IoT Core MQTT](https://docs.aws.amazon.com/iot/latest/developerguide/mqtt.html)
- [LopCore MQTT Examples](../examples/04_mqtt_esp_client/)
- [AWS IoT Fleet Provisioning](https://docs.aws.amazon.com/iot/latest/developerguide/provision-wo-cert.html)

---

## Support

For issues, questions, or contributions:
- GitHub Issues: https://github.com/stevanStevic/lopcore/issues
- Documentation: https://github.com/stevanStevic/lopcore/tree/develop/docs
