# CoreMQTT Sync Example - Manual Processing

This example demonstrates CoreMQTT in **manual/synchronous mode** for deterministic request-response patterns
and controlled message processing.

## Overview

Unlike async mode where a background task automatically processes MQTT events, **manual mode** gives you
explicit control over when messages are processed via `processLoop()`. This enables:

-   **Synchronous request-response** patterns (subscribe → publish → wait → process)
-   **Deterministic message ordering** for state machines
-   **Controlled timing** for critical workflows
-   **No race conditions** between requests and responses

## Why Manual Mode?

### Use Cases

✅ **Use manual mode when you need:**

-   Request-response transactions with guaranteed ordering
-   State machines with strict transition control
-   Multi-step workflows (like provisioning sequences)
-   Fine-grained control over message timing
-   Testing and debugging message flows

❌ **Use async mode instead for:**

-   Simple pub/sub patterns
-   Event-driven architectures
-   High-frequency messaging
-   Fire-and-forget publishing

### Key Concept: processLoop()

In manual mode, **you control when MQTT processes messages** by calling:

```cpp
mqttClient->processLoop(200);  // Process for 200ms
```

This processes:

-   Incoming PUBLISH messages → triggers your callbacks
-   SUBACK confirmations → subscription succeeds
-   PUBACK acknowledgments → QoS 1 publish completes
-   PINGREQ/PINGRESP → keepalive maintenance

Without calling `processLoop()`, **no messages are received or processed**.

## Features

-   **Manual message processing** - explicit `processLoop()` control
-   **TLS with certificate verification** - certificate stored in SPIFFS
-   **Request-response helpers** - synchronous transaction patterns
-   **Response state tracking** - manages request/response correlation
-   **Timeout handling** - prevents infinite waits
-   **Public broker support** - works with test.mosquitto.org

## Prerequisites

-   ESP32 connected to WiFi
-   Understanding of MQTT pub/sub model
-   Familiarity with request-response patterns

The example uses `test.mosquitto.org:8883` by default with TLS enabled.

## Configuration

Edit `main/main.cpp`:

```cpp
const char *brokerHost = "test.mosquitto.org";
const uint16_t brokerPort = 8883;
const char *requestTopic = "lopcore/rpc/request";
const char *responseTopic = "lopcore/rpc/response";
```

## Building

```bash
cd examples/06_mqtt_coremqtt_sync
idf.py build
idf.py flash monitor
```

## Expected Output

```
I (1234) mqtt_coremqtt_sync: ===========================================
I (1235) mqtt_coremqtt_sync: CoreMQTT Sync Client Example
I (1236) mqtt_coremqtt_sync: Request-Response Pattern with Manual Loop
I (1237) mqtt_coremqtt_sync: ===========================================
I (1240) mqtt_coremqtt_sync: Initializing SPIFFS...
I (1241) mqtt_coremqtt_sync: SPIFFS: 960 KB total, 10 KB used
I (1242) mqtt_coremqtt_sync: Writing certificate to /spiffs/root_ca.crt...
I (1243) mqtt_coremqtt_sync: Certificate written successfully (1774 bytes)
I (1244) mqtt_coremqtt_sync: Configuring TLS...
I (1450) mqtt_coremqtt_sync: ✓ TLS connection established
I (1650) mqtt_coremqtt_sync: ✓ MQTT connected - Manual mode
I (1651) mqtt_coremqtt_sync: Subscribing to: lopcore/rpc/response
I (1652) mqtt_coremqtt_sync: ✓ Subscribed to: lopcore/rpc/response
I (1750) mqtt_coremqtt_sync: ===========================================
I (1751) mqtt_coremqtt_sync: Starting Request-Response Examples
I (1752) mqtt_coremqtt_sync: ===========================================
I (1753) mqtt_coremqtt_sync:
I (1754) mqtt_coremqtt_sync: --- Example 1: Ping Request ---
I (1755) mqtt_coremqtt_sync: Publishing to lopcore/rpc/request: ping
I (1850) mqtt_coremqtt_sync: Waiting for response (timeout: 10000 ms)...
I (2100) mqtt_coremqtt_sync: 📬 Response received: pong
I (2101) mqtt_coremqtt_sync: ✓ Transaction complete: pong
```

## What the Example Does

### 1. Initialization

-   Initializes SPIFFS filesystem (960KB partition)
-   Writes ISRG Root X1 certificate to `/spiffs/root_ca.crt`
-   Sets up TLS transport with certificate verification
-   Creates CoreMQTT client in manual mode (no background task)
-   Connects to broker with manual CONNACK processing
-   Subscribes to response topic

### 2. Request-Response Examples

The example demonstrates three request-response transactions:

#### Example 1: Ping Request

```cpp
// 1. Reset response state
responseState.reset();

// 2. Publish request
publishWithProcessing("lopcore/rpc/request", "ping");

// 3. Wait for response
waitForResponse(10000);  // 10 second timeout

// 4. Process response
handleResponse(responseState.payload);  // "pong"
```

#### Example 2: Echo Request

```cpp
publishWithProcessing("lopcore/rpc/request", "echo:Hello!");
waitForResponse(10000);
// Response: "Hello!"
```

#### Example 3: Info Request

```cpp
publishWithProcessing("lopcore/rpc/request", "info");
waitForResponse(10000);
// Response: JSON device information
```

### 3. Periodic Status Publishing

After examples complete, enters a loop that:

-   Publishes status every 15 seconds
-   Shows counter, uptime, heap memory
-   Continues processing incoming messages
-   Displays statistics

## Code Structure

### Helper Function: Subscribe with Processing

```cpp
bool subscribeWithProcessing(const char *topic, uint32_t timeoutMs)
{
    // 1. Send subscribe request
    mqttClient->subscribe(topic, onMessageReceived, MqttQos::AT_LEAST_ONCE);

    // 2. Process events until SUBACK received
    uint32_t elapsed = 0;
    while (elapsed < timeoutMs) {
        mqttClient->processLoop(200);  // Handle incoming packets

        if (mqttClient->isSubscribed(topic)) {
            return true;  // SUBACK received
        }

        vTaskDelay(pdMS_TO_TICKS(200));
        elapsed += 200;
    }

    return false;  // Timeout
}
```

**Why?** The SUBACK packet arrives asynchronously. We must call `processLoop()` to receive and handle it.

### Helper Function: Publish with Processing

```cpp
bool publishWithProcessing(const char *topic, const char *payload)
{
    // 1. Send publish request
    mqttClient->publishString(topic, payload, MqttQos::AT_LEAST_ONCE, false);

    // 2. Process until PUBACK received
    mqttClient->processLoop(200);

    return true;
}
```

**Why?** For QoS 1 publishes, we need to receive the PUBACK acknowledgment.

### Helper Function: Wait for Response

```cpp
bool waitForResponse(uint32_t timeoutMs)
{
    uint32_t elapsed = 0;

    while (elapsed < timeoutMs) {
        // Process incoming messages
        mqttClient->processLoop(200);

        // Check if response arrived (set by callback)
        if (responseState.received) {
            return true;
        }

        vTaskDelay(pdMS_TO_TICKS(200));
        elapsed += 200;
    }

    return false;  // Timeout
}
```

**Why?** Messages only arrive when we call `processLoop()`. This gives us deterministic control.

### Request-Response Transaction Pattern

```cpp
bool performRequestResponse(const char *request)
{
    // Step 1: Subscribe to response topic (if not already subscribed)
    subscribeWithProcessing(responseTopic, 5000);

    // Step 2: Reset response state (CRITICAL!)
    responseState.reset();

    // Step 3: Publish request
    publishWithProcessing(requestTopic, request);

    // Step 4: Wait for response
    if (!waitForResponse(10000)) {
        LOPCORE_LOGW(TAG, "Response timeout");
        return false;
    }

    // Step 5: Process response
    handleResponse(responseState.payload);

    return true;
}
```

**Critical:** Reset response state BEFORE publishing to avoid processing stale responses.

## Testing

### Simulate Response Server

Create a simple MQTT client that responds to requests:

```python
import paho.mqtt.client as mqtt

def on_message(client, userdata, msg):
    request = msg.payload.decode()
    print(f"Request: {request}")

    if request == "ping":
        client.publish("lopcore/rpc/response", "pong")
    elif request.startswith("echo:"):
        client.publish("lopcore/rpc/response", request[5:])
    elif request == "info":
        client.publish("lopcore/rpc/response", '{"device":"esp32","version":"1.0"}')

client = mqtt.Client()
client.tls_set("/etc/ssl/certs/ca-certificates.crt")
client.on_message = on_message
client.connect("test.mosquitto.org", 8883, 60)
client.subscribe("lopcore/rpc/request")
client.loop_forever()
```

### Monitor Traffic

```bash
# Watch requests
mosquitto_sub -h test.mosquitto.org -p 8883 \
    --cafile /etc/ssl/certs/ca-certificates.crt \
    -t "lopcore/rpc/request" -v

# Watch responses
mosquitto_sub -h test.mosquitto.org -p 8883 \
    --cafile /etc/ssl/certs/ca-certificates.crt \
    -t "lopcore/rpc/response" -v
```

## Common Pitfalls

### 1. Forgetting to Reset State

❌ **Wrong:**

```cpp
publishWithProcessing(requestTopic, payload);
responseState.reset();  // TOO LATE!
waitForResponse();
```

✅ **Correct:**

```cpp
responseState.reset();  // Reset BEFORE publish
publishWithProcessing(requestTopic, payload);
waitForResponse();
```

### 2. Not Calling processLoop()

❌ **Wrong:**

```cpp
mqttClient->publish(topic, payload);
// Response will never arrive - no processLoop()!
if (responseState.received) { ... }
```

✅ **Correct:**

```cpp
mqttClient->publish(topic, payload);
while (!responseState.received) {
    mqttClient->processLoop(200);  // Process incoming messages
    vTaskDelay(pdMS_TO_TICKS(200));
}
```

### 3. Infinite Loop on Disconnect

❌ **Wrong:**

```cpp
while (!responseState.received) {
    mqttClient->processLoop(200);  // Returns false on disconnect!
}
```

✅ **Correct:**

```cpp
while (!responseState.received) {
    if (!mqttClient->processLoop(200)) {
        LOPCORE_LOGE(TAG, "Disconnected");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
}
```

## Troubleshooting

### Subscription Timeout

**Symptom:** `subscribeWithProcessing()` times out

**Solutions:**

-   Verify network connectivity
-   Check MQTT broker is reachable
-   Increase timeout (default 5000ms)
-   Enable debug logging

### Response Timeout

**Symptom:** `waitForResponse()` times out

**Solutions:**

-   Verify response topic is correct
-   Check responder is publishing response
-   Increase timeout (default 10000ms)
-   Add debug logging in message callback
-   Verify callback is receiving messages

### ProcessLoop Returns False

**Symptom:** `processLoop()` returns false immediately

**Solutions:**

-   Check TLS connection is still active
-   Verify network hasn't disconnected
-   Check for keepalive timeout
-   Add reconnection logic

### Memory Usage

**Symptom:** Memory usage grows over time

**Solutions:**

-   Ensure response payload is cleared with `reset()`
-   Check no accumulating subscriptions
-   Monitor heap with `esp_get_free_heap_size()`

## Comparison: Manual vs Async Mode

| Feature             | Manual Mode (Example 06)     | Async Mode (Example 05) |
| ------------------- | ---------------------------- | ----------------------- |
| Background task     | ❌ No                        | ✅ Yes                  |
| ProcessLoop control | ✅ Explicit                  | ❌ Automatic            |
| Message timing      | ✅ Deterministic             | ❌ Asynchronous         |
| Request-response    | ✅ Natural                   | ⚠️ Complex              |
| CPU usage           | ✅ Lower (on-demand)         | ⚠️ Higher (continuous)  |
| Best for            | State machines, provisioning | General pub/sub         |

## Performance Tips

1. **Tune Polling Interval**: Match your TLS recv timeout (200ms typical)
2. **Minimize Allocations**: Reuse buffers for payloads
3. **Batch Processing**: Process multiple messages per `processLoop()` call
4. **Timeout Management**: Set realistic timeouts (5-10 seconds typical)
5. **Error Handling**: Always check `processLoop()` return value

## Further Reading

-   [CoreMQTT Async Example](../05_mqtt_coremqtt_async/) - Background processing mode
-   [ESP-MQTT Example](../04_mqtt_esp_client/) - Standard MQTT client
-   [CoreMQTT Documentation](https://freertos.github.io/coreMQTT/)

## License

MIT License - See [LICENSE](../../LICENSE) for details.
