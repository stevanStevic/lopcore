/**
 * @file factory_reset_state.cpp
 * @brief FACTORY_RESET state implementation.
 */

#include "factory_reset_state.hpp"

#include "esp_system.h"

static const char *TAG = "app_sm:factory_reset";

FactoryResetState::FactoryResetState(lopcore::StateMachine<ApplicationState> *sm, ProvisioningContext &ctx)
    : sm_(sm), ctx_(ctx)
{
}

void FactoryResetState::onEnter()
{
    LOPCORE_LOGI(TAG, "");
    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "FACTORY_RESET: Erasing device configuration");
    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "")

        ;
    LOPCORE_LOGE(TAG, "Provisioning failed after %d attempts", ctx_.config.retryCount + 1);
    LOPCORE_LOGE(TAG, "Last error: %s", ctx_.config.lastPhaseError.c_str());
    LOPCORE_LOGI(TAG, "");
    LOPCORE_LOGI(TAG, "Erasing NVS namespaces...");

    // Erase provisioning namespace
    if (ctx_.awsNvs->initialize())
    {
        ctx_.awsNvs->clear();
        LOPCORE_LOGI(TAG, "  ✓ Erased AWS provisioning config");
    }

    LOPCORE_LOGI(TAG, "");
    LOPCORE_LOGI(TAG, "Factory reset complete. Restarting in 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    LOPCORE_LOGI(TAG, "");
    LOPCORE_LOGI(TAG, "RESTARTING...");
    LOPCORE_LOGI(TAG, "");

    esp_restart();
    // Does not return
}

void FactoryResetState::update()
{
    // Should not be reached (esp_restart above doesn't return)
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void FactoryResetState::onExit()
{
    // Should not be reached
}
