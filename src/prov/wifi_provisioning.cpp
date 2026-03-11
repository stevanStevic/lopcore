#include "lopcore/prov/wifi_provisioning.hpp"

#include <cstdlib>
#include <cstring>
#include <vector>

#include <esp_event.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <protocomm_ble.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>
#include <wifi_provisioning/scheme_softap.h>

#include "lopcore/prov/ble_data_framer.hpp"

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

/**
 * Mutex protecting g_handlerMap and g_framerMap.
 * Taken by init()/stop() (app task) and custom_endpoint_handler() (event task)
 * to prevent concurrent access across FreeRTOS tasks.
 */
static SemaphoreHandle_t g_mapMutex = nullptr;

// ---------- Event group bits ----------

static const EventBits_t PROV_SUCCESS_BIT = BIT0;
static const EventBits_t PROV_FAILURE_BIT = BIT1;

/**
 * Pointer to the active WiFiProvisioningEvents so the static event handler
 * can call them.  Set during init(), cleared during stop().
 */
static const WiFiProvisioningEvents *g_eventCallbacks = nullptr;

/**
 * Event group used by waitForCompletion().
 * Aliased through eventGroup_ (stored as void*) in WiFiProvisioning.
 */
static EventGroupHandle_t g_provEventGroup = nullptr;

/**
 * Static ESP-IDF event handler for WIFI_PROV_EVENT and
 * PROTOCOMM_TRANSPORT_BLE_EVENT.
 */
static void prov_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_PROV_EVENT)
    {
        switch (event_id)
        {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Provisioning started");
                if (g_eventCallbacks && g_eventCallbacks->onStarted)
                {
                    g_eventCallbacks->onStarted();
                }
                break;

            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t *wifi_sta_cfg = static_cast<wifi_sta_config_t *>(event_data);
                std::string ssid(reinterpret_cast<const char *>(wifi_sta_cfg->ssid));
                std::string password(reinterpret_cast<const char *>(wifi_sta_cfg->password));
                ESP_LOGI(TAG, "Received WiFi credentials — SSID: %s", ssid.c_str());
                if (g_eventCallbacks && g_eventCallbacks->onCredentialsReceived)
                {
                    g_eventCallbacks->onCredentialsReceived(ssid, password);
                }
                break;
            }

            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t *reason = static_cast<wifi_prov_sta_fail_reason_t *>(event_data);
                std::string reasonStr = (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "WiFi authentication failed"
                                                                              : "Access-point not found";
                ESP_LOGE(TAG, "Provisioning failed: %s", reasonStr.c_str());
                wifi_prov_mgr_reset_sm_state_on_failure();
                if (g_eventCallbacks && g_eventCallbacks->onFailed)
                {
                    g_eventCallbacks->onFailed(reasonStr);
                }
                if (g_provEventGroup)
                {
                    xEventGroupSetBits(g_provEventGroup, PROV_FAILURE_BIT);
                }
                break;
            }

            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning successful");
                if (g_eventCallbacks && g_eventCallbacks->onSuccess)
                {
                    g_eventCallbacks->onSuccess();
                }
                if (g_provEventGroup)
                {
                    xEventGroupSetBits(g_provEventGroup, PROV_SUCCESS_BIT);
                }
                break;

            case WIFI_PROV_END:
                ESP_LOGI(TAG, "Provisioning ended");
                if (g_eventCallbacks && g_eventCallbacks->onEnd)
                {
                    g_eventCallbacks->onEnd();
                }
                break;

            default:
                break;
        }
    }
    else if (event_base == PROTOCOMM_TRANSPORT_BLE_EVENT)
    {
        switch (event_id)
        {
            case PROTOCOMM_TRANSPORT_BLE_CONNECTED:
                ESP_LOGI(TAG, "BLE transport: Connected");
                if (g_eventCallbacks && g_eventCallbacks->onBleConnected)
                {
                    g_eventCallbacks->onBleConnected();
                }
                break;

            case PROTOCOMM_TRANSPORT_BLE_DISCONNECTED:
                ESP_LOGI(TAG, "BLE transport: Disconnected");
                if (g_eventCallbacks && g_eventCallbacks->onBleDisconnected)
                {
                    g_eventCallbacks->onBleDisconnected();
                }
                break;

            default:
                break;
        }
    }
}

static esp_err_t custom_endpoint_handler(uint32_t session_id,
                                         const uint8_t *inbuf,
                                         ssize_t inlen,
                                         uint8_t **outbuf,
                                         ssize_t *outlen,
                                         void *priv_data)
{
    const char *endpoint_name = static_cast<const char *>(priv_data);

    // ---- Acquire map mutex ----
    if (g_mapMutex == nullptr || xSemaphoreTake(g_mapMutex, pdMS_TO_TICKS(5000)) == pdFALSE)
    {
        ESP_LOGE(TAG, "Map mutex unavailable for endpoint: %s", endpoint_name);
        *outbuf = nullptr;
        *outlen = 0;
        return ESP_FAIL;
    }

    auto it = g_handlerMap.find(endpoint_name);
    if (it == g_handlerMap.end())
    {
        xSemaphoreGive(g_mapMutex);
        ESP_LOGE(TAG, "Handler not found for endpoint: %s", endpoint_name);
        return ESP_ERR_NOT_FOUND;
    }

    // Copy shared_ptr so stop() may erase the map entry concurrently after we release
    auto handler = it->second;
    auto framerIt = g_framerMap.find(endpoint_name);
    if (framerIt == g_framerMap.end())
    {
        xSemaphoreGive(g_mapMutex);
        ESP_LOGE(TAG, "Framer not found for endpoint: %s", endpoint_name);
        return ESP_ERR_NOT_FOUND;
    }
    auto &framer = framerIt->second;

    // Feed raw BLE chunk to the length-prefixed framer
    FrameResult result = framer.onData(inbuf, inlen);

    if (result == FrameResult::NEED_MORE)
    {
        xSemaphoreGive(g_mapMutex);
        *outbuf = nullptr;
        *outlen = 0;
        return ESP_OK;
    }

    if (result == FrameResult::ERROR_TOO_LARGE)
    {
        framer.reset();
        xSemaphoreGive(g_mapMutex);
        ESP_LOGE(TAG, "Framer error: payload too large for endpoint %s", endpoint_name);
        return ESP_ERR_NO_MEM;
    }

    if (result != FrameResult::COMPLETE)
    {
        framer.reset();
        xSemaphoreGive(g_mapMutex);
        ESP_LOGE(TAG, "Framer error: invalid data for endpoint %s", endpoint_name);
        return ESP_ERR_INVALID_ARG;
    }

    // COMPLETE: copy payload out of the framer buffer, then reset & release mutex
    // before calling user handler (which may block or do I/O).
    std::vector<uint8_t> payloadCopy(framer.payload(), framer.payload() + framer.payloadSize());
    framer.reset();
    xSemaphoreGive(g_mapMutex);

    // ---- Call handler outside the mutex ----
    bool success = handler->onDataReceived(session_id, payloadCopy.data(), payloadCopy.size(), true);
    if (!success)
    {
        ESP_LOGE(TAG, "Handler failed for endpoint: %s", endpoint_name);
        return ESP_FAIL;
    }

    // Build response (if any).
    // protocomm will call free(*outbuf) — MUST use malloc, not a stack buffer.
    const size_t kMaxResponseSize = 1024;
    uint8_t temp_buf[kMaxResponseSize];
    size_t response_len = handler->getResponse(session_id, temp_buf, sizeof(temp_buf));

    if (response_len > 0)
    {
        *outbuf = static_cast<uint8_t *>(malloc(response_len));
        if (*outbuf == nullptr)
        {
            ESP_LOGE(TAG, "OOM allocating response for endpoint %s", endpoint_name);
            *outlen = 0;
            return ESP_ERR_NO_MEM;
        }
        memcpy(*outbuf, temp_buf, response_len);
        *outlen = static_cast<ssize_t>(response_len);
    }
    else
    {
        *outbuf = nullptr;
        *outlen = 0;
    }

    return ESP_OK;
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

    // Create FreeRTOS mutex protecting g_handlerMap / g_framerMap
    if (g_mapMutex == nullptr)
    {
        g_mapMutex = xSemaphoreCreateMutex();
        if (g_mapMutex == nullptr)
        {
            ESP_LOGE(TAG, "Failed to create map mutex");
            return false;
        }
    }

    // Create FreeRTOS event group for waitForCompletion()
    EventGroupHandle_t evtGroup = xEventGroupCreate();
    if (evtGroup == nullptr)
    {
        ESP_LOGE(TAG, "Failed to create event group");
        return false;
    }
    eventGroup_ = static_cast<void *>(evtGroup);
    g_provEventGroup = evtGroup;

    // Register event callbacks pointer (static, valid for init/stop lifetime)
    g_eventCallbacks = &config_.getEventCallbacks();

    // Register event handler for WIFI_PROV_EVENT
    esp_event_handler_instance_t evtInst = nullptr;
    esp_err_t evtErr = esp_event_handler_instance_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID,
                                                           &prov_event_handler, nullptr, &evtInst);
    if (evtErr != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register WIFI_PROV_EVENT handler: %s", esp_err_to_name(evtErr));
        vEventGroupDelete(evtGroup);
        eventGroup_ = nullptr;
        g_provEventGroup = nullptr;
        return false;
    }
    eventHandlerInstance_ = static_cast<void *>(evtInst);

    // Register event handler for PROTOCOMM_TRANSPORT_BLE_EVENT (BLE connect/disconnect)
    // Only relevant for BLE transport — skip for SoftAP.
    if (config.getTransport() == ProvisioningTransport::BLE)
    {
        esp_event_handler_instance_t bleEvtInst = nullptr;
        esp_err_t bleErr = esp_event_handler_instance_register(PROTOCOMM_TRANSPORT_BLE_EVENT,
                                                               ESP_EVENT_ANY_ID, &prov_event_handler, nullptr,
                                                               &bleEvtInst);
        if (bleErr != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to register BLE transport event handler (non-fatal): %s",
                     esp_err_to_name(bleErr));
            // Non-fatal: BLE connect/disconnect events are optional
        }
        bleEventHandlerInstance_ = static_cast<void *>(bleEvtInst);
    }

    // Initialize ESP-IDF wifi_prov_mgr
    wifi_prov_mgr_config_t mgr_config;
    memset(&mgr_config, 0, sizeof(mgr_config));

    // Set transport scheme and security event handler
    if (config.getTransport() == ProvisioningTransport::BLE)
    {
        mgr_config.scheme = wifi_prov_scheme_ble;
        // BLE scheme event handler frees Bluetooth memory after provisioning.
        // The level depends on the security mode chosen.
        switch (config.getSecurity())
        {
            case ProvisioningSecurity::SECURITY_0:
                mgr_config.scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE;
                break;
            case ProvisioningSecurity::SECURITY_2:
                mgr_config.scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BT;
                break;
            case ProvisioningSecurity::SECURITY_1:
            default:
                mgr_config.scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM;
                break;
        }
    }
    else // SoftAP
    {
        mgr_config.scheme = wifi_prov_scheme_softap;
        // SoftAP does not use Bluetooth; memory management not applicable.
        mgr_config.scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE;
        ESP_LOGI(TAG, "Using SoftAP provisioning transport");
    }

    esp_err_t err = wifi_prov_mgr_init(mgr_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "wifi_prov_mgr_init failed: %s", esp_err_to_name(err));
        return false;
    }

    // (P3-5) Set custom BLE Service UUID, if provided
    if (config.getTransport() == ProvisioningTransport::BLE)
    {
        if (const auto &uuid = config.getBleServiceUuid(); uuid.has_value())
        {
            wifi_prov_scheme_ble_set_service_uuid(const_cast<uint8_t *>(uuid->data()));
            ESP_LOGI(TAG, "Custom BLE Service UUID configured");
        }
    }

    // Register custom endpoints
    for (const auto &endpoint : config.getCustomEndpoints())
    {
        // Store handler and framer in global maps (protected by g_mapMutex)
        if (xSemaphoreTake(g_mapMutex, pdMS_TO_TICKS(1000)) == pdFALSE)
        {
            ESP_LOGE(TAG, "Timeout acquiring mutex for endpoint: %s", endpoint.endpointName.c_str());
            return false;
        }
        g_handlerMap[endpoint.endpointName] = endpoint.handler;
#ifdef CONFIG_LOPCORE_PROV_BLE_MAX_PAYLOAD
        g_framerMap.insert_or_assign(endpoint.endpointName,
                                     BleDataFramer(CONFIG_LOPCORE_PROV_BLE_MAX_PAYLOAD));
#else
        g_framerMap.insert_or_assign(endpoint.endpointName, BleDataFramer(4096));
#endif
        xSemaphoreGive(g_mapMutex);

        handlers_[endpoint.endpointName] = endpoint.handler;

        // Create endpoint slot in wifi_prov_mgr.
        // Registration (wifi_prov_mgr_endpoint_register) must happen AFTER
        // wifi_prov_mgr_start_provisioning(), so we defer it to start().
        err = wifi_prov_mgr_endpoint_create(endpoint.endpointName.c_str());
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to create endpoint %s: %s", endpoint.endpointName.c_str(),
                     esp_err_to_name(err));
            return false;
        }

        ESP_LOGI(TAG, "Created custom endpoint slot: %s", endpoint.endpointName.c_str());
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
    // For SoftAP, service_key is the WPA2 network password; for BLE it is unused.
    const char *service_key = config_.getServiceKey().has_value() ? config_.getServiceKey()->c_str()
                                                                  : nullptr;

    esp_err_t err = wifi_prov_mgr_start_provisioning(security, pop, service_name, service_key);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "wifi_prov_mgr_start_provisioning failed: %s", esp_err_to_name(err));
        return false;
    }

    running_ = true;
    ESP_LOGI(TAG, "WiFi provisioning started (service: %s)", service_name);

    // Register custom endpoints NOW — wifi_prov_mgr_endpoint_register must be called
    // AFTER wifi_prov_mgr_start_provisioning().  Use the key pointer from handlers_
    // (stable for the lifetime of the map) to avoid a dangling priv_data pointer.
    for (const auto &[name, handler] : handlers_)
    {
        const char *stable_name = handlers_.find(name)->first.c_str();
        esp_err_t reg_err = wifi_prov_mgr_endpoint_register(name.c_str(), custom_endpoint_handler,
                                                            const_cast<char *>(stable_name));

        if (reg_err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register endpoint %s: %s", name.c_str(), esp_err_to_name(reg_err));
            return false;
        }
        ESP_LOGI(TAG, "Registered endpoint: %s", name.c_str());
    }

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

    // Unregister event handlers
    if (eventHandlerInstance_)
    {
        esp_event_handler_instance_unregister(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID,
                                              static_cast<esp_event_handler_instance_t>(
                                                  eventHandlerInstance_));
        eventHandlerInstance_ = nullptr;
    }
    if (bleEventHandlerInstance_)
    {
        esp_event_handler_instance_unregister(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID,
                                              static_cast<esp_event_handler_instance_t>(
                                                  bleEventHandlerInstance_));
        bleEventHandlerInstance_ = nullptr;
    }

    // Clear global event state
    g_eventCallbacks = nullptr;
    if (g_provEventGroup)
    {
        vEventGroupDelete(g_provEventGroup);
        g_provEventGroup = nullptr;
    }
    eventGroup_ = nullptr;

    // Clear handlers and framers (protected by mutex so no in-flight handler sees deleted data)
    if (g_mapMutex != nullptr && xSemaphoreTake(g_mapMutex, pdMS_TO_TICKS(5000)) == pdTRUE)
    {
        for (const auto &[name, handler] : handlers_)
        {
            g_handlerMap.erase(name);
            g_framerMap.erase(name);
        }
        xSemaphoreGive(g_mapMutex);
    }
    else
    {
        ESP_LOGW(TAG, "Could not acquire map mutex during stop — skipping map cleanup");
    }

    // Delete mutex (no more callbacks after wifi_prov_mgr_deinit)
    if (g_mapMutex != nullptr)
    {
        vSemaphoreDelete(g_mapMutex);
        g_mapMutex = nullptr;
    }

    running_ = false;
    initialized_ = false;
    ESP_LOGI(TAG, "WiFi provisioning stopped");
}

bool WiFiProvisioning::waitForCompletion(uint32_t timeoutMs)
{
    if (!running_ || eventGroup_ == nullptr)
    {
        ESP_LOGE(TAG, "waitForCompletion: not running or event group not created");
        return false;
    }

    EventGroupHandle_t evtGroup = static_cast<EventGroupHandle_t>(eventGroup_);
    TickType_t ticksToWait = (timeoutMs == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);

    EventBits_t bits = xEventGroupWaitBits(evtGroup, PROV_SUCCESS_BIT | PROV_FAILURE_BIT,
                                           pdFALSE, // do not clear bits on exit
                                           pdFALSE, // wait for any bit
                                           ticksToWait);

    if (bits & PROV_SUCCESS_BIT)
    {
        ESP_LOGI(TAG, "waitForCompletion: provisioning succeeded");
        return true;
    }

    if (bits & PROV_FAILURE_BIT)
    {
        ESP_LOGE(TAG, "waitForCompletion: provisioning failed");
        return false;
    }

    ESP_LOGW(TAG, "waitForCompletion: timed out after %" PRIu32 " ms", timeoutMs);
    return false;
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
