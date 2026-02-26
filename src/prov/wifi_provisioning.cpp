#include "lopcore/prov/wifi_provisioning.hpp"
#include "lopcore/prov/ble_data_framer.hpp"

#include <cstring>

#include <esp_log.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>
#include <wifi_provisioning/scheme_softap.h>

static const char *TAG = "WiFiProvisioning";

namespace lopcore
{
namespace prov
{

// ---------- ESP-IDF Callbacks ----------

/**
 * Custom endpoint handler wrapper for ESP-IDF
 *
 * ESP-IDF expects C-style function pointers, so we use static functions
 * that lookup the handler from the global map.
 */
static std::map<std::string, std::shared_ptr<ICustomEndpointHandler>> g_handlerMap;

// BLE framer for each endpoint (handles length-prefixed chunks)
static std::map<std::string, BleDataFramer> g_framerMap;

static esp_err_t custom_endpoint_handler(uint32_t session_id,
                                         const uint8_t *inbuf,
                                         ssize_t inlen,
                                         uint8_t **outbuf,
                                         ssize_t *outlen,
                                         void *priv_data)
{
    const char *endpoint_name = static_cast<const char *>(priv_data);

    auto it = g_handlerMap.find(endpoint_name);
    if (it == g_handlerMap.end())
    {
        ESP_LOGE(TAG, "Handler not found for endpoint: %s", endpoint_name);
        return ESP_ERR_NOT_FOUND;
    }

    auto &handler = it->second;
    auto &framer = g_framerMap[endpoint_name];

    // Feed data to framer (handles length-prefixed chunk reassembly)
    BleDataFramer::FrameResult result = framer.onData(inbuf, inlen);

    switch (result)
    {
        case BleDataFramer::FrameResult::NEED_MORE:
            // Waiting for more chunks, send empty ACK
            *outbuf = nullptr;
            *outlen = 0;
            return ESP_OK;

        case BleDataFramer::FrameResult::COMPLETE:
        {
            // Full payload received, forward to handler
            const std::vector<uint8_t>& payload = framer.payload();
            bool success = handler->onDataReceived(session_id, payload.data(), payload.size(), true);
            
            // Reset framer for next transfer
            framer.reset();
            
            if (!success)
            {
                ESP_LOGE(TAG, "Handler failed for endpoint: %s", endpoint_name);
                return ESP_FAIL;
            }

            // Get response (if any)
            static uint8_t response_buffer[1024]; // Static to persist after return
            size_t response_len = handler->getResponse(session_id, response_buffer, sizeof(response_buffer));

            if (response_len > 0)
            {
                *outbuf = response_buffer;
                *outlen = response_len;
            }
            else
            {
                *outbuf = nullptr;
                *outlen = 0;
            }

            return ESP_OK;
        }

        case BleDataFramer::FrameResult::ERROR_TOO_LARGE:
            ESP_LOGE(TAG, "Framer error: payload too large for endpoint %s", endpoint_name);
            framer.reset();
            return ESP_ERR_NO_MEM;

        case BleDataFramer::FrameResult::ERROR_INVALID:
        default:
            ESP_LOGE(TAG, "Framer error: invalid data for endpoint %s", endpoint_name);
            framer.reset();
            return ESP_ERR_INVALID_ARG;
    }
}

// ---------- WiFiProvisioning Implementation ----------

WiFiProvisioning::WiFiProvisioning() = default;

WiFiProvisioning::~WiFiProvisioning()
{
    stop();
}

bool WiFiProvisioning::init(const WiFiProvisioningConfig &config)
{
    if (initialized_)
    {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }

    config_ = config;

    // Initialize ESP-IDF wifi_prov_mgr
    wifi_prov_mgr_config_t mgr_config;
    memset(&mgr_config, 0, sizeof(mgr_config));

    // Set transport scheme
    if (config.getTransport() == ProvisioningTransport::BLE)
    {
        mgr_config.scheme = wifi_prov_scheme_ble;
    }
    else
    {
        ESP_LOGE(TAG, "SoftAP transport not yet implemented");
        return false;
    }

    // Set security
    switch (config.getSecurity())
    {
        case ProvisioningSecurity::SECURITY_0:
            mgr_config.scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE;
            break;
        case ProvisioningSecurity::SECURITY_1:
            mgr_config.scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM;
            break;
        case ProvisioningSecurity::SECURITY_2:
            mgr_config.scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BT;
            break;
    }

    esp_err_t err = wifi_prov_mgr_init(mgr_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "wifi_prov_mgr_init failed: %s", esp_err_to_name(err));
        return false;
    }

    // Register custom endpoints
    for (const auto &endpoint : config.getCustomEndpoints())
    {
        // Store handler in global map (for C callbacks)
        g_handlerMap[endpoint.endpointName] = endpoint.handler;
        handlers_[endpoint.endpointName] = endpoint.handler;
        
        // Initialize BLE framer for this endpoint (default max size from Kconfig)
#ifdef CONFIG_LOPCORE_PROV_BLE_MAX_PAYLOAD_SIZE
        g_framerMap[endpoint.endpointName] = BleDataFramer(CONFIG_LOPCORE_PROV_BLE_MAX_PAYLOAD_SIZE);
#else
        g_framerMap[endpoint.endpointName] = BleDataFramer(4096);  // Default 4KB
#endif

        // Determine protocol ID for ESP-IDF
        uint8_t protocol_id = 0;
        switch (endpoint.handler->getProtocol())
        {
            case EndpointProtocol::JSON_LENGTH_PREFIXED:
                protocol_id = 0x01;
                break;
            case EndpointProtocol::PROTOBUF:
                protocol_id = 0x02;
                break;
            case EndpointProtocol::CBOR:
                protocol_id = 0x03;
                break;
            case EndpointProtocol::RAW:
            default:
                protocol_id = 0x00;
                break;
        }

        err = wifi_prov_mgr_endpoint_create(endpoint.endpointName.c_str());
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to create endpoint %s: %s", endpoint.endpointName.c_str(),
                     esp_err_to_name(err));
            return false;
        }

        err = wifi_prov_mgr_endpoint_register(endpoint.endpointName.c_str(), custom_endpoint_handler,
                                              const_cast<char *>(endpoint.endpointName.c_str()) // priv_data
        );

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register endpoint %s: %s", endpoint.endpointName.c_str(),
                     esp_err_to_name(err));
            return false;
        }

        ESP_LOGI(TAG, "Registered custom endpoint: %s (protocol: 0x%02X)", endpoint.endpointName.c_str(),
                 protocol_id);
    }

    initialized_ = true;
    ESP_LOGI(TAG, "WiFi provisioning initialized");
    return true;
}

bool WiFiProvisioning::start()
{
    if (!initialized_)
    {
        ESP_LOGE(TAG, "Not initialized, call init() first");
        return false;
    }

    if (running_)
    {
        ESP_LOGW(TAG, "Already running");
        return true;
    }

    // Check if already provisioned
    if (isProvisioned())
    {
        ESP_LOGI(TAG, "Device already provisioned, skipping provisioning");
        return true;
    }

    // Set security
    const char *pop = nullptr;
    if (config_.getProofOfPossession().has_value())
    {
        pop = config_.getProofOfPossession()->c_str();
    }

    // Map security level from config
    wifi_prov_security_t security;
    switch (config_.getSecurity())
    {
        case ProvisioningSecurity::SECURITY_0:
            security = WIFI_PROV_SECURITY_0;
            break;
        case ProvisioningSecurity::SECURITY_2:
            security = WIFI_PROV_SECURITY_2;
            break;
        case ProvisioningSecurity::SECURITY_1:
        default:
            security = WIFI_PROV_SECURITY_1;
            break;
    }

    // Start provisioning service
    const char *service_name = config_.getServiceName().c_str();

    esp_err_t err = wifi_prov_mgr_start_provisioning(security, pop, service_name,
                                                     nullptr // service_key (for QR code, optional)
    );

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "wifi_prov_mgr_start_provisioning failed: %s", esp_err_to_name(err));
        return false;
    }

    running_ = true;
    ESP_LOGI(TAG, "WiFi provisioning started (service: %s)", service_name);
    return true;
}

void WiFiProvisioning::stop()
{
    if (!running_)
    {
        return;
    }

    // Stop provisioning
    wifi_prov_mgr_stop_provisioning();

    // Deinitialize
    wifi_prov_mgr_deinit();

    // Clear handlers and framers
    for (const auto &[name, handler] : handlers_)
    {
        g_handlerMap.erase(name);
        g_framerMap.erase(name);
    }

    running_ = false;
    initialized_ = false;
    ESP_LOGI(TAG, "WiFi provisioning stopped");
}

bool WiFiProvisioning::isProvisioned() const
{
    if (!config_.getWiFiStorage().has_value())
    {
        // No storage configured, check ESP-IDF manager
        bool provisioned = false;
        esp_err_t err = wifi_prov_mgr_is_provisioned(&provisioned);
        return (err == ESP_OK) && provisioned;
    }

    // Check storage for provisioned flag
    const auto &storage = config_.getWiFiStorage().value();
    auto result = storage.read("wifi.provisioned");
    return result.has_value() && result.value() == "1";
}

bool WiFiProvisioning::resetProvisioning()
{
    if (!config_.getWiFiStorage().has_value())
    {
        ESP_LOGE(TAG, "No storage configured, cannot reset");
        return false;
    }

    const auto &storage = config_.getWiFiStorage().value();

    // Clear WiFi credentials
    bool success = true;
    success &= storage.write("wifi.ssid", "");
    success &= storage.write("wifi.password", "");
    success &= storage.write("wifi.provisioned", "0");

    if (success)
    {
        ESP_LOGI(TAG, "Provisioning reset");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to reset provisioning");
    }

    return success;
}

std::optional<std::string> WiFiProvisioning::getWiFiSsid() const
{
    if (!config_.getWiFiStorage().has_value())
    {
        return std::nullopt;
    }

    const auto &storage = config_.getWiFiStorage().value();
    return storage.read("wifi.ssid");
}

} // namespace prov
} // namespace lopcore
