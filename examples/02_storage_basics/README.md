# Storage Basics Example

This example demonstrates both NVS and SPIFFS storage using LopCore's unified storage interface.

## What it demonstrates

-   **NVS Storage**: Key-value storage for configuration (persistent across reboots)
-   **SPIFFS Storage**: File system storage for larger data and files
-   Storage factory pattern
-   Reading and writing text and binary data
-   Checking file existence
-   Listing files
-   Storage usage statistics

## Hardware Required

-   Any ESP32/ESP32-S2/ESP32-S3/ESP32-C3 development board

## Partition Table

This example requires a partition table with both NVS and SPIFFS partitions. Most default partition tables
include these.

## How to use

### Build and Flash

```bash
idf.py build flash monitor
```

## Example Output

```
I (123) STORAGE_EXAMPLE: ===========================================
I (124) STORAGE_EXAMPLE: LopCore Storage Basics Example
I (125) STORAGE_EXAMPLE: ===========================================

I (126) STORAGE_EXAMPLE: --- NVS Storage Example ---
I (127) STORAGE_EXAMPLE: Writing configuration to NVS...
I (130) STORAGE_EXAMPLE: Reading configuration from NVS...
I (131) STORAGE_EXAMPLE: WiFi SSID: MyNetwork
I (132) STORAGE_EXAMPLE: MQTT Broker: mqtt.example.com
I (133) STORAGE_EXAMPLE: Device Name: ESP32-Device-001
I (134) STORAGE_EXAMPLE: WiFi credentials found in NVS

I (135) STORAGE_EXAMPLE: --- SPIFFS Storage Example ---
I (136) STORAGE_EXAMPLE: Writing JSON config file...
I (140) STORAGE_EXAMPLE: Config file written successfully
I (141) STORAGE_EXAMPLE: Read config file:
I (142) STORAGE_EXAMPLE: {
    "version": "1.0.0",
    "mode": "auto",
    "interval": 60,
    "enabled": true
}

I (145) STORAGE_EXAMPLE: Writing binary certificate...
I (146) STORAGE_EXAMPLE: Certificate written successfully
I (147) STORAGE_EXAMPLE: Read certificate: 8 bytes

I (148) STORAGE_EXAMPLE: Listing files in SPIFFS:
I (149) STORAGE_EXAMPLE:   - config.json
I (150) STORAGE_EXAMPLE:   - cert.der

I (151) STORAGE_EXAMPLE: Storage usage:
I (152) STORAGE_EXAMPLE:   Total: 512000 bytes
I (153) STORAGE_EXAMPLE:   Used:  1024 bytes (0%)
I (154) STORAGE_EXAMPLE:   Free:  510976 bytes

I (155) STORAGE_EXAMPLE: Deleting test file...
I (156) STORAGE_EXAMPLE: File deleted successfully
```

## Key Concepts

### NVS vs. SPIFFS

| Feature           | NVS                            | SPIFFS                         |
| ----------------- | ------------------------------ | ------------------------------ |
| **Purpose**       | Configuration, key-value pairs | Files, larger data             |
| **Size**          | Small values (<4KB)            | Any size                       |
| **Performance**   | Very fast                      | Moderate                       |
| **Wear Leveling** | Built-in                       | Yes                            |
| **Best For**      | WiFi creds, settings, counters | Logs, certificates, web assets |

### Factory Pattern

```cpp
// Create NVS storage
auto nvs = lopcore::StorageFactory::createNvs("namespace");

// Create SPIFFS storage
auto spiffs = lopcore::StorageFactory::createSpiffs("/spiffs");
```

### Error Handling with std::optional

```cpp
auto data = storage->read("key");
if (data)  // Check if value exists
{
    // Use data->c_str()
}
```

## See Also

-   [01_basic_logging](../01_basic_logging/) - Basic logging setup
-   [03_mqtt_basics](../03_mqtt_basics/) - MQTT client usage
