/**
 * @file main.cpp
 * @brief BLE + AWS Fleet Provisioning Example with State Machine
 *
 * Demonstrates:
 *  - StateMachine<ApplicationState> with transition rules and observer
 *  - BLE + AWS Fleet Provisioning in CONFIGURATION state
 *  - DEGRADED state for WiFi reconnection
 *  - GPIO interrupt-driven factory reset button
 *  - lopcore: Logger, NvsStorage, StateMachine, WiFiProvisioning, CoreMqttClient
 */
#include <cstring>
#include <memory>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "lopcore/logging/console_sink.hpp"
#include "lopcore/logging/logger.hpp"
#include "lopcore/storage/nvs_storage.hpp"
#include "lopcore/storage/storage_config.hpp"

#include "application_state_machine.hpp"

static const char *TAG = "prov_example";

// ============================================================================
// Configuration — edit these constants for your setup
// ============================================================================

// BLE device name visible to the provisioning app (esp_prov.py or mobile)
static constexpr const char *BLE_SERVICE_NAME = "PROV_EXAMPLE";

// NVS namespace for AWS provisioning credentials and Thing Name
static constexpr const char *NVS_NS_AWS = "prov_aws";

// Compile-time AWS IoT endpoint fallback (can be overridden by BLE provisioning)
static constexpr const char *DEFAULT_AWS_ENDPOINT = "";

// CSR subject name for the device certificate generated during Fleet Provisioning
static constexpr const char *CSR_SUBJECT_NAME = "CN=ExampleDevice";

// ============================================================================
// Mock factory reset button (GPIO interrupt, active-low)
//
// Default: GPIO_NUM_0 (BOOT button on most ESP32 dev boards).
// Change FACTORY_RESET_GPIO to your button pin.
// For production: add debounce (e.g. 50ms software filter) and
// hold-duration check before triggering reset.
// ============================================================================
static constexpr gpio_num_t FACTORY_RESET_GPIO = GPIO_NUM_0;

static SemaphoreHandle_t s_resetSemaphore = nullptr;
static lopcore::StateMachine<ApplicationState> *s_smPtr = nullptr;

static void IRAM_ATTR factory_reset_isr_handler(void *arg)
{
    // ISR: minimal work — post semaphore, let task do the transition
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(s_resetSemaphore, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

static void button_task(void *arg)
{
    while (true)
    {
        if (xSemaphoreTake(s_resetSemaphore, portMAX_DELAY) == pdTRUE)
        {
            LOPCORE_LOGI(TAG, "[button] Factory reset triggered via GPIO %d", FACTORY_RESET_GPIO);
            if (s_smPtr)
                s_smPtr->transition(ApplicationState::FACTORY_RESET);
        }
    }
}

static void setup_factory_reset_button(lopcore::StateMachine<ApplicationState> *sm)
{
    s_smPtr        = sm;
    s_resetSemaphore = xSemaphoreCreateBinary();

    gpio_config_t io_conf{};
    io_conf.intr_type    = GPIO_INTR_NEGEDGE;   // falling edge = button press (active-low)
    io_conf.mode         = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << FACTORY_RESET_GPIO);
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(FACTORY_RESET_GPIO, factory_reset_isr_handler, nullptr);

    xTaskCreate(button_task, "button_task", 2048, nullptr, 5, nullptr);
    LOPCORE_LOGI(TAG, "Factory reset button configured on GPIO %d (active-low)", FACTORY_RESET_GPIO);
}

// ============================================================================
// Device ID from MAC address
// ============================================================================
static std::string getDeviceId()
{
    uint8_t mac[6] = {};
    esp_base_mac_addr_get(mac);
    char id[13];
    snprintf(id, sizeof(id), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(id);
}

// ============================================================================
// Application task — runs state machine on a large stack
// ============================================================================

struct AppTaskParams {
    std::shared_ptr<lopcore::NvsStorage> awsNvs;
    std::string deviceId;
};

static void app_task(void *pvParam)
{
    auto *p = static_cast<AppTaskParams *>(pvParam);

    // Initialize lopcore logger with console sink
    auto &logger = lopcore::Logger::getInstance();
    logger.addSink(std::make_unique<lopcore::ConsoleSink>());
    logger.setGlobalLevel(lopcore::LogLevel::INFO);

    // Application context
    ProvisioningContext ctx;
    ctx.awsNvs               = p->awsNvs;
    ctx.deviceId             = p->deviceId;
    ctx.ble_service_name     = BLE_SERVICE_NAME;
    ctx.csr_subject_name     = CSR_SUBJECT_NAME;
    ctx.default_aws_endpoint = DEFAULT_AWS_ENDPOINT;
    delete p;

    // Create state machine
    auto sm = createApplicationStateMachine(ctx);

    // Setup factory reset button (GPIO interrupt)
    setup_factory_reset_button(sm.get());

    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "LopCore BLE Fleet Provisioning Example");
    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "Device ID: %s", ctx.deviceId.c_str());
    LOPCORE_LOGI(TAG, "State machine ready. Starting in INIT...");
    LOPCORE_LOGI(TAG, "");

    while (true)
    {
        sm->update();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ============================================================================
// app_main — minimal; heavy init delegated to app_task (32 KB stack)
// ============================================================================
extern "C" void app_main(void)
{
    // NVS flash
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Event loop + TCP/IP + WiFi (required by wifi_prov_mgr before start)
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // NVS storage (created here so it doesn't live on app_task stack)
    auto awsNvsConfig = lopcore::storage::NvsConfig()
                            .setNamespace(NVS_NS_AWS)
                            .setReadOnly(false);
    auto awsNvs = std::make_shared<lopcore::NvsStorage>(awsNvsConfig);
    if (!awsNvs->initialize())
    {
        ESP_LOGE(TAG, "Failed to initialize NVS namespace '%s'", NVS_NS_AWS);
        return;
    }

    auto *p      = new AppTaskParams{awsNvs, getDeviceId()};
    xTaskCreate(app_task, "app_task", 32768, p, 5, nullptr);
}
