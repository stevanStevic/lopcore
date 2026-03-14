# Example 07 — BLE + AWS Fleet Provisioning

This example demonstrates a best-practice application lifecycle managed by lopcore's
`StateMachine<ApplicationState>`. A device with no prior configuration advertises over
BLE, receives WiFi credentials and AWS IoT claim credentials from `esp_prov.py` (or a
mobile app), then self-registers with AWS IoT Core using Fleet Provisioning.

## lopcore Modules Used

| Module | Where used | Purpose |
|--------|-----------|---------|
| `Logger` + `ConsoleSink` | `main.cpp` | Multi-sink logger init |
| `NvsStorage` | `main.cpp`, `INIT`, `CONFIGURATION` | Provisioning data persistence |
| `StateMachine<T>` + `IState<T>` | `application_state_machine` | Full application lifecycle |
| Transition rules | `application_state_machine.cpp` | Enforce valid state transitions |
| State observer | `application_state_machine.cpp` | Log every transition with elapsed time |
| `WiFiProvisioning` | `provisioning/ble_helper` | BLE transport + `wifi_prov_mgr` |
| `AwsDataEndpointHandler` | `provisioning/ble_helper` | Custom BLE endpoint for AWS credentials |
| `CertificateManager` | `provisioning/aws_helper` | Claim cert import, key gen, CSR |
| `AwsFleetProvisioner<T>` | `provisioning/aws_helper` | Fleet Provisioning workflow |
| `CoreMqttClient` + `MbedtlsTransport` | `provisioning/provisioning_mqtt_adapter` | MQTT for provisioning |
| `nvsStorage()` helper | `ble_helper`, `aws_helper` | `StorageCallbacks` from NVS |

---

## Architecture

### State Machine

```
                    ┌──────────────────────────────────────────────────────┐
                    │                                                        │
                    ▼                                                        │
  ┌──────┐  no   ┌──────────────────┐  success  ┌─────────┐               │
  │ INIT │──────►│  CONFIGURATION   │──────────►│ NOMINAL │               │
  └──────┘  prov  └──────────────────┘           └─────────┘               │
     │             │                               │     │                  │
     │  provisioned│ max retries                   │wifi │                  │
     └─────────────┘ exceeded                      │lost │   BOOT button   │
                    │                               ▼     │   (GPIO_NUM_0)  │
                    │            ┌──────────────────────┐  │                │
                    │            │       DEGRADED        │  │                │
                    │            └──────────────────────┘  │                │
                    │              │ timeout  │ reconnect   │                │
                    │              ▼          └────────────►│NOMINAL         │
                    │            INIT                       │                │
                    │                                       │                │
                    └───────────────────────────────────────►               │
                                                            ▼                │
                                                   ┌──────────────────┐     │
                                                   │  FACTORY_RESET   │─────┘
                                                   └──────────────────┘
                                                   (erases NVS, restarts)
```

### Transition Rules

| From | To | Trigger |
|------|----|---------|
| `INIT` | `CONFIGURATION` | Not provisioned (no `thing_name` in NVS) |
| `INIT` | `NOMINAL` | Already provisioned |
| `CONFIGURATION` | `NOMINAL` | Fleet Provisioning succeeded |
| `CONFIGURATION` | `FACTORY_RESET` | Max retries exceeded |
| `NOMINAL` | `DEGRADED` | WiFi connection lost |
| `NOMINAL` | `FACTORY_RESET` | BOOT button pressed |
| `DEGRADED` | `NOMINAL` | WiFi reconnected |
| `DEGRADED` | `INIT` | Reconnect timeout (30s) |

### CONFIGURATION — Internal Phase Sequence

The provisioning flow is a manual phase switch inside `ConfigurationState` (not a nested
`StateMachine`) — the provisioning phases are an implementation detail, not the FSM showcase:

```
BLE_PROVISIONING  ──(complete)──►  AWAITING_WIFI (2s)  ──►  AWS_PROVISIONING  ──(success)──►
SUCCESS_CLEANUP  ──►  sm->transition(NOMINAL)

BLE_PROVISIONING  ──(timeout/fail)──►  FAILED_RETRY_WAIT  ──(retry)──►  BLE_PROVISIONING
                                                            ──(max retries)──►  FACTORY_RESET
```

### State Observer

Every state transition is logged automatically:

```
[FSM] INIT → CONFIGURATION  (+412ms)
[FSM] CONFIGURATION → NOMINAL  (+47823ms)
[FSM] NOMINAL → DEGRADED  (+120034ms)
[FSM] DEGRADED → INIT  (+30001ms)
```

---

## Prerequisites

### Hardware

- ESP32 or ESP32-S3 with BLE support
- Optional: external button on `GPIO_NUM_0` (or use the on-board BOOT button)

### AWS Setup

You need an AWS account with IoT Core access. Run these commands once:

```bash
# 1. Get your AWS IoT endpoint
aws iot describe-endpoint --endpoint-type iot:Data-ATS \
  --query endpointAddress --output text
# Output: xxxx.iot.us-east-1.amazonaws.com

# 2. Create the device policy (attach to claim certificates)
cat > claim_policy.json << 'EOF'
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": [
        "iot:Connect",
        "iot:Publish",
        "iot:Subscribe",
        "iot:Receive",
        "iot:CreateCertificateFromCsr",
        "iot:RegisterThing"
      ],
      "Resource": "*"
    }
  ]
}
EOF

aws iot create-policy \
  --policy-name ExampleClaimPolicy \
  --policy-document file://claim_policy.json

# 3. Create the Fleet Provisioning template
cat > fleet_provisioning_template.json << 'EOF'
{
  "Parameters": {
    "DeviceId": { "Type": "String" },
    "AWS::IoT::Certificate::Id": { "Type": "String" }
  },
  "Resources": {
    "certificate": {
      "Properties": {
        "CertificateId": {"Ref": "AWS::IoT::Certificate::Id"},
        "Status": "Active"
      },
      "Type": "AWS::IoT::Certificate"
    },
    "policy": {
      "Properties": { "PolicyName": "ExampleClaimPolicy" },
      "Type": "AWS::IoT::Policy"
    },
    "thing": {
      "OverrideSettings": {
        "AttributePayload": "REPLACE",
        "ThingGroups": "REPLACE",
        "ThingTypeName": "REPLACE"
      },
      "Properties": {
        "ThingName": {"Fn::Join": ["-", ["device", {"Ref": "DeviceId"}]]},
        "ThingGroups": []
      },
      "Type": "AWS::IoT::Thing"
    }
  }
}
EOF

aws iot create-provisioning-template \
  --template-name ExampleFleetTemplate \
  --template-body file://fleet_provisioning_template.json \
  --provisioning-role-arn arn:aws:iam::YOUR_ACCOUNT_ID:role/IoTFleetProvisioningRole \
  --enabled

# 4. Create claim credentials (one-time use; burn into each device before deployment)
aws iot create-keys-and-certificate \
  --set-as-active \
  --certificate-pem-outfile claim_cert.pem \
  --public-key-outfile claim_public.key \
  --private-key-outfile claim_private.key

# 5. Attach the claim policy to the certificate
CERT_ARN=$(aws iot list-certificates --query 'certificates[0].certificateArn' --output text)
aws iot attach-policy --policy-name ExampleClaimPolicy --target "$CERT_ARN"
```

### Python Setup (for esp_prov.py)

```bash
pip install bleak protobuf cryptography boto3
aws configure   # or set AWS_ACCESS_KEY_ID / AWS_SECRET_ACCESS_KEY / AWS_DEFAULT_REGION
```

---

## Building & Flashing

```bash
source $IDF_PATH/export.sh
cd components/lopcore/examples/07_ble_fleet_provisioning

idf.py set-target esp32s3   # or esp32

# Optional: idf.py menuconfig
# Component config → Bluetooth → Enable Bluetooth
# Component config → LopCore Configuration → Enable Provisioning module
#   → Enable BLE transport

idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## Running esp_prov.py

`esp_prov.py` is the desktop equivalent of a mobile provisioning app. It connects to the
device over BLE and sends WiFi credentials plus AWS IoT claim credentials in the correct order.

### Why Order Matters

```
ORDERING REQUIREMENT: AWS credentials MUST be sent before WiFi credentials.

Once wifi_prov_mgr receives WiFi credentials and the device connects
(WIFI_PROV_CRED_SUCCESS), WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
tears down the BLE transport immediately. Any AWS data sent after WiFi
credentials arrives into a dead transport and is lost.

esp_prov.py enforces the correct order automatically.
```

### Running the Script

```bash
cd scripts/

python3 esp_prov.py \
  --transport ble \
  --service_name PROV_EXAMPLE \
  --sec_ver 0 \
  --ssid "YourWiFiSSID" \
  --passphrase "YourWiFiPassword" \
  --template_name "ExampleFleetTemplate" \
  --aws_endpoint "xxxx.iot.us-east-1.amazonaws.com" \
  --aws_region "us-east-1" \
  --claim_cert /path/to/claim_cert.pem \
  --claim_key /path/to/claim_private.key \
  --root_ca /path/to/AmazonRootCA1.pem
```

**Flag reference:**

| Flag | Description |
|------|-------------|
| `--transport ble` | Use BLE (alternative: `softap`) |
| `--service_name` | Must match `BLE_SERVICE_NAME` in `main.cpp` (default: `PROV_EXAMPLE`) |
| `--sec_ver 0` | Security level — see table below |
| `--ssid` / `--passphrase` | WiFi network to connect the device to |
| `--template_name` | Must match the Fleet Provisioning template name in AWS |
| `--aws_endpoint` | Your AWS IoT Data endpoint (from `describe-endpoint` above) |
| `--aws_region` | AWS region where your IoT Core is configured |
| `--claim_cert` | Path to claim certificate PEM |
| `--claim_key` | Path to claim private key PEM |
| `--root_ca` | Path to Amazon Root CA 1 PEM |

### Security Levels

| Level | Flag | Description | Use case |
|-------|------|-------------|----------|
| 0 | `--sec_ver 0` | No encryption | Development only |
| 1 | `--sec_ver 1` | X25519 key exchange + AES-CTR | Production |
| 2 | `--sec_ver 2` | SRP6a + AES-GCM | Production (mutual auth) |

### Expected Output

```
Connecting to PROV_EXAMPLE over BLE...
Sending AWS data endpoint...
  certificate: [claim_cert.pem contents]
  private_key: [claim_private.key contents]
  aws_endpoint: xxxx.iot.us-east-1.amazonaws.com
  root_ca: [AmazonRootCA1.pem contents]
  provisioning_template: ExampleFleetTemplate
AWS data sent successfully

Sending WiFi credentials...
  SSID: YourWiFiSSID
WiFi credentials sent

Waiting for provisioning to complete...
Provisioning successful!
```

On the device serial monitor you will see:

```
[FSM] INIT → CONFIGURATION  (+1ms)
CONFIGURATION: BLE + AWS Provisioning
  BLE advertising as: PROV_EXAMPLE
  Use esp_prov.py or a mobile app to provision.
  [BLE] WiFi + AWS credentials received!
  [WiFi] Stabilization complete
  [AWS] Running Fleet Provisioning workflow...
  Fleet provisioning succeeded!
  Thing Name: device-AABBCCDDEEFF
[FSM] CONFIGURATION → NOMINAL  (+52341ms)
NOMINAL: Device provisioned and ready
  Thing Name:   device-AABBCCDDEEFF
  AWS Endpoint: xxxx.iot.us-east-1.amazonaws.com
  Device ID:    AABBCCDDEEFF
```

---

## Factory Reset Button

The BOOT button (`GPIO_NUM_0`) triggers a factory reset when pressed. This erases the
`prov_aws` NVS namespace and restarts the device, returning it to the unprovisioned state.

**To remap the button:**

In `main.cpp`, change:
```cpp
static constexpr gpio_num_t FACTORY_RESET_GPIO = GPIO_NUM_0;
```

**For production use:** Add a debounce filter (e.g., 50ms) and a hold-duration check
(e.g., 3s) before triggering the reset. The current implementation triggers on any falling edge.

---

## Expected Serial Output

### First Boot (no provisioning data)

```
===========================================
LopCore BLE Fleet Provisioning Example
===========================================
Device ID: AABBCCDDEEFF
State machine ready. Starting in INIT...

[INIT] Checking provisioning status...
[INIT] Device not provisioned
[INIT] Transition → CONFIGURATION
[FSM] INIT → CONFIGURATION  (+1ms)

===========================================
CONFIGURATION: BLE + AWS Provisioning
===========================================
Starting Phase 1: BLE WiFi Provisioning (attempt 1 of 3)
  BLE advertising as: PROV_EXAMPLE
  Use esp_prov.py or a mobile app to provision.
```

### Second Boot (already provisioned)

```
===========================================
LopCore BLE Fleet Provisioning Example
===========================================
Device ID: AABBCCDDEEFF
State machine ready. Starting in INIT...

[INIT] Checking provisioning status...
[INIT] Device already provisioned as: device-AABBCCDDEEFF
[INIT] Transition → NOMINAL
[FSM] INIT → NOMINAL  (+3ms)

===========================================
NOMINAL: Device provisioned and ready
===========================================
  Thing Name:   device-AABBCCDDEEFF
  AWS Endpoint: xxxx.iot.us-east-1.amazonaws.com
  Device ID:    AABBCCDDEEFF

  For MQTT integration see:
    examples/04_mqtt_esp_client     -- EspMqttClient patterns
    examples/06_mqtt_coremqtt_sync  -- CoreMqttClient + AWS IoT
  [heartbeat] heap: 234512 bytes  uptime: 0s
```

### Factory Reset (button press)

```
[button] Factory reset triggered via GPIO 0
[FSM] NOMINAL → FACTORY_RESET  (+15023ms)

===========================================
FACTORY_RESET: Erasing device configuration
===========================================

Provisioning failed after 0 attempts
Last error:
  Erased AWS provisioning config

Factory reset complete. Restarting in 3 seconds...
RESTARTING...
```

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| BLE not found by esp_prov.py | BLE not enabled in menuconfig | `idf.py menuconfig` → Component config → Bluetooth → Enable Bluetooth |
| `service_name not found` | Wrong `--service_name` flag | Must match `BLE_SERVICE_NAME` in `main.cpp` (default: `PROV_EXAMPLE`) |
| AWS provisioning fails: auth error | Claim policy not attached | Attach `ExampleClaimPolicy` to the claim certificate in AWS Console |
| AWS provisioning fails: template | Template name mismatch | Confirm `--template_name` matches the AWS template name exactly |
| WiFi accepted but AWS data lost | Wrong sending order | Use `esp_prov.py` — it sends AWS data before WiFi credentials automatically |
| `nvs_flash_init` fails | Full or corrupted NVS | Hold BOOT button at startup to trigger factory reset, or `idf.py erase-flash` |
| `Failed to initialize NVS namespace` | NVS partition missing | Check `partitions.csv` includes NVS partition |
| Device stuck in DEGRADED | WiFi credentials lost after reset | The 30s timeout will return to INIT → CONFIGURATION |

---

## NVS Storage Layout

All provisioning data stored in namespace `prov_aws`:

| BLE JSON field | NVS key | Written by | Deleted by | Purpose |
|---------------|---------|-----------|-----------|---------|
| `certificate` | `claim_cert` | `AwsDataEndpointHandler` | `deleteClaimCredentials()` | Claim certificate PEM |
| `private_key` | `claim_key` | `AwsDataEndpointHandler` | `deleteClaimCredentials()` | Claim private key PEM |
| `aws_endpoint` | `aws_endpoint` | `AwsDataEndpointHandler` | — | AWS IoT endpoint URL |
| `root_ca` | `root_ca` | `AwsDataEndpointHandler` | — | Root CA certificate PEM |
| `provisioning_template` | `provisioning_template` | `AwsDataEndpointHandler` | — | Fleet Provisioning template name |
| *(set by provisioner)* | `thing_name` | `AwsFleetProvisioner` | — | Permanent Thing Name (success marker) |

Claim credentials (`claim_cert`, `claim_key`) are deleted from NVS after successful Fleet
Provisioning as a security hygiene measure — they are single-use and should not persist.
