# BLE + AWS Fleet Provisioning Example

This example demonstrates the complete **zero-touch device provisioning** flow using the lopcore provisioning
module: WiFi credentials and AWS IoT claim credentials are delivered over BLE by a mobile app, and the device
then self-registers with AWS IoT using Fleet Provisioning.

## Overview

Provisioning happens in two consecutive phases:

### Phase 1 — BLE WiFi + AWS Credential Transfer

The lopcore `WiFiProvisioning` class wraps the ESP-IDF `wifi_prov_mgr` and exposes two channels:

- **Standard provisioning channel** — mobile app sends WiFi SSID + password (ESP-IDF BLE protocol)
- **Custom `"aws-data"` endpoint** — mobile app sends AWS credentials as a JSON payload:

```json
{
    "certificate": "<claim cert PEM>",
    "private_key": "<claim key PEM>",
    "aws_endpoint": "xxx.iot.us-east-1.amazonaws.com",
    "root_ca": "<root CA PEM>",
    "provisioning_template": "GrowBorgTemplate"
}
```

The `AwsDataEndpointHandler` receives this payload and writes each field to NVS via `StorageCallbacks`.

### Phase 2 — AWS IoT Fleet Provisioning

Using the claim credentials stored in Phase 1:

1. Import claim certificate into `CertificateManager`
2. Connect to AWS IoT over TLS with claim credentials
3. Subscribe to `CreateCertificateFromCsr` response topics
4. Generate EC P-256 key pair + CSR (mbedTLS or PKCS11)
5. Publish CSR → receive signed device certificate from AWS
6. Publish to `RegisterThing` → receive permanent AWS IoT Thing Name
7. Persist Thing Name in NVS
8. Delete claim credentials (single-use — must not persist)
9. Restart → device enters normal operation on every boot thereafter

## Features

- **BLE provisioning** — uses `WiFiProvisioning` (BLE or SoftAP)
- **Custom BLE endpoint** — `AwsDataEndpointHandler` parses and stores AWS credentials
- **Certificate lifecycle** — `CertificateManager` handles import, generation, and deletion
- **Fleet Provisioning** — `AwsFleetProvisioner<T>` implements the full AWS workflow
- **Duck-typed MQTT** — `ProvisioningMqttAdapter` adapts `CoreMqttClient` + `MbedTlsTransport` to the
  interface required by the provisioner template parameter
- **Retry with backoff** — automatic retry configurable via `setRetries(count, delaySeconds)`
- **Idempotent boot** — checks NVS for existing `thing_name`; skips provisioning if already done

## Prerequisites

- ESP32 with BLE support (ESP32, ESP32-S3, etc.)
- AWS account with:
    - IoT Core Fleet Provisioning template (e.g. `GrowBorgTemplate`)
    - Claim certificate + key with a policy allowing `iot:CreateCertificateFromCsr` and `iot:RegisterThing`
- Mobile app capable of ESP-IDF BLE provisioning + custom JSON endpoint:
    - [ESP RainMaker iOS/Android](https://github.com/espressif/esp-rainmaker) — for standard WiFi provisioning
      only (does not support custom endpoints out of the box)
    - Custom React Native / Flutter app using
      [esp-idf-provisioning-android](https://github.com/espressif/esp-idf-provisioning-android) or
      [esp-idf-provisioning-ios](https://github.com/espressif/esp-idf-provisioning-ios) SDK

## Configuration

Edit the constants at the top of `main/main.cpp`:

```cpp
// BLE device name visible to mobile apps
static const char *BLE_SERVICE_NAME = "PROV_GROWBORG";

// AWS IoT endpoint (optional compile-time fallback; app can override via BLE)
static const char *DEFAULT_AWS_ENDPOINT = "xxx.iot.us-east-1.amazonaws.com";

// CSR subject name embedded in the device certificate
static const char *CSR_SUBJECT_NAME = "CN=GrowBorgDevice";
```

For production, switch to **SECURITY_1** in `sdkconfig.defaults` and set a proof-of-possession:

```cpp
// In main.cpp WiFiProvisioningConfig setup:
wifiProvConfig
    .setSecurity(ProvisioningSecurity::SECURITY_1)
    .setProofOfPossession("abcd1234");
```

To use **PKCS11 hardware-backed keys** (recommended for production):

```cpp
CertificateManager::Config cmConfig;
cmConfig.usePkcs11    = true;
cmConfig.pkcs11Backend = CertificateManager::Pkcs11Backend::AWS_CORE_PKCS11;
```

And enable `CONFIG_LOPCORE_PROV_CERT_COREP11=y` in `sdkconfig.defaults`.

## Key lopcore APIs Used

| API                      | Purpose                                                                |
| ------------------------ | ---------------------------------------------------------------------- |
| `WiFiProvisioningConfig` | Configure BLE transport, security, service name, custom endpoints      |
| `WiFiProvisioning`       | Wrap `wifi_prov_mgr`: `init()`, `start()`, `isProvisioned()`, `stop()` |
| `AwsDataEndpointConfig`  | Map AWS credential field names to `StorageCallbacks`                   |
| `AwsDataEndpointHandler` | Parse incoming JSON payload; persist to NVS via callbacks              |
| `CertificateManager`     | Import claim cert, generate key pair + CSR, delete credentials         |
| `AwsProvisioningConfig`  | All AWS provisioning parameters + storage bindings                     |
| `AwsFleetProvisioner<T>` | Execute full Fleet Provisioning workflow (blocking)                    |
| `nvsStorage(nvs)`        | Create `StorageCallbacks` wrapping a `NvsStorage` instance             |

## Building

```bash
# Set IDF target (ESP32-S3 shown; adjust for your board)
idf.py set-target esp32s3

# Configure (review sdkconfig.defaults)
idf.py menuconfig

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

## Expected Serial Output

**First boot (not provisioned):**

```
I (350) prov_example: ===========================================
I (355) prov_example: LopCore BLE Fleet Provisioning Example
I (360) prov_example:   Device ID: AABBCCDDEEFF
I (365) prov_example: ===========================================
I (370) prov_example: Device not provisioned — starting BLE provisioning flow
I (375) prov_example: ===========================================
I (380) prov_example: Phase 1: BLE WiFi Provisioning
I (385) prov_example: ===========================================
I (390) prov_example: BLE provisioning started — device name: PROV_GROWBORG
I (395) prov_example: Open your provisioning app and connect to "PROV_GROWBORG"
...
I (xxxxx) prov_example: WiFi credentials received! Device is connected to WiFi.
I (xxxxx) prov_example: ===========================================
I (xxxxx) prov_example: Phase 2: AWS Fleet Provisioning
I (xxxxx) prov_example: ===========================================
I (xxxxx) prov_example:   Endpoint:  xxx.iot.us-east-1.amazonaws.com
I (xxxxx) prov_example:   Template:  GrowBorgTemplate
I (xxxxx) prov_example:   Importing claim certificate...
I (xxxxx) prov_example:   Claim certificate imported OK
I (xxxxx) prov_example:   Starting fleet provisioning workflow...
...
I (xxxxx) prov_example:   ✓ Fleet provisioning succeeded!
I (xxxxx) prov_example:   Thing Name : GrowBorg-AABBCCDDEEFF
I (xxxxx) prov_example:   Deleting claim credentials (security hygiene)...
I (xxxxx) prov_example:   Device is permanently provisioned. Restarting...
```

**Subsequent boots (already provisioned):**

```
I (350) prov_example: Device already provisioned as: GrowBorg-AABBCCDDEEFF
I (355) prov_example: Normal Operation
I (360) prov_example:   Thing Name:   GrowBorg-AABBCCDDEEFF
I (365) prov_example:   AWS Endpoint: xxx.iot.us-east-1.amazonaws.com
```

## Factory Reset

To re-provision the device, erase NVS:

```bash
idf.py -p /dev/ttyUSB0 erase-flash
```

Or call `certManager->deleteDeviceCredentials()` and erase the `prov_aws` NVS namespace from application code.

## Architecture Notes

### ProvisioningMqttAdapter

The `AwsFleetProvisioner<T>` uses duck typing (C++ templates, no virtual interface) and expects the MQTT type
`T` to expose raw C-string `connect/disconnect/subscribe/publish/processEvents` methods.

During fleet provisioning the device only has the claim credentials as **in-memory PEM strings**. lopcore's
standard `TlsConfig` references PKCS#11 labels (written by `CertificateManager` after provisioning), so it
cannot be used here. Instead, `ProvisioningMqttAdapter` wraps the ESP-IDF native `esp_mqtt_client` API, which
directly accepts PEM strings in `esp_mqtt_client_config_t.broker.verification.certificate` and
`credentials.authentication.certificate/key`.

```
AwsFleetProvisioner<ProvisioningMqttAdapter>
         │
         └── ProvisioningMqttAdapter
                  └── esp_mqtt_client_handle_t   ← ESP-IDF MQTT with in-memory PEM
```

After provisioning succeeds, normal application MQTT uses the full lopcore `CoreMqttClient` +
`MbedtlsTransport` + PKCS#11 stack with the permanent device certificate stored by
`CertificateManager::storeFinalCertificate()`.

If you want to mock or test the Fleet Provisioning workflow without real hardware, implement the same six
methods in a `MockMqttClient` and instantiate `AwsFleetProvisioner<MockMqttClient>`.

### Storage Layout

| NVS Namespace | Key                     | Written by               | Purpose                     |
| ------------- | ----------------------- | ------------------------ | --------------------------- |
| `prov_aws`    | `claim_cert`            | `AwsDataEndpointHandler` | Claim certificate PEM       |
| `prov_aws`    | `claim_key`             | `AwsDataEndpointHandler` | Claim private key PEM       |
| `prov_aws`    | `aws_endpoint`          | `AwsDataEndpointHandler` | AWS IoT endpoint URL        |
| `prov_aws`    | `root_ca`               | `AwsDataEndpointHandler` | Root CA certificate PEM     |
| `prov_aws`    | `provisioning_template` | `AwsDataEndpointHandler` | Fleet Provisioning template |
| `prov_aws`    | `thing_name`            | `AwsFleetProvisioner`    | Permanent AWS Thing Name    |
