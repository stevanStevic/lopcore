/**
 * @file init_state.cpp
 * @brief INIT state implementation.
 */

#include "init_state.hpp"

#include "esp_mac.h"

static const char *TAG = "app_sm:init";

InitState::InitState(lopcore::StateMachine<ApplicationState> *sm, ProvisioningContext &ctx)
    : sm_(sm), ctx_(ctx)
{
}

void InitState::onEnter()
{
    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "LopCore BLE Fleet Provisioning Example");
    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "Device ID: %s", ctx_.deviceId.c_str());
    LOPCORE_LOGI(TAG, "");
}

void InitState::update()
{
    LOPCORE_LOGI(TAG, "[INIT] Checking provisioning status...");

    // Read provisioning state from NVS
    ctx_.prov_data.thing_name = ctx_.awsNvs->read("thing_name");
    ctx_.prov_data.aws_endpoint = ctx_.awsNvs->read("aws_endpoint");

    bool isProvisioned = ctx_.prov_data.thing_name.has_value() && !ctx_.prov_data.thing_name->empty() &&
                         ctx_.prov_data.aws_endpoint.has_value() && !ctx_.prov_data.aws_endpoint->empty();

    if (isProvisioned)
    {
        LOPCORE_LOGI(TAG, "[INIT] Device already provisioned as: %s", ctx_.prov_data.thing_name->c_str());
        LOPCORE_LOGI(TAG, "[INIT] Transition → NOMINAL");
        sm_->transition(ApplicationState::NOMINAL);
    }
    else
    {
        LOPCORE_LOGI(TAG, "[INIT] Device not provisioned");
        LOPCORE_LOGI(TAG, "[INIT] Transition → CONFIGURATION");
        sm_->transition(ApplicationState::CONFIGURATION);
    }
}

void InitState::onExit()
{
    LOPCORE_LOGI(TAG, "[INIT] Exit\n");
}
