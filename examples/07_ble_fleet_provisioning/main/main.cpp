/**
 * @file main.cpp
 * @brief BLE + AWS Fleet Provisioning Example
 *
 * Demonstrates the full "zero-touch" device provisioning flow using lopcore:
 *
 *  Phase 1 — WiFi + AWS credentials via BLE
 *  ────────────────────────────────────────
 *  1. Start BLE advertising (WiFiProvisioning)
 *  2. Mobile app connects and transfers:
 *       - WiFi SSID + password  (standard ESP-IDF provisioning protocol)
 *       - AWS claim certificate + key PEM
 *       - AWS IoT endpoint URL
 *       - Root CA certificate
 *       - Fleet provisioning template name
 *     via custom "aws-data" BLE endpoint (AwsDataEndpointHandler)
 *  3. ESP-IDF wifi_prov_mgr connects to WiFi automatically
 *  4. BLE / SoftAP interface shuts down
 *
 *  Phase 2 — AWS IoT Fleet Provisioning
 *  ──────────────────────────────────────
 *  5. Import claim certificate into CertificateManager
 *  6. Connect to AWS IoT using claim credentials (TLS)
 *  7. Subscribe to CreateCertificateFromCsr response topics
 *  8. Generate EC P-256 key pair + CSR (mbedTLS or PKCS11)
 *  9. Send CSR → receive signed device certificate from AWS
 * 10. Publish to RegisterThing → receive AWS IoT Thing Name
 * 11. Store device certificate + Thing Name in NVS
 * 12. Delete claim credentials (security hygiene)
 * 13. Device is now permanently provisioned — restart to normal operation
 *
 * Prerequisites:
 *   - Flash claim certificate + key via sdkconfig or include them as embedded
 *     binary, or provision them from the mobile app over BLE (this example).
 *   - AWS IoT Fleet Provisioning template created in your AWS account.
 *   - Mobile app that speaks ESP-IDF provisioning BLE protocol + custom JSON
 *     endpoint (e.g. ESP RainMaker app, or a custom React Native app).
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#include <cstring>
#include <memory>
#include <optional>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

// lopcore — logging
#include "lopcore/logging/console_sink.hpp"
#include "lopcore/logging/logger.hpp"

// lopcore — storage
#include "lopcore/storage/nvs_storage.hpp"
#include "lopcore/storage/storage_config.hpp"

// lopcore — provisioning
#include "lopcore/prov/aws_data_endpoint_handler.hpp"
#include "lopcore/prov/aws_fleet_provisioner.hpp"
#include "lopcore/prov/certificate_manager.hpp"
#include "lopcore/prov/provisioning_config.hpp"
#include "lopcore/prov/storage_helpers.hpp"
#include "lopcore/prov/wifi_provisioning.hpp"

// ESP-IDF — MQTT / TLS used inside ProvisioningMqttAdapter
// The provisioning adapter uses raw esp_mqtt_client_config_t because lopcore's
// TlsConfig references PKCS#11 labels, not in-memory PEM strings — and at
// provisioning time the claim credentials are only available as PEM strings
// (they have not yet been imported into PKCS#11).
#include "mqtt_client.h"

// ESP-IDF
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "prov_example";

// ============================================================================
// Configuration — edit these for your environment
// ============================================================================

// BLE device name visible to mobile apps
static const char *BLE_SERVICE_NAME = "PROV_GROWBORG";

// NVS namespaces (max 15 characters)
// NVS_NS_WIFI could be used with WiFiProvisioningConfig::setWiFiStorage() to
// persist WiFi credentials in a custom namespace — here we rely on the
// default ESP-IDF wifi_prov_mgr storage (namespace "nvs.net80211").
static const char *NVS_NS_AWS = "prov_aws"; // AWS config + claim creds namespace

// AWS IoT endpoint (may also be pushed via BLE custom endpoint)
// Leave empty to require the mobile app to provide it
static const char *DEFAULT_AWS_ENDPOINT = ""; // e.g. "xxx.iot.us-east-1.amazonaws.com"

// CSR subject name embedded in the device certificate
static const char *CSR_SUBJECT_NAME = "CN=GrowBorgDevice";

// ============================================================================
// Helper — derive a device-unique client ID from the MAC address
// ============================================================================

static std::string getDeviceId()
{
    uint8_t mac[6] = {};
    esp_base_mac_addr_get(mac);
    char id[18];
    snprintf(id, sizeof(id), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(id);
}

// ============================================================================
// ProvisioningMqttAdapter
//
// Thin adapter that satisfies the duck-typed interface expected by
// AwsFleetProvisioner<T>.  It wraps the ESP-IDF MQTT client configured with
// in-memory PEM credentials — necessary because lopcore's TlsConfig references
// PKCS#11 labels, which are only available AFTER fleet provisioning completes.
//
// Required interface (duck-typed by AwsFleetProvisioner):
//   bool connect(endpoint, port, clientId, certPem, keyPem, rootCaPem)
//   bool disconnect()
//   bool subscribe(topic, callback<const char*, size_t>)
//   bool publish(topic, payload, length)
//   bool isConnected() const
//   void processEvents(timeoutMs)
// ============================================================================

// Maximum number of topics the provisioner subscribes to (4 in practice)
static constexpr int PROV_MAX_TOPICS = 8;

class ProvisioningMqttAdapter
{
public:
    ProvisioningMqttAdapter() : handle_(nullptr), connected_(false)
    {
    }

    ~ProvisioningMqttAdapter()
    {
        disconnect();
    }

    // Non-copyable
    ProvisioningMqttAdapter(const ProvisioningMqttAdapter &) = delete;
    ProvisioningMqttAdapter &operator=(const ProvisioningMqttAdapter &) = delete;

    bool connect(const char *endpoint,
                 uint16_t port,
                 const char *clientId,
                 const char *certPem,
                 const char *keyPem,
                 const char *rootCaPem)
    {
        LOPCORE_LOGI(TAG, "  [MQTT] Connecting to %s:%u as %s", endpoint, port, clientId);

        // Store credentials — lifetime must cover the entire connection
        endpoint_ = endpoint;
        certPem_ = certPem;
        keyPem_ = keyPem;
        rootCaPem_ = rootCaPem;

        // Configure ESP-IDF MQTT with in-memory PEM strings
        esp_mqtt_client_config_t cfg = {};
        cfg.broker.address.hostname = endpoint_.c_str();
        cfg.broker.address.port = port;
        cfg.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;

        // TLS: server certificate verification
        cfg.broker.verification.certificate = rootCaPem_.c_str();
        cfg.broker.verification.certificate_len = rootCaPem_.size() + 1; // include NUL

        // TLS: mutual authentication with claim credentials
        cfg.credentials.authentication.certificate = certPem_.c_str();
        cfg.credentials.authentication.certificate_len = certPem_.size() + 1;
        cfg.credentials.authentication.key = keyPem_.c_str();
        cfg.credentials.authentication.key_len = keyPem_.size() + 1;

        cfg.credentials.client_id = clientId;
        cfg.session.keepalive = 60;

        handle_ = esp_mqtt_client_init(&cfg);
        if (!handle_)
        {
            LOPCORE_LOGE(TAG, "  [MQTT] esp_mqtt_client_init failed");
            return false;
        }

        // Register event handler to track connection state
        esp_mqtt_client_register_event(
            handle_, MQTT_EVENT_ANY,
            [](void *arg, esp_event_base_t, int32_t eventId, void *eventData) {
                auto *self = static_cast<ProvisioningMqttAdapter *>(arg);
                auto *event = static_cast<esp_mqtt_event_handle_t>(eventData);
                self->onEvent(static_cast<esp_mqtt_event_id_t>(eventId), event);
            },
            this);

        if (esp_mqtt_client_start(handle_) != ESP_OK)
        {
            LOPCORE_LOGE(TAG, "  [MQTT] esp_mqtt_client_start failed");
            esp_mqtt_client_destroy(handle_);
            handle_ = nullptr;
            return false;
        }

        // Wait up to 15 s for connection
        for (int i = 0; i < 150 && !connected_; ++i)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if (!connected_)
        {
            LOPCORE_LOGE(TAG, "  [MQTT] Connection timeout");
            return false;
        }

        LOPCORE_LOGI(TAG, "  [MQTT] Connected");
        return true;
    }

    bool disconnect()
    {
        if (!handle_)
            return true;
        esp_mqtt_client_stop(handle_);
        esp_mqtt_client_destroy(handle_);
        handle_ = nullptr;
        connected_ = false;
        topicCount_ = 0;
        return true;
    }

    bool subscribe(const char *topic, std::function<void(const char *, size_t)> callback)
    {
        if (!handle_ || topicCount_ >= PROV_MAX_TOPICS)
            return false;

        subscriptions_[topicCount_] = {std::string(topic), std::move(callback)};
        ++topicCount_;

        int mid = esp_mqtt_client_subscribe(handle_, topic, 1 /*QoS*/);
        return mid != -1;
    }

    bool publish(const char *topic, const char *payload, size_t length)
    {
        if (!handle_)
            return false;
        int mid = esp_mqtt_client_publish(handle_, topic, payload, static_cast<int>(length), 1 /*QoS*/, 0);
        return mid != -1;
    }

    bool isConnected() const
    {
        return connected_;
    }

    // The provisioner calls this in a polling loop while waiting for responses.
    // ESP-MQTT is event-driven so we just yield briefly here.
    void processEvents(uint32_t timeoutMs)
    {
        vTaskDelay(pdMS_TO_TICKS(timeoutMs > 0 ? timeoutMs : 10));
    }

private:
    struct Subscription
    {
        std::string topic;
        std::function<void(const char *, size_t)> callback;
    };

    void onEvent(esp_mqtt_event_id_t id, esp_mqtt_event_handle_t event)
    {
        switch (id)
        {
            case MQTT_EVENT_CONNECTED:
                connected_ = true;
                break;

            case MQTT_EVENT_DISCONNECTED:
                connected_ = false;
                break;

            case MQTT_EVENT_DATA:
                // Dispatch to the matching subscription callback
                for (int i = 0; i < topicCount_; ++i)
                {
                    if (subscriptions_[i].topic.compare(0, std::string::npos, event->topic,
                                                        static_cast<size_t>(event->topic_len)) == 0)
                    {
                        subscriptions_[i].callback(event->data, static_cast<size_t>(event->data_len));
                        break;
                    }
                }
                break;

            default:
                break;
        }
    }

    esp_mqtt_client_handle_t handle_;
    volatile bool connected_;

    // Stored PEM strings — must outlive the client handle
    std::string endpoint_;
    std::string certPem_;
    std::string keyPem_;
    std::string rootCaPem_;

    // Simple subscription table
    Subscription subscriptions_[PROV_MAX_TOPICS];
    int topicCount_ = 0;
};

// ============================================================================
// Phase 1: BLE + WiFi Provisioning
//
// Spins up the ESP-IDF wifi_prov_mgr over BLE.  Blocks until WiFi is
// provisioned (SSID + password received from the mobile app AND WiFi
// station is connected) or a timeout is reached.
//
// Returns true if WiFi is now connected, false on failure.
// ============================================================================

static bool runWifiProvisioning(std::shared_ptr<lopcore::NvsStorage> awsNvs)
{
    using namespace lopcore::prov;

    LOPCORE_LOGI(TAG, "");
    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "Phase 1: BLE WiFi Provisioning");
    LOPCORE_LOGI(TAG, "===========================================");

    // -------------------------------------------------------------------------
    // 1. Create storage callbacks for each AWS config field.
    //    AwsDataEndpointHandler will call these to persist the data it receives
    //    from the mobile app over BLE.
    // -------------------------------------------------------------------------

    auto awsStorage = nvsStorage(awsNvs); // one StorageCallbacks wrapping awsNvs

    AwsDataEndpointConfig awsEndpointCfg;
    awsEndpointCfg.endpointName = "aws-data";
    awsEndpointCfg.storageMap = {
        {"claim_cert", awsStorage},
        {"claim_key", awsStorage},
        {"aws_endpoint", awsStorage},
        {"root_ca", awsStorage},
        {"provisioning_template", awsStorage},
    };

    auto awsHandler = std::make_shared<AwsDataEndpointHandler>(awsEndpointCfg);

    // -------------------------------------------------------------------------
    // 2. Configure WiFi Provisioning
    //
    //    - Transport:    BLE (switch to ProvisioningTransport::SOFTAP for SoftAP)
    //    - Security:     SECURITY_0 (no PoP) — suitable for development
    //                    Use SECURITY_1 + setProofOfPossession("abcd1234") in
    //                    production to prevent eavesdropping.
    //    - Service name: visible to mobile apps as BLE advertisement name
    //    - Custom endpoint: "aws-data" for receiving AWS credentials from app
    // -------------------------------------------------------------------------

    WiFiProvisioningConfig wifiProvConfig;
    wifiProvConfig.setTransport(ProvisioningTransport::BLE)
        .setSecurity(ProvisioningSecurity::SECURITY_0)
        .setServiceName(BLE_SERVICE_NAME)
        .addCustomEndpoint(CustomEndpointConfig("aws-data", awsHandler));

    // -------------------------------------------------------------------------
    // 3. Start provisioning
    // -------------------------------------------------------------------------

    lopcore::prov::WiFiProvisioning wifiProv;

    if (!wifiProv.init(wifiProvConfig))
    {
        LOPCORE_LOGE(TAG, "WiFiProvisioning::init() failed");
        return false;
    }

    if (!wifiProv.start())
    {
        LOPCORE_LOGE(TAG, "WiFiProvisioning::start() failed");
        return false;
    }

    LOPCORE_LOGI(TAG, "BLE provisioning started — device name: %s", BLE_SERVICE_NAME);
    LOPCORE_LOGI(TAG, "Open your provisioning app and connect to \"%s\"", BLE_SERVICE_NAME);
    LOPCORE_LOGI(TAG, "Provide WiFi credentials + AWS config via the app.");

    // -------------------------------------------------------------------------
    // 4. Wait until WiFi is provisioned (ESP-IDF connects WiFi automatically)
    //    Poll with a generous 5-minute timeout for user interaction.
    // -------------------------------------------------------------------------

    const int TIMEOUT_TICKS = 300 * 1000 / 500; // 5 min in 500 ms slices
    int waited = 0;

    while (!wifiProv.isProvisioned() && waited < TIMEOUT_TICKS)
    {
        if (waited % 20 == 0) // log every 10 s
        {
            LOPCORE_LOGI(TAG, "  Waiting for BLE provisioning... (%d s elapsed)", waited / 2);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        ++waited;
    }

    if (!wifiProv.isProvisioned())
    {
        LOPCORE_LOGE(TAG, "BLE provisioning timed out — erasing credentials and restarting");
        wifiProv.resetProvisioning();
        wifiProv.stop();
        return false;
    }

    LOPCORE_LOGI(TAG, "WiFi credentials received! Device is connected to WiFi.");

    wifiProv.stop();
    vTaskDelay(pdMS_TO_TICKS(200)); // let BLE stack settle

    return true;
}

// ============================================================================
// Phase 2: AWS IoT Fleet Provisioning
//
// Reads claim credentials from NVS (stored by AwsDataEndpointHandler in
// Phase 1), connects to AWS IoT, and runs the Fleet Provisioning workflow to
// obtain a permanent device certificate and Thing Name.
//
// Returns true if provisioning succeeded.
// ============================================================================

static bool runFleetProvisioning(std::shared_ptr<lopcore::NvsStorage> awsNvs)
{
    using namespace lopcore::prov;

    LOPCORE_LOGI(TAG, "");
    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "Phase 2: AWS Fleet Provisioning");
    LOPCORE_LOGI(TAG, "===========================================");

    auto awsStorage = nvsStorage(awsNvs);

    // -------------------------------------------------------------------------
    // 1. Read AWS config from NVS (written by AwsDataEndpointHandler over BLE)
    // -------------------------------------------------------------------------

    auto claimCert = awsNvs->read("claim_cert");
    auto claimKey = awsNvs->read("claim_key");
    auto awsEndpoint = awsNvs->read("aws_endpoint");
    auto rootCa = awsNvs->read("root_ca");
    auto templateName = awsNvs->read("provisioning_template");

    // Fall back to compile-time endpoint if the mobile app did not send one
    if (!awsEndpoint.has_value() || awsEndpoint->empty())
    {
        if (strlen(DEFAULT_AWS_ENDPOINT) > 0)
        {
            awsEndpoint = DEFAULT_AWS_ENDPOINT;
        }
        else
        {
            LOPCORE_LOGE(TAG, "AWS endpoint not set — cannot provision");
            return false;
        }
    }

    if (!claimCert.has_value() || claimCert->empty() || !claimKey.has_value() || claimKey->empty())
    {
        LOPCORE_LOGE(TAG, "Claim certificate/key missing — cannot provision");
        return false;
    }

    if (!rootCa.has_value() || rootCa->empty())
    {
        LOPCORE_LOGE(TAG, "Root CA not set — cannot provision");
        return false;
    }

    if (!templateName.has_value() || templateName->empty())
    {
        LOPCORE_LOGE(TAG, "Provisioning template name not set — cannot provision");
        return false;
    }

    LOPCORE_LOGI(TAG, "  Endpoint:  %s", awsEndpoint->c_str());
    LOPCORE_LOGI(TAG, "  Template:  %s", templateName->c_str());

    // -------------------------------------------------------------------------
    // 2. Create CertificateManager and import claim credentials
    //
    //    usePkcs11 = false: mbedTLS software keys (development mode).
    //    For production set usePkcs11 = true and enable
    //    CONFIG_LOPCORE_PROV_CERT_COREP11 in sdkconfig.
    // -------------------------------------------------------------------------

    CertificateManager::Config cmConfig;
    cmConfig.usePkcs11 = false;
    cmConfig.pkcs11Backend = CertificateManager::Pkcs11Backend::AWS_CORE_PKCS11;

    auto certManager = std::make_shared<CertificateManager>(cmConfig);

    LOPCORE_LOGI(TAG, "  Importing claim certificate...");
    if (!certManager->importClaimCertificate(claimCert.value(), claimKey.value()))
    {
        LOPCORE_LOGE(TAG, "  Failed to import claim certificate");
        return false;
    }
    LOPCORE_LOGI(TAG, "  Claim certificate imported OK");

    // -------------------------------------------------------------------------
    // 3. Build AWS Provisioning Configuration
    //
    //    Storage bindings:
    //      "thing_name"            → NVS key "thing_name"   (written by provisioner)
    //      "claim_cert"            → NVS key "claim_cert"   (already written by BLE handler)
    //      "claim_key"             → NVS key "claim_key"
    //      "aws_endpoint"          → NVS key "aws_endpoint"
    //      "root_ca"               → NVS key "root_ca"
    //      "provisioning_template" → NVS key "provisioning_template"
    //
    //    The provisioner uses these callbacks to:
    //      - Read claim credentials (connect step)
    //      - Write thing_name (after RegisterThing succeeds)
    //      - Read stored config values at runtime
    // -------------------------------------------------------------------------

    AwsProvisioningConfig awsCfg;
    awsCfg.setEndpoint(awsEndpoint.value())
        .setTemplateName(templateName.value())
        .setRootCa(rootCa.value())
        .setCsrSubjectName(CSR_SUBJECT_NAME)
        .setCertificateManager(certManager)
        .setRetries(3, 5)
        .setDeviceIdProvider(getDeviceId)
        .addConfigStorage("thing_name", awsStorage)
        .addConfigStorage("claim_cert", awsStorage)
        .addConfigStorage("claim_key", awsStorage)
        .addConfigStorage("aws_endpoint", awsStorage)
        .addConfigStorage("root_ca", awsStorage)
        .addConfigStorage("provisioning_template", awsStorage);

    // -------------------------------------------------------------------------
    // 4. Create the MQTT adapter and AwsFleetProvisioner
    //
    //    ProvisioningMqttAdapter wraps esp_mqtt_client (ESP-IDF) configured
    //    with in-memory PEM credentials — avoiding the PKCS#11 label dependency
    //    that lopcore TlsConfig normally requires.  After provisioning finishes,
    //    normal application MQTT uses the full lopcore TLS stack with PKCS#11.
    //
    //    Duck-typed interface required by AwsFleetProvisioner<T>:
    //      connect(endpoint, port, clientId, certPem, keyPem, rootCaPem)
    //      disconnect()
    //      subscribe(topic, callback)
    //      publish(topic, payload, length)
    //      isConnected()
    //      processEvents(ms)
    // -------------------------------------------------------------------------

    auto mqttAdapter = std::make_shared<ProvisioningMqttAdapter>();
    AwsFleetProvisioner<ProvisioningMqttAdapter> provisioner(awsCfg, mqttAdapter);

    // -------------------------------------------------------------------------
    // 5. Run provisioning (blocking — typically 20–60 seconds)
    // -------------------------------------------------------------------------

    LOPCORE_LOGI(TAG, "  Starting fleet provisioning workflow...");
    LOPCORE_LOGI(TAG, "  (this may take up to 60 seconds)");

    bool success = provisioner.provision();

    const auto &result = provisioner.getLastResult();

    if (success)
    {
        LOPCORE_LOGI(TAG, "");
        LOPCORE_LOGI(TAG, "  ✓ Fleet provisioning succeeded!");
        LOPCORE_LOGI(TAG, "  Thing Name : %s", result.deviceId.c_str());
        LOPCORE_LOGI(TAG, "  Certificate: %zu bytes", result.certificatePem.size());

        // Delete claim credentials — they are single-use and must not persist
        LOPCORE_LOGI(TAG, "  Deleting claim credentials (security hygiene)...");
        certManager->deleteClaimCredentials();
        awsNvs->remove("claim_cert");
        awsNvs->remove("claim_key");

        LOPCORE_LOGI(TAG, "  Device is permanently provisioned. Restarting...");
    }
    else
    {
        LOPCORE_LOGE(TAG, "");
        LOPCORE_LOGE(TAG, "  ✗ Fleet provisioning failed at step: %d", static_cast<int>(result.lastStep));
        LOPCORE_LOGE(TAG, "  Error: %s", result.errorMessage.c_str());
        LOPCORE_LOGE(TAG, "  Retrying on next boot (credentials preserved).");
    }

    return success;
}

// ============================================================================
// Normal operation — entered when device is already provisioned
// ============================================================================

static void runNormalOperation(const std::string &thingName, const std::string &awsEndpoint)
{
    LOPCORE_LOGI(TAG, "");
    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "Normal Operation");
    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "  Thing Name:  %s", thingName.c_str());
    LOPCORE_LOGI(TAG, "  AWS Endpoint: %s", awsEndpoint.c_str());
    LOPCORE_LOGI(TAG, "");
    LOPCORE_LOGI(TAG, "  Device is provisioned. Connect to AWS IoT and start");
    LOPCORE_LOGI(TAG, "  your application logic here.");
    LOPCORE_LOGI(TAG, "");
    LOPCORE_LOGI(TAG, "  See examples/04_mqtt_esp_client and");
    LOPCORE_LOGI(TAG, "  examples/06_mqtt_coremqtt_sync for MQTT usage.");

    // Blink / idle loop — replace with your application
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(10000));
        LOPCORE_LOGI(TAG, "  [idle] heap free: %lu bytes", esp_get_free_heap_size());
    }
}

// ============================================================================
// app_main
// ============================================================================

extern "C" void app_main(void)
{
    // -------------------------------------------------------------------------
    // System initialisation
    // -------------------------------------------------------------------------

    // Initialize NVS flash partition (required before any NVS usage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create default event loop (required by WiFi, BLE, and ESP-MQTT)
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize lopcore logger with console sink
    auto &logger = lopcore::Logger::getInstance();
    logger.addSink(std::make_unique<lopcore::ConsoleSink>());
    logger.setGlobalLevel(lopcore::LogLevel::INFO);

    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "LopCore BLE Fleet Provisioning Example");
    LOPCORE_LOGI(TAG, "  Device ID: %s", getDeviceId().c_str());
    LOPCORE_LOGI(TAG, "===========================================");

    // -------------------------------------------------------------------------
    // Create NVS storage instances (one per namespace)
    // -------------------------------------------------------------------------

    // AWS config namespace — stores claim creds, endpoint, root_ca, thing_name
    auto awsNvsConfig = lopcore::storage::NvsConfig().setNamespace(NVS_NS_AWS).setReadOnly(false);
    auto awsNvs = std::make_shared<lopcore::NvsStorage>(awsNvsConfig);

    if (!awsNvs->initialize())
    {
        LOPCORE_LOGE(TAG, "Failed to initialize AWS NVS namespace '%s'", NVS_NS_AWS);
        return;
    }

    // -------------------------------------------------------------------------
    // Determine provisioning state
    //
    //   "thing_name" present in NVS  → device is provisioned
    //   Otherwise                    → run provisioning flow
    // -------------------------------------------------------------------------

    auto thingName = awsNvs->read("thing_name");
    auto awsEndpoint = awsNvs->read("aws_endpoint");

    bool isProvisioned = thingName.has_value() && !thingName->empty() && awsEndpoint.has_value() &&
                         !awsEndpoint->empty();

    if (isProvisioned)
    {
        LOPCORE_LOGI(TAG, "Device already provisioned as: %s", thingName->c_str());
        runNormalOperation(thingName.value(), awsEndpoint.value());
        return; // not reached — runNormalOperation loops forever
    }

    // -------------------------------------------------------------------------
    // Phase 1: BLE + WiFi provisioning
    // -------------------------------------------------------------------------

    LOPCORE_LOGI(TAG, "Device not provisioned — starting BLE provisioning flow");

    if (!runWifiProvisioning(awsNvs))
    {
        LOPCORE_LOGE(TAG, "WiFi provisioning failed — restarting in 5 s");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }

    // Allow WiFi station to fully associate before attempting AWS connection
    vTaskDelay(pdMS_TO_TICKS(2000));

    // -------------------------------------------------------------------------
    // Phase 2: AWS Fleet Provisioning
    // -------------------------------------------------------------------------

    if (!runFleetProvisioning(awsNvs))
    {
        LOPCORE_LOGE(TAG, "Fleet provisioning failed — restarting in 10 s");
        vTaskDelay(pdMS_TO_TICKS(10000));
        esp_restart();
    }

    // Restart to enter normal operation with permanent credentials
    LOPCORE_LOGI(TAG, "Provisioning complete — restarting in 3 s");
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}
