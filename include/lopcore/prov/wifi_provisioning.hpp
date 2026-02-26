#pragma once

#include "custom_endpoint_handler.hpp"
#include "storage_types.hpp"
#include <string>
#include <map>
#include <memory>
#include <optional>
#include <vector>  // For std::vector

namespace lopcore {
namespace prov {

/**
 * BLE Provisioning Security
 * 
 * Defines security level for WiFi provisioning over BLE.
 */
enum class ProvisioningSecurity {
    SECURITY_0,  // No security (open)
    SECURITY_1,  // Secure with proof-of-possession (PoP)
    SECURITY_2   // Secure with PoP + additional encryption
};

/**
 * BLE Provisioning Transport
 * 
 * Defines transport mechanism for provisioning.
 */
enum class ProvisioningTransport {
    BLE,         // Bluetooth Low Energy
    SOFTAP       // WiFi SoftAP
};

/**
 * Custom Endpoint Configuration
 * 
 * Configuration for adding custom BLE characteristics to provisioning.
 */
struct CustomEndpointConfig {
    std::string endpointName;                              // BLE characteristic name
    std::shared_ptr<ICustomEndpointHandler> handler;       // Handler implementation
    
    CustomEndpointConfig(const std::string& name,
                        std::shared_ptr<ICustomEndpointHandler> h)
        : endpointName(name), handler(std::move(h)) {}
};

/**
 * WiFi Provisioning Configuration
 * 
 * Configuration for ESP-IDF wifi_prov_mgr wrapper.
 */
class WiFiProvisioningConfig {
public:
    WiFiProvisioningConfig() = default;

    // ---------- BLE Configuration ----------

    /**
     * Set BLE device name (default: "PROV_DEVICE")
     */
    WiFiProvisioningConfig& setServiceName(const std::string& name) {
        serviceName_ = name;
        return *this;
    }

    /**
     * Set security level (default: SECURITY_1)
     */
    WiFiProvisioningConfig& setSecurity(ProvisioningSecurity security) {
        security_ = security;
        return *this;
    }

    /**
     * Set proof-of-possession string for SECURITY_1/2
     */
    WiFiProvisioningConfig& setProofOfPossession(const std::string& pop) {
        pop_ = pop;
        return *this;
    }

    /**
     * Set transport mechanism (default: BLE)
     */
    WiFiProvisioningConfig& setTransport(ProvisioningTransport transport) {
        transport_ = transport;
        return *this;
    }

    // ---------- Storage Configuration ----------

    /**
     * Set storage callbacks for WiFi credentials
     * 
     * WiFi provisioner will store:
     * - "wifi.ssid" → SSID string
     * - "wifi.password" → Password string
     * - "wifi.provisioned" → "1" when complete
     */
    WiFiProvisioningConfig& setWiFiStorage(const StorageCallbacks& storage) {
        wifiStorage_ = storage;
        return *this;
    }

    // ---------- Custom Endpoints ----------

    /**
     * Add custom BLE characteristic for receiving app data
     * 
     * Example: Add endpoint for AWS credentials
     * ```cpp
     * config.addCustomEndpoint(CustomEndpointConfig{
     *     .endpointName = "aws-creds",
     *     .handler = std::make_shared<AwsCredentialsHandler>(nvsStorage)
     * });
     * ```
     */
    WiFiProvisioningConfig& addCustomEndpoint(const CustomEndpointConfig& endpoint) {
        customEndpoints_.push_back(endpoint);
        return *this;
    }

    // ---------- Accessors ----------

    const std::string& getServiceName() const { return serviceName_; }
    ProvisioningSecurity getSecurity() const { return security_; }
    const std::optional<std::string>& getProofOfPossession() const { return pop_; }
    ProvisioningTransport getTransport() const { return transport_; }
    const std::optional<StorageCallbacks>& getWiFiStorage() const { return wifiStorage_; }
    const std::vector<CustomEndpointConfig>& getCustomEndpoints() const { return customEndpoints_; }

private:
    std::string serviceName_ = "PROV_DEVICE";
    ProvisioningSecurity security_ = ProvisioningSecurity::SECURITY_1;
    std::optional<std::string> pop_;
    ProvisioningTransport transport_ = ProvisioningTransport::BLE;
    std::optional<StorageCallbacks> wifiStorage_;
    std::vector<CustomEndpointConfig> customEndpoints_;
};

/**
 * WiFi Provisioning Manager
 * 
 * High-level wrapper around ESP-IDF wifi_prov_mgr for provisioning devices
 * over BLE or SoftAP with custom endpoint support.
 * 
 * Lifecycle:
 * 1. Initialize: `init(config)`
 * 2. Check if already provisioned: `isProvisioned()`
 * 3. Start provisioning: `start()` (if not provisioned)
 * 4. Mobile app connects, sends WiFi credentials + custom data
 * 5. Device connects to WiFi, stores credentials
 * 6. Stop provisioning: `stop()`
 * 
 * Features:
 * - Automatic WiFi credential storage (if storage configured)
 * - Custom BLE endpoints for app-specific data (AWS, GCP, etc.)
 * - Support for length-prefixed JSON, Protobuf, CBOR protocols
 * - Proof-of-possession security (PIN-based)
 * 
 * Example Usage:
 * ```cpp
 * auto nvsStorage = std::make_shared<lopcore::NvsStorage>("wifi", false);
 * nvsStorage->init();
 * 
 * auto awsHandler = std::make_shared<AwsCredentialsHandler>(nvsStorage);
 * 
 * WiFiProvisioningConfig config;
 * config.setServiceName("GrowBorg-123456")
 *       .setSecurity(ProvisioningSecurity::SECURITY_1)
 *       .setProofOfPossession("abcd1234")
 *       .setWiFiStorage(makeNvsStorage(nvsStorage))
 *       .addCustomEndpoint({"aws-creds", awsHandler});
 * 
 * WiFiProvisioning wifiProv;
 * wifiProv.init(config);
 * 
 * if (!wifiProv.isProvisioned()) {
 *     wifiProv.start();
 *     // Wait for mobile app provisioning...
 * }
 * 
 * wifiProv.stop();
 * ```
 */
class WiFiProvisioning {
public:
    WiFiProvisioning();
    ~WiFiProvisioning();

    // Prevent copying
    WiFiProvisioning(const WiFiProvisioning&) = delete;
    WiFiProvisioning& operator=(const WiFiProvisioning&) = delete;

    /**
     * Initialize provisioning manager with configuration
     * 
     * Must be called before start(). Registers custom endpoints
     * and configures ESP-IDF wifi_prov_mgr.
     * 
     * @param config Provisioning configuration
     * @return true on success, false on error
     */
    bool init(const WiFiProvisioningConfig& config);

    /**
     * Start provisioning service
     * 
     * Starts BLE advertising (or SoftAP) for mobile app to connect.
     * Blocks briefly for service startup, then returns.
     * 
     * Call stop() when provisioning is complete or timeout occurs.
     * 
     * @return true on success, false on error
     */
    bool start();

    /**
     * Stop provisioning service
     * 
     * Stops BLE advertising and cleans up ESP-IDF resources.
     * Should be called after WiFi credentials are received.
     */
    void stop();

    /**
     * Check if device is already provisioned
     * 
     * Queries storage for "wifi.provisioned" flag.
     * 
     * @return true if WiFi credentials exist, false otherwise
     */
    bool isProvisioned() const;

    /**
     * Reset provisioning state
     * 
     * Clears stored WiFi credentials and custom endpoint data.
     * Forces device to re-provision on next boot.
     * 
     * @return true on success, false on error
     */
    bool resetProvisioning();

    /**
     * Get current WiFi SSID
     * 
     * Reads from storage (does NOT scan WiFi).
     * 
     * @return SSID string if provisioned, empty optional otherwise
     */
    std::optional<std::string> getWiFiSsid() const;

private:
    WiFiProvisioningConfig config_;
    bool initialized_ = false;
    bool running_ = false;
    
    // Custom endpoint handlers (keep alive for ESP-IDF callbacks)
    std::map<std::string, std::shared_ptr<ICustomEndpointHandler>> handlers_;
};

} // namespace prov
} // namespace lopcore
