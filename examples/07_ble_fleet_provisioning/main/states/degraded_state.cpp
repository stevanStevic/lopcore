/**
 * @file degraded_state.cpp
 */
#include "states/degraded_state.hpp"

#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lopcore/logging/logger.hpp"

static const char *TAG = "app_sm:degraded";

DegradedState::DegradedState(lopcore::StateMachine<ApplicationState> *sm, ProvisioningContext &ctx)
    : sm_(sm), ctx_(ctx)
{
}

void DegradedState::onEnter()
{
    enterTimeMs_ = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "DEGRADED: WiFi connection lost");
    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "  Attempting to reconnect (timeout: %us)...", RECONNECT_TIMEOUT_MS / 1000);
    esp_wifi_connect();
}

void DegradedState::update()
{
    uint32_t nowMs   = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    uint32_t elapsed = nowMs - enterTimeMs_;

    // Check if WiFi is connected (got IP)
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip_info{};
    if (sta_netif && esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0)
    {
        LOPCORE_LOGI(TAG, "  WiFi reconnected! Transitioning to NOMINAL...");
        sm_->transition(ApplicationState::NOMINAL);
        return;
    }

    if (elapsed >= RECONNECT_TIMEOUT_MS)
    {
        LOPCORE_LOGW(TAG, "  Reconnect timeout (%us). Returning to INIT to re-evaluate...",
                     RECONNECT_TIMEOUT_MS / 1000);
        sm_->transition(ApplicationState::INIT);
        return;
    }

    // Retry every 5s
    if (elapsed % 5000 < 200)
        LOPCORE_LOGI(TAG, "  Retrying WiFi connect... (%us elapsed)", elapsed / 1000);

    vTaskDelay(pdMS_TO_TICKS(200));
}

void DegradedState::onExit()
{
    LOPCORE_LOGI(TAG, "[DEGRADED] Exit\n");
}
