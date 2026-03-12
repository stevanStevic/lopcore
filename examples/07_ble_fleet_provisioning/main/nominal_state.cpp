/**
 * @file nominal_state.cpp
 * @brief NOMINAL state implementation.
 */

#include "nominal_state.hpp"

#include "esp_system.h"

static const char *TAG = "app_sm:nominal";

NominalState::NominalState(lopcore::StateMachine<ApplicationState> *sm, ProvisioningContext &ctx)
    : sm_(sm), ctx_(ctx)
{
}

void NominalState::onEnter()
{
    LOPCORE_LOGI(TAG, "");
    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "NOMINAL: System Ready");
    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "Device is provisioned and ready for operation");
    LOPCORE_LOGI(TAG, "  Thing Name:   %s", ctx_.prov_data.thing_name->c_str());
    LOPCORE_LOGI(TAG, "  AWS Endpoint: %s", ctx_.prov_data.aws_endpoint->c_str());
    LOPCORE_LOGI(TAG, "");
    LOPCORE_LOGI(TAG, "TODO: Integrate your application logic here:");
    LOPCORE_LOGI(TAG, "  - Connect to AWS IoT Core MQTT");
    LOPCORE_LOGI(TAG, "  - Subscribe to device shadows");
    LOPCORE_LOGI(TAG, "  - Read sensors, publish telemetry");
    LOPCORE_LOGI(TAG, "  - Control LEDs, camera, etc.");
    LOPCORE_LOGI(TAG, "");
    LOPCORE_LOGI(TAG, "See examples/04_mqtt_esp_client and");
    LOPCORE_LOGI(TAG, "examples/06_mqtt_coremqtt_sync for MQTT patterns.");
    LOPCORE_LOGI(TAG, "");

    loopCount_ = 0;
}

void NominalState::update()
{
    // Simple idle loop — replace with your application logic
    if (loopCount_ % 100 == 0)
    {
        LOPCORE_LOGI(TAG, "  [idle %u] heap: %lu bytes", loopCount_ / 100, esp_get_free_heap_size());
    }
    ++loopCount_;

    vTaskDelay(pdMS_TO_TICKS(100));
}

void NominalState::onExit()
{
    LOPCORE_LOGI(TAG, "[NOMINAL] Exit (ran for %u cycles)\n", loopCount_);
}
