/**
 * @file provisioning_helpers.cpp
 * @brief Implementation of provisioning helpers (BLE and AWS).
 */

#include "provisioning_helpers.hpp"

#include <optional>

#include "lopcore/mqtt/mqtt_config.hpp"
#include "lopcore/tls/tls_config.hpp"

#include "esp_timer.h"

using lopcore::mqtt::MqttConfig;
using lopcore::mqtt::MqttQos;
using lopcore::tls::TlsConfig;

static const char *TAG = "provisioning_helpers";

// ============================================================================
// ProvisioningMqttAdapter Implementation
//
// Bridges the duck-typed AwsFleetProvisioner<T> interface to lopcore's
// EspMqttClient. Claim credentials are passed as in-memory PEM strings
// (they have not yet been imported into PKCS#11 at this stage).
// ============================================================================

bool ProvisioningMqttAdapter::connect(const char *endpoint,
                                      uint16_t port,
                                      const char *clientId,
                                      const char *certPem,
                                      const char *keyPem,
                                      const char *rootCaPem)
{
    LOPCORE_LOGI(TAG, "  [MQTT] Connecting to %s:%u as %s", endpoint, port, clientId);

    // Build TLS config with in-memory PEM claim credentials
    TlsConfig tlsCfg;
    tlsCfg.hostname = endpoint;
    tlsCfg.port = port;
    tlsCfg.caCertPath = rootCaPem;  // passed as inline PEM (accepted by ESP-TLS)
    tlsCfg.clientCertPem = certPem; // in-memory PEM — no PKCS#11
    tlsCfg.clientKeyPem = keyPem;

    // Build MQTT config — disable auto-reconnect and budgeting for provisioning
    MqttConfig mqttCfg;
    mqttCfg.broker = endpoint;
    mqttCfg.port = port;
    mqttCfg.clientId = clientId;
    mqttCfg.keepAlive = std::chrono::seconds(60);
    mqttCfg.tls = tlsCfg;
    mqttCfg.reconnect.autoReconnect = false;
    mqttCfg.budget.enabled = false;

    connected_ = false;
    client_ = std::make_unique<lopcore::mqtt::EspMqttClient>(mqttCfg);

    // Track connection state via lopcore callback
    client_->setConnectionCallback([this](bool connected) { connected_ = connected; });

    if (client_->connect() != ESP_OK)
    {
        LOPCORE_LOGE(TAG, "  [MQTT] connect() failed");
        client_.reset();
        return false;
    }

    // Wait up to 15 s for the async connection to complete
    for (int i = 0; i < 150 && !connected_; ++i)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (!connected_)
    {
        LOPCORE_LOGE(TAG, "  [MQTT] Connection timeout");
        client_.reset();
        return false;
    }

    LOPCORE_LOGI(TAG, "  [MQTT] Connected");
    return true;
}

bool ProvisioningMqttAdapter::disconnect()
{
    if (!client_)
        return true;
    client_->disconnect();
    client_.reset();
    connected_ = false;
    return true;
}

bool ProvisioningMqttAdapter::subscribe(const char *topic, std::function<void(const char *, size_t)> callback)
{
    if (!client_)
        return false;

    // Bridge: convert lopcore MqttMessage to the raw (data, len) duck-typed callback
    auto bridged = [cb = std::move(callback)](const lopcore::mqtt::MqttMessage &msg) {
        cb(reinterpret_cast<const char *>(msg.payload.data()), msg.payload.size());
    };

    return client_->subscribe(topic, std::move(bridged), MqttQos::AT_LEAST_ONCE) == ESP_OK;
}

bool ProvisioningMqttAdapter::unsubscribe(const char *topic)
{
    if (!client_)
        return true;
    return client_->unsubscribe(topic) == ESP_OK;
}

bool ProvisioningMqttAdapter::publish(const char *topic, const char *payload, size_t length)
{
    if (!client_)
        return false;
    std::string str(payload, length);
    return client_->publishString(topic, str, MqttQos::AT_LEAST_ONCE, false) == ESP_OK;
}

bool ProvisioningMqttAdapter::isConnected() const
{
    return connected_;
}

void ProvisioningMqttAdapter::processEvents(uint32_t timeoutMs)
{
    // EspMqttClient is event-driven; just yield to let the MQTT task run
    vTaskDelay(pdMS_TO_TICKS(timeoutMs > 0 ? timeoutMs : 10));
}

// ============================================================================
// BleProvisioningHelper Implementation
// ============================================================================

BleProvisioningHelper::BleProvisioningHelper(ProvisioningContext &ctx) : ctx_(ctx)
{
}

BleProvisioningHelper::~BleProvisioningHelper()
{
    stop();
}

bool BleProvisioningHelper::start()
{
    LOPCORE_LOGI(TAG, "");
    LOPCORE_LOGI(TAG, "  Starting BLE provisioning...");

    // Create storage callbacks for AWS fields
    auto awsStorage = nvsStorage(ctx_.awsNvs);

    AwsDataEndpointConfig awsEndpointCfg;
    awsEndpointCfg.endpointName = "aws-data";
    awsEndpointCfg.storageMap = {
        {"claim_cert", awsStorage},
        {"claim_key", awsStorage},
        {"aws_endpoint", awsStorage},
        {"root_ca", awsStorage},
        {"provisioning_template", awsStorage},
    };

    awsHandler_ = std::make_shared<AwsDataEndpointHandler>(awsEndpointCfg);

    // Configure WiFi Provisioning
    WiFiProvisioningConfig wifiProvConfig;
    wifiProvConfig.setTransport(ProvisioningTransport::BLE)
        .setSecurity(ProvisioningSecurity::SECURITY_0)
        .setServiceName(ctx_.ble_service_name.c_str())
        .addCustomEndpoint(CustomEndpointConfig("aws-data", awsHandler_));

    // Start provisioning
    wifiProv_ = std::make_shared<WiFiProvisioning>();

    if (!wifiProv_->init(wifiProvConfig))
    {
        lastError_ = "WiFiProvisioning::init() failed";
        LOPCORE_LOGE(TAG, "  %s", lastError_.c_str());
        return false;
    }

    if (!wifiProv_->start())
    {
        lastError_ = "WiFiProvisioning::start() failed";
        LOPCORE_LOGE(TAG, "  %s", lastError_.c_str());
        return false;
    }

    LOPCORE_LOGI(TAG, "  BLE advertising: %s", ctx_.ble_service_name.c_str());
    LOPCORE_LOGI(TAG, "  Open your provisioning app and connect.");

    return true;
}

void BleProvisioningHelper::stop()
{
    if (wifiProv_)
    {
        wifiProv_->stop();
        wifiProv_.reset();
    }
}

bool BleProvisioningHelper::isComplete() const
{
    if (!wifiProv_)
        return false;
    return wifiProv_->isProvisioned();
}

// ============================================================================
// AwsProvisioningHelper Implementation
// ============================================================================

AwsProvisioningHelper::AwsProvisioningHelper(ProvisioningContext &ctx) : ctx_(ctx)
{
}

AwsProvisioningHelper::~AwsProvisioningHelper() = default;

bool AwsProvisioningHelper::provision()
{
    LOPCORE_LOGI(TAG, "");
    LOPCORE_LOGI(TAG, "  Starting AWS Fleet Provisioning...");

    // Read claim credentials from NVS
    ctx_.prov_data.claim_cert = ctx_.awsNvs->read("claim_cert");
    ctx_.prov_data.claim_key = ctx_.awsNvs->read("claim_key");
    ctx_.prov_data.aws_endpoint = ctx_.awsNvs->read("aws_endpoint");
    ctx_.prov_data.root_ca = ctx_.awsNvs->read("root_ca");
    ctx_.prov_data.provisioning_template = ctx_.awsNvs->read("provisioning_template");

    // Fallback to compile-time endpoint if needed
    if (!ctx_.prov_data.aws_endpoint.has_value() || ctx_.prov_data.aws_endpoint->empty())
    {
        if (!ctx_.default_aws_endpoint.empty())
        {
            ctx_.prov_data.aws_endpoint = ctx_.default_aws_endpoint;
        }
        else
        {
            lastError_ = "AWS endpoint not set";
            LOPCORE_LOGE(TAG, "  %s", lastError_.c_str());
            return false;
        }
    }

    // Validate credentials
    if (!ctx_.prov_data.claim_cert.has_value() || ctx_.prov_data.claim_cert->empty())
    {
        lastError_ = "Claim certificate missing from BLE provisioning";
        LOPCORE_LOGE(TAG, "  %s", lastError_.c_str());
        return false;
    }

    if (!ctx_.prov_data.claim_key.has_value() || ctx_.prov_data.claim_key->empty())
    {
        lastError_ = "Claim key missing from BLE provisioning";
        LOPCORE_LOGE(TAG, "  %s", lastError_.c_str());
        return false;
    }

    if (!ctx_.prov_data.root_ca.has_value() || ctx_.prov_data.root_ca->empty())
    {
        lastError_ = "Root CA missing from BLE provisioning";
        LOPCORE_LOGE(TAG, "  %s", lastError_.c_str());
        return false;
    }

    if (!ctx_.prov_data.provisioning_template.has_value() || ctx_.prov_data.provisioning_template->empty())
    {
        lastError_ = "Provisioning template name missing from BLE provisioning";
        LOPCORE_LOGE(TAG, "  %s", lastError_.c_str());
        return false;
    }

    LOPCORE_LOGI(TAG, "    Endpoint:  %s", ctx_.prov_data.aws_endpoint->c_str());
    LOPCORE_LOGI(TAG, "    Template:  %s", ctx_.prov_data.provisioning_template->c_str());

    // Create CertificateManager and import claim credentials
    CertificateManager::Config cmConfig;
    cmConfig.usePkcs11 = false;
    cmConfig.pkcs11Backend = CertificateManager::Pkcs11Backend::AWS_CORE_PKCS11;

    certManager_ = std::make_shared<CertificateManager>(cmConfig);

    LOPCORE_LOGI(TAG, "    Importing claim certificate...");
    if (!certManager_->importClaimCertificate(ctx_.prov_data.claim_cert.value(),
                                              ctx_.prov_data.claim_key.value()))
    {
        lastError_ = "Failed to import claim certificate";
        LOPCORE_LOGE(TAG, "    %s", lastError_.c_str());
        return false;
    }
    LOPCORE_LOGI(TAG, "    Claim certificate imported OK");

    // Create provisioning configuration
    auto awsStorage = nvsStorage(ctx_.awsNvs);

    AwsProvisioningConfig awsCfg;
    awsCfg.setEndpoint(ctx_.prov_data.aws_endpoint.value())
        .setTemplateName(ctx_.prov_data.provisioning_template.value())
        .setRootCa(ctx_.prov_data.root_ca.value())
        .setCsrSubjectName(ctx_.csr_subject_name.c_str())
        .setCertificateManager(certManager_)
        .setRetries(3, 5)
        .setDeviceIdProvider([&ctx = ctx_]() { return ctx.deviceId; })
        .addConfigStorage("thing_name", awsStorage)
        .addConfigStorage("claim_cert", awsStorage)
        .addConfigStorage("claim_key", awsStorage)
        .addConfigStorage("aws_endpoint", awsStorage)
        .addConfigStorage("root_ca", awsStorage)
        .addConfigStorage("provisioning_template", awsStorage);

    // Create MQTT adapter and provisioner
    mqttAdapter_ = std::make_shared<ProvisioningMqttAdapter>();
    AwsFleetProvisioner<ProvisioningMqttAdapter> provisioner(awsCfg, mqttAdapter_);

    LOPCORE_LOGI(TAG, "    Running Fleet Provisioning workflow...");
    LOPCORE_LOGI(TAG, "    (This may take up to 60 seconds)");

    succeeded_ = provisioner.provision();

    const auto &result = provisioner.getLastResult();

    if (succeeded_)
    {
        thingName_ = result.deviceId;
        LOPCORE_LOGI(TAG, "    ✓ Succeeded!");
        LOPCORE_LOGI(TAG, "    Thing Name: %s", thingName_.c_str());
        return true;
    }
    else
    {
        lastError_ = "Fleet Provisioning failed at step " +
                     std::to_string(static_cast<int>(result.lastStep)) + ": " + result.errorMessage;
        LOPCORE_LOGE(TAG, "    ✗ Failed: %s", lastError_.c_str());
        return false;
    }
}

void AwsProvisioningHelper::deleteClaimCredentials()
{
    LOPCORE_LOGI(TAG, "    Deleting claim credentials (security hygiene)...");
    if (certManager_)
    {
        certManager_->deleteClaimCredentials();
    }
    ctx_.awsNvs->remove("claim_cert");
    ctx_.awsNvs->remove("claim_key");
    LOPCORE_LOGI(TAG, "    Claim credentials deleted OK");
}
