/**
 * @file main.cpp
 * @brief BLE + AWS Fleet Provisioning Example with State Machine
 *
 * Demonstrates professional application architecture using a state machine:
 *
 *  States:
 *  ──────
 *  1. INIT — Check provisioning status, initialize system
 *  2. CONFIGURATION — BLE WiFi provisioning + AWS Fleet Provisioning (with retry)
 *  3. NOMINAL — Normal operation (app-ready idle loop)
 *  4. FACTORY_RESET — Erase NVS and restart
 *
 * This pattern mirrors production systems. Provisioning is encapsulated as a
 * single CONFIGURATION state with an internal phase machine,
 * allowing clean separation of concerns and extensibility.
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#include <cstring>
#include <memory>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// LopCore components
#include "lopcore/logging/console_sink.hpp"
#include "lopcore/logging/logger.hpp"
#include "lopcore/storage/nvs_storage.hpp"
#include "lopcore/storage/storage_config.hpp"

// State machine
#include "application_state_machine.hpp"

// Provisioning helpers
#include "provisioning_helpers.hpp"

// ESP-IDF
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "nvs_flash.h"

static const char *TAG = "prov_example";

// ============================================================================
// Configuration
// ============================================================================

// BLE device name visible to mobile apps
static constexpr const char *BLE_SERVICE_NAME = "PROV_EXAMPLE";

// NVS namespace for AWS provisioning config
static constexpr const char *NVS_NS_AWS = "prov_aws";

// AWS IoT endpoint (may be overridden via BLE)
static constexpr const char *DEFAULT_AWS_ENDPOINT = "";

// CSR subject name embedded in the device certificate
static constexpr const char *CSR_SUBJECT_NAME = "CN=ExampleDevice";

// ============================================================================
// Helper: Get device ID from MAC address
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
// app_main
// ============================================================================

extern "C" void app_main(void)
{
    // =========================================================================
    // System Initialization
    // =========================================================================

    // Initialize NVS flash partition
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create default event loop (required by WiFi, BLE, and ESP-MQTT)
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize LopCore logger with console sink
    auto &logger = lopcore::Logger::getInstance();
    logger.addSink(std::make_unique<lopcore::ConsoleSink>());
    logger.setGlobalLevel(lopcore::LogLevel::INFO);

    // =========================================================================
    // Create NVS Storage Instance
    // =========================================================================

    auto awsNvsConfig = lopcore::storage::NvsConfig().setNamespace(NVS_NS_AWS).setReadOnly(false);
    auto awsNvs = std::make_shared<lopcore::NvsStorage>(awsNvsConfig);

    if (!awsNvs->initialize())
    {
        LOPCORE_LOGE(TAG, "Failed to initialize AWS NVS namespace '%s'", NVS_NS_AWS);
        return;
    }

    // =========================================================================
    // Create Application Context
    // =========================================================================

    ProvisioningContext ctx;
    ctx.awsNvs = awsNvs;
    ctx.deviceId = getDeviceId();
    ctx.ble_service_name = BLE_SERVICE_NAME;
    ctx.csr_subject_name = CSR_SUBJECT_NAME;
    ctx.default_aws_endpoint = DEFAULT_AWS_ENDPOINT;

    // =========================================================================
    // Create and Initialize State Machine
    // =========================================================================

    auto sm = createApplicationStateMachine(ctx);
    sm->initialize(ApplicationState::INIT);

    // =========================================================================
    // Main Loop
    // =========================================================================

    LOPCORE_LOGI(TAG, "State machine initialized. Entering main loop...");

    while (sm->isRunning())
    {
        sm->update();
        vTaskDelay(pdMS_TO_TICKS(100)); // 100 ms cycle
    }

    // Should not reach here (NominalState runs forever)
    LOPCORE_LOGE(TAG, "State machine stopped unexpectedly");
}
