/**
 * @file provisioning_helpers.hpp
 * @brief Helper classes and functions for provisioning workflow.
 *
 * Encapsulates BLE provisioning and AWS fleet provisioning logic that is
 * called by the ConfigurationState state machine.
 */

#pragma once

#include <functional>
#include <memory>
#include <string>

#include "lopcore/mqtt/esp_mqtt_client.hpp"
#include "lopcore/mqtt/mqtt_config.hpp"
#include "lopcore/mqtt/mqtt_types.hpp"
#include "lopcore/prov/aws_data_endpoint_handler.hpp"
#include "lopcore/prov/certificate_manager.hpp"
#include "lopcore/prov/wifi_provisioning.hpp"

#include "application_state_machine.hpp"

using namespace lopcore::prov;

// ============================================================================
// MQTT Adapter for Provisioning
//
// Bridges the duck-typed interface expected by AwsFleetProvisioner<T> to
// lopcore's EspMqttClient. Uses in-memory PEM credentials (claim cert/key)
// that are available before PKCS#11 import during the provisioning phase.
// ============================================================================

class ProvisioningMqttAdapter
{
public:
    ProvisioningMqttAdapter() = default;
    ~ProvisioningMqttAdapter() = default;

    // Non-copyable
    ProvisioningMqttAdapter(const ProvisioningMqttAdapter &) = delete;
    ProvisioningMqttAdapter &operator=(const ProvisioningMqttAdapter &) = delete;

    // Called by AwsFleetProvisioner — creates and connects lopcore EspMqttClient
    bool connect(const char *endpoint,
                 uint16_t port,
                 const char *clientId,
                 const char *certPem,
                 const char *keyPem,
                 const char *rootCaPem);
    bool disconnect();

    // Bridge: lopcore MessageCallback → duck-typed callback
    bool subscribe(const char *topic, std::function<void(const char *, size_t)> callback);
    bool unsubscribe(const char *topic);
    bool publish(const char *topic, const char *payload, size_t length);
    bool isConnected() const;
    void processEvents(uint32_t timeoutMs);

private:
    std::unique_ptr<lopcore::mqtt::EspMqttClient> client_;
    volatile bool connected_{false};
};

// ============================================================================
// BLE Provisioning Helper
//
// Encapsulates Phase 1 (WiFi + AWS credentials via BLE).
// ============================================================================

class BleProvisioningHelper
{
public:
    explicit BleProvisioningHelper(ProvisioningContext &ctx);
    ~BleProvisioningHelper();

    // Start BLE provisioning, return true if WiFi + AWS creds received
    bool start();

    // Stop BLE provisioning (cleanup)
    void stop();

    // Check if WiFi station + AWS credentials have been provisioned
    bool isComplete() const;

    // Get detailed error (if failed)
    const std::string &getLastError() const
    {
        return lastError_;
    }

private:
    ProvisioningContext &ctx_;
    std::shared_ptr<WiFiProvisioning> wifiProv_;
    std::shared_ptr<AwsDataEndpointHandler> awsHandler_;
    std::string lastError_;
};

// ============================================================================
// AWS Provisioning Helper
//
// Encapsulates Phase 2 (Fleet Provisioning workflow with MQTT).
// ============================================================================

class AwsProvisioningHelper
{
public:
    explicit AwsProvisioningHelper(ProvisioningContext &ctx);
    ~AwsProvisioningHelper();

    // Run the full AWS Fleet Provisioning workflow (blocking).
    // Returns true on success, false on failure. Use getLastError() to diagnose.
    bool provision();

    // Check result
    bool succeeded() const
    {
        return succeeded_;
    }
    const std::string &getLastError() const
    {
        return lastError_;
    }
    const std::string &getThingName() const
    {
        return thingName_;
    }

    // Clean up claim credentials (security: delete single-use certificates)
    void deleteClaimCredentials();

private:
    ProvisioningContext &ctx_;
    std::shared_ptr<ProvisioningMqttAdapter> mqttAdapter_;
    std::shared_ptr<CertificateManager> certManager_;
    bool succeeded_{false};
    std::string lastError_;
    std::string thingName_;
};

#endif // PROVISIONING_HELPERS_HPP
