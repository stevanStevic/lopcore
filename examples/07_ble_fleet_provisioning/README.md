# BLE + AWS Fleet Provisioning Example (State Machine Version)

This example demonstrates **professional application architecture** using a state machine to manage the
complete **zero-touch device provisioning** flow. WiFi credentials and AWS IoT claim credentials are delivered
over BLE, and the device then self-registers with AWS IoT using Fleet Provisioning.

Unlike simple procedural examples, this uses a **state machine pattern** (mirroring professional IoT systems)
to separate concerns, enable retry logic, and provide a foundation for extensions (OTA updates, degraded mode,
etc.).

## Architecture Overview

### Application State Machine

The device lifecycle is managed by an application state machine with four states:

```
┌─────────────────────────────────────────────────────────┐
│  INIT                                                   │
│  - Check NVS for provision status (thing_name)          │
│  - Route to CONFIGURATION or NOMINAL                    │
└─────────────────────────────────────────────────────────┘
              ↓
    ┌────────────────────────┐
    │ Provisioned?           │
    ├─── Yes ───→ NOMINAL    │
    └─── No  ───→ CONFIGURATION
              ↓
┌─────────────────────────────────────────────────────────┐
│  CONFIGURATION (Mini State Machine Inside)              │
│  ┌───────────────────────────────────────────────────┐  │
│  │ Phases:                                           │  │
│  │  1. BLE_PROVISIONING — wait for WiFi + AWS creds │  │
│  │  2. AWAITING_WIFI — brief stabilization (2 sec)  │  │
│  │  3. AWS_PROVISIONING — Fleet Provisioning MQTT   │  │
│  │  4. SUCCESS_CLEANUP — delete claim creds         │  │
│  │  5. FAILED_RETRY_WAIT — retry on failure         │  │
│  │  6. ABORTED — max retries exceeded               │  │
│  └───────────────────────────────────────────────────┘  │
│  On success → NOMINAL                                   │
│  On max retries → FACTORY_RESET                         │
└─────────────────────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────────────────────┐
│  NOMINAL                                                │
│  - System provisioned and ready                         │
│  - App-level idle loop (placeholder for MQTT, etc.)     │
│  - Runs forever                                         │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  FACTORY_RESET (Triggered on max retries or error)      │
│  - Erase NVS provisioning namespace                     │
│  - Call esp_restart() → back to INIT                    │
└─────────────────────────────────────────────────────────┘
```

### Why a State Machine?

1. **Clear structure** — Easier to understand and extend than nested if/else
2. **Encapsulation** — Provisioning logic is contained in CONFIGURATION state
3. **Professional pattern** — Mirrors industrial application architectures
4. **Retry logic** — Integrated into state machine, not scattered
5. **Extensible** — Easy to add new states (DEGRADED, OTA, etc.)

### Two-Phase Provisioning Inside CONFIGURATION State

Once in the CONFIGURATION state, the device orchestrates two phases with an internal mini state machine:

#### **Phase 1 — BLE WiFi + AWS Credential Transfer** (BLE_PROVISIONING phase)

The lopcore `WiFiProvisioning` class wraps the ESP-IDF `wifi_prov_mgr` and exposes two channels:

- **Standard provisioning channel** — mobile app sends WiFi SSID + password (ESP-IDF BLE protocol)
- **Custom `"aws-data"` endpoint** — mobile app sends AWS credentials as a JSON payload:

```json
{
    "certificate": "<claim cert PEM>",
    "private_key": "<claim key PEM>",
    "aws_endpoint": "xxx.iot.us-east-1.amazonaws.com",
    "root_ca": "<root CA PEM>",
    "provisioning_template": "ExampleTemplate"
}
```

The `AwsDataEndpointHandler` receives this payload and writes each field to NVS via `StorageCallbacks`.

**Timeout:** 5 minutes. If BLE provisioning fails or times out, transitions to FAILED_RETRY_WAIT.

#### **Phase 2 — AWS IoT Fleet Provisioning** (AWS_PROVISIONING phase)

Using the claim credentials stored in Phase 1:

1. Import claim certificate into `CertificateManager`
2. Connect to AWS IoT over TLS with claim credentials
3. Subscribe to `CreateCertificateFromCsr` response topics
4. Generate EC P-256 key pair + CSR (mbedTLS or PKCS11)
5. Publish CSR → receive signed device certificate from AWS
6. Publish to `RegisterThing` → receive permanent AWS IoT Thing Name
7. Persist Thing Name in NVS
8. Delete claim credentials (single-use — must not persist)
9. Transition to NOMINAL

**Timeout:** 5 minutes. If AWS provisioning fails or times out → FAILED_RETRY_WAIT.

#### **Retry Logic** (FAILED_RETRY_WAIT phase)

On any provisioning failure:

1. Log detailed error (which phase, why)
2. Wait 5 seconds
3. If retries remaining (default 3) → back to BLE_PROVISIONING (single transition)
4. If max retries exhausted → FACTORY_RESET (erase NVS and restart)

## Features

- **State machine architecture** — Professional lifecycle management
- **Automatic retry** — Configurable retries with exponential backoff
- **Comprehensive logging** — Per-state entry/exit, phase transitions, detailed errors
- **BLE provisioning** — uses `WiFiProvisioning` (BLE or SoftAP)
- **Custom BLE endpoint** — `AwsDataEndpointHandler` parses and stores AWS credentials
- **Certificate lifecycle** — `CertificateManager` handles import, generation, and deletion
- **Fleet Provisioning** — `AwsFleetProvisioner<T>` implements the full AWS workflow
- **Duck-typed MQTT** — `ProvisioningMqttAdapter` adapts `CoreMqttClient` + `MbedTlsTransport` to the
  interface required by the provisioner template parameter
- **Retry with backoff** — automatic retry configurable via `setRetries(count, delaySeconds)`
- **Idempotent boot** — checks NVS for existing `thing_name`; skips provisioning if already done

## Code Organization

### Main Files

| File                            | Purpose                                                             |
| ------------------------------- | ------------------------------------------------------------------- |
| `main.cpp`                      | App entry point (`app_main`), minimal (~80 lines)                   |
| `application_state_machine.hpp` | State enum, context struct, factory function                        |
| `application_state_machine.cpp` | State machine factory (registers all states)                        |
| `init_state.hpp/.cpp`           | InitState implementation (check provisioning status)                |
| `configuration_state.hpp/.cpp`  | ConfigurationState with internal phase machine (provisioning logic) |
| `nominal_state.hpp/.cpp`        | NominalState implementation (app-ready idle loop)                   |
| `factory_reset_state.hpp/.cpp`  | FactoryResetState implementation (erase NVS + restart)              |
| `provisioning_helpers.hpp/.cpp` | BLE & AWS provisioning helpers, ProvisioningMqttAdapter             |

### Key Classes

- **`ProvisioningContext`** (`application_state_machine.hpp`) — Shared state passed to all states
- **`ConfigurationState`** — Contains a mini state machine (Phase enum) for provisioning phases
- **`BleProvisioningHelper`** — Wraps Phase 1 (BLE WiFi + AWS credential delivery)
- **`AwsProvisioningHelper`** — Wraps Phase 2 (AWS Fleet Provisioning workflow)
- **`ProvisioningMqttAdapter`** — Duck-typed MQTT interface for provisioning

### State Machine Initialization

In `main()`:

```cpp
// Create context (shared state)
ProvisioningContext ctx;
ctx.awsNvs = awsNvs;
ctx.deviceId = getDeviceId();
// ... other config ...

// Create state machine
auto sm = createApplicationStateMachine(ctx);  // factory function
sm->initialize(ApplicationState::INIT);        // start in INIT state

// Main loop (100 ms cycle)
while (sm->isRunning()) {
    sm->update();
    vTaskDelay(pdMS_TO_TICKS(100));
}
```

## Prerequisites

- ESP32 with BLE support (ESP32, ESP32-S3, etc.)
- AWS account with:
    - IoT Core Fleet Provisioning template
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
static const char *BLE_SERVICE_NAME = "PROV_EXAMPLE";

// AWS IoT endpoint (optional compile-time fallback; app can override via BLE)
static const char *DEFAULT_AWS_ENDPOINT = "xxx.iot.us-east-1.amazonaws.com";

// CSR subject name embedded in the device certificate
static const char *CSR_SUBJECT_NAME = "CN=ExampleDevice";
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

### First Boot (Not Provisioned)

The device transitions through states: INIT → CONFIGURATION (BLE → AWS phases) → NOMINAL

```
I (350) prov_example: ===========================================
I (355) prov_example: LopCore BLE Fleet Provisioning Example
I (361) prov_example: State machine initialized. Entering main loop...
I
I (370) app_sm:init: ===========================================
I (375) app_sm:init: LopCore BLE Fleet Provisioning Example
I (380) app_sm:init: ===========================================
I (385) app_sm:init: Device ID: AABBCCDDEEFF
I (390) app_sm:init:
I (395) app_sm:init: [INIT] Checking provisioning status...
I (400) app_sm:init: [INIT] Device not provisioned
I (405) app_sm:init: [INIT] Transition → CONFIGURATION
I (410) app_sm:init: [INIT] Exit
I
I (415) app_sm:config: ===========================================
I (420) app_sm:config: CONFIGURATION: BLE + AWS Provisioning
I (425) app_sm:config: ===========================================
I (430) app_sm:config: Starting Phase 1: BLE WiFi Provisioning (attempt 1 of 3)
I (435) app_sm:config:   Starting BLE provisioning...
I (440) app_sm:config:   BLE advertising: PROV_EXAMPLE
I (445) app_sm:config:   Open your provisioning app and connect.
I (450) app_sm:config:   Phase transition: BLE_PROVISIONING → BLE_PROVISIONING
I (455) app_sm:config:   Waiting for WiFi provisioning...
...
I (xxxxx) provisioning_helpers:   BLE provisioning started — device name: PROV_EXAMPLE
I (xxxxx) app_sm:config:   [BLE] WiFi + AWS credentials received!
I (xxxxx) app_sm:config:   Phase transition: BLE_PROVISIONING → AWAITING_WIFI
...
I (xxxxx) app_sm:config:   Phase transition: AWAITING_WIFI → AWS_PROVISIONING
I (xxxxx) app_sm:config:   Starting AWS Fleet Provisioning...
I (xxxxx) provisioning_helpers:     Endpoint:  xxx.iot.us-east-1.amazonaws.com
I (xxxxx) provisioning_helpers:     Importing claim certificate...
I (xxxxx) provisioning_helpers:     Claim certificate imported OK
I (xxxxx) provisioning_helpers:     Running Fleet Provisioning workflow...
...
I (xxxxx) provisioning_helpers:     ✓ Succeeded!
I (xxxxx) provisioning_helpers:     Thing Name: device-AABBCCDDEEFF
I (xxxxx) app_sm:config:   ✓ Fleet provisioning succeeded!
I (xxxxx) app_sm:config:   Thing Name: device-AABBCCDDEEFF
I (xxxxx) app_sm:config:   Cleaning up: deleting claim credentials (security hygiene)...
I (xxxxx) provisioning_helpers:     Claim credentials deleted OK
I (xxxxx) app_sm:config:   Device provisioning complete!
I (xxxxx) app_sm:config:   Transitioning to NOMINAL state...
I (xxxxx) app_sm:config: [CONFIGURATION] Exit
I
I (xxxxx) app_sm:nominal: ===========================================
I (xxxxx) app_sm:nominal: NOMINAL: System Ready
I (xxxxx) app_sm:nominal: ===========================================
I (xxxxx) app_sm:nominal: Device is provisioned and ready for operation
I (xxxxx) app_sm:nominal:   Thing Name:   device-AABBCCDDEEFF
I (xxxxx) app_sm:nominal:   AWS Endpoint: xxx.iot.us-east-1.amazonaws.com
I (xxxxx) app_sm:nominal:
I (xxxxx) app_sm:nominal: TODO: Integrate your application logic here:
I (xxxxx) app_sm:nominal:   - Connect to AWS IoT Core MQTT
I (xxxxx) app_sm:nominal:   - Subscribe to device shadows
I (xxxxx) app_sm:nominal:   - Read sensors, publish telemetry
I (xxxxx) app_sm:nominal:   - Control LEDs, camera, etc.
I (xxxxx) app_sm:nominal:
I (xxxxx) app_sm:nominal: [idle 0] heap: 152000 bytes
```

### Subsequent Boots (Already Provisioned)

Direct transition: INIT → NOMINAL

```
I (350) prov_example: ===========================================
I (355) prov_example: LopCore BLE Fleet Provisioning Example
I (360) prov_example: State machine initialized. Entering main loop...
I
I (365) app_sm:init: [INIT] Checking provisioning status...
I (370) app_sm:init: [INIT] Device already provisioned as: device-AABBCCDDEEFF
I (375) app_sm:init: [INIT] Transition → NOMINAL
I (380) app_sm:init: [INIT] Exit
I
I (385) app_sm:nominal: [NOMINAL] System Ready
I (390) app_sm:nominal:   Thing Name:   device-AABBCCDDEEFF
I (395) app_sm:nominal:   AWS Endpoint: xxx.iot.us-east-1.amazonaws.com
...
```

### Provisioning Failure with Retry

If BLE times out, transitions to FAILED_RETRY_WAIT, then back to BLE_PROVISIONING:

```
I (xxxxx) app_sm:config: [BLE] Timeout! — transitioning to FAILED_RETRY_WAIT
I (xxxxx) app_sm:config:
I (xxxxx) app_sm:config:   ✗ Provisioning failed at phase: BLE_PROVISIONING
I (xxxxx) app_sm:config:   Reason: BLE provisioning timeout
I (xxxxx) app_sm:config:   Phase transition: BLE_PROVISIONING → FAILED_RETRY_WAIT
I (xxxxx) app_sm:config:   Waiting 5000 ms before retry...
I (xxxxx) app_sm:config:   Retrying provisioning (attempt 2 of 3)
I (xxxxx) app_sm:config:   Phase transition: FAILED_RETRY_WAIT → BLE_PROVISIONING
...
```

### Max Retries Exceeded → Factory Reset

```
I (xxxxx) app_sm:config:   ✗ Max retries exceeded! Transitioning to FACTORY_RESET
I (xxxxx) app_sm:factory_reset: ===========================================
I (xxxxx) app_sm:factory_reset: FACTORY_RESET: Erasing device configuration
I (xxxxx) app_sm:factory_reset: ===========================================
I (xxxxx) app_sm:factory_reset:
I (xxxxx) app_sm:factory_reset: Provisioning failed after 3 attempts
I (xxxxx) app_sm:factory_reset: Last error: AWS provisioning timeout
I (xxxxx) app_sm:factory_reset:
I (xxxxx) app_sm:factory_reset: Erasing NVS namespaces...
I (xxxxx) app_sm:factory_reset:   ✓ Erased AWS provisioning config
I (xxxxx) app_sm:factory_reset:
I (xxxxx) app_sm:factory_reset: Factory reset complete. Restarting in 3 seconds...
I (xxxxx) app_sm:factory_reset:
I (xxxxx) app_sm:factory_reset: RESTARTING...
I (xxxxx) app_sm:factory_reset:
(device restarts)
```

## Factory Reset

To re-provision the device, trigger a factory reset:

**Option 1:** Erase NVS via terminal

```bash
idf.py -p /dev/ttyUSB0 erase-flash
idf.py -p /dev/ttyUSB0 flash monitor
```

**Option 2:** Let provisioning fail max retries (automatic factory reset)

The CONFIGURATION state will automatically transition to FACTORY_RESET after 3 failed attempts.

## Understanding the State Machine

### State Transitions Diagram

```
INIT (check NVS)
  ├─ provisioned?  → NOMINAL (forever)
  └─ not provisioned? → CONFIGURATION

CONFIGURATION (provisioning loop with retry)
  ├─ BLE_PROVISIONING
  │   ├─ WiFi + AWS creds received? → AWAITING_WIFI
  │   └─ Timeout (5 min)? → FAILED_RETRY_WAIT
  │
  ├─ AWAITING_WIFI (2 sec delay)
  │   └─ Done? → AWS_PROVISIONING
  │
  ├─ AWS_PROVISIONING
  │   ├─ thing_name received? → SUCCESS_CLEANUP
  │   └─ Failed or timeout (5 min)? → FAILED_RETRY_WAIT
  │
  ├─ SUCCESS_CLEANUP
  │   └─ Delete claim creds → NOMINAL
  │
  └─ FAILED_RETRY_WAIT (5 sec)
      ├─ Retries remaining? → BLE_PROVISIONING (loop back)
      └─ Max retries exceeded? → FACTORY_RESET

FACTORY_RESET
  └─ Erase NVS → esp_restart() → INIT
```

### Customizing CONFIGURATION State Behavior

Edit the timeout and retry parameters in `ProvisioningContext`:

```cpp
// In main.cpp, before creating the state machine:
ctx.config.maxRetries = 5;                        // default 3
ctx.config.ble_timeout_ms = 600000;               // default 5 min (300 sec)
ctx.config.aws_timeout_ms = 600000;               // default 5 min
ctx.config.wifi_stabilization_ms = 3000;          // default 2 sec
```

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
