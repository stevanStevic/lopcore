/**
 * @file ble_helper.hpp
 * @brief Phase 1 provisioning: BLE WiFi + AWS credentials delivery.
 *
 * ORDERING REQUIREMENT: AWS credentials MUST be sent before WiFi credentials.
 * Once wifi_prov_mgr receives WiFi credentials and the device connects
 * (WIFI_PROV_CRED_SUCCESS), WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
 * tears down the BLE transport immediately. Any AWS data sent after WiFi
 * credentials arrives into a dead transport and is lost.
 * esp_prov.py enforces the correct order automatically.
 */
#pragma once

#include <memory>
#include <string>

#include "lopcore/prov/aws_data_endpoint_handler.hpp"
#include "lopcore/prov/wifi_provisioning.hpp"

#include "application_state_machine.hpp"

// Note: No `using namespace` in headers — use fully-qualified names to avoid polluting includers.

class BleProvisioningHelper
{
public:
    explicit BleProvisioningHelper(ProvisioningContext &ctx);
    ~BleProvisioningHelper();

    bool start();
    void stop();

    /** Returns true when both WiFi credentials AND AWS data have been received. */
    bool isComplete() const;

    const std::string &getLastError() const { return lastError_; }

private:
    ProvisioningContext &ctx_;
    std::shared_ptr<lopcore::prov::WiFiProvisioning> wifiProv_;
    std::shared_ptr<lopcore::prov::AwsDataEndpointHandler> awsHandler_;
    std::string lastError_;
};
