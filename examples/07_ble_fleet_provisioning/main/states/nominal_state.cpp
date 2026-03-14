/**
 * @file nominal_state.cpp
 * @brief NOMINAL state implementation.
 */

#include "states/nominal_state.hpp"

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "app_sm:nominal";

NominalState::NominalState(lopcore::StateMachine<ApplicationState> *sm, ProvisioningContext &ctx)
    : sm_(sm), ctx_(ctx)
{
}

void NominalState::onEnter()
{
    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "NOMINAL: Device provisioned and ready");
    LOPCORE_LOGI(TAG, "===========================================");
    if (ctx_.prov_data.thing_name.has_value())
        LOPCORE_LOGI(TAG, "  Thing Name:   %s", ctx_.prov_data.thing_name->c_str());
    if (ctx_.prov_data.aws_endpoint.has_value())
        LOPCORE_LOGI(TAG, "  AWS Endpoint: %s", ctx_.prov_data.aws_endpoint->c_str());
    LOPCORE_LOGI(TAG, "  Device ID:    %s", ctx_.deviceId.c_str());
    LOPCORE_LOGI(TAG, "");
    LOPCORE_LOGI(TAG, "  For MQTT integration see:");
    LOPCORE_LOGI(TAG, "    examples/04_mqtt_esp_client     -- EspMqttClient patterns");
    LOPCORE_LOGI(TAG, "    examples/06_mqtt_coremqtt_sync  -- CoreMqttClient + AWS IoT");
    loopCount_ = 0;
}

void NominalState::update()
{
    if (loopCount_ % 100 == 0)
        LOPCORE_LOGI(TAG, "  [heartbeat] heap: %lu bytes  uptime: %us",
                     esp_get_free_heap_size(), loopCount_ / 10);
    ++loopCount_;
    vTaskDelay(pdMS_TO_TICKS(100));
}

void NominalState::onExit()
{
    LOPCORE_LOGI(TAG, "[NOMINAL] Exit (ran for %u cycles)\n", loopCount_);
}
