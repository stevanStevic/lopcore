/**
 * @file aws_helper.hpp
 * @brief Phase 2 provisioning: AWS IoT Fleet Provisioning workflow.
 */
#pragma once

#include <memory>
#include <string>

#include "lopcore/prov/aws_fleet_provisioner.hpp"
#include "lopcore/prov/certificate_manager.hpp"

#include "application_state_machine.hpp"
#include "provisioning/provisioning_mqtt_adapter.hpp"

// Note: No `using namespace` in headers — use fully-qualified names.

class AwsProvisioningHelper
{
public:
    explicit AwsProvisioningHelper(ProvisioningContext &ctx);
    ~AwsProvisioningHelper() = default;

    /** Run full Fleet Provisioning workflow (blocking, ~30-60s). */
    bool provision();

    bool succeeded() const { return succeeded_; }
    const std::string &getThingName() const { return thingName_; }
    const std::string &getLastError() const { return lastError_; }

    /** Delete single-use claim credentials from NVS and PKCS#11 (security hygiene). */
    void deleteClaimCredentials();

private:
    ProvisioningContext &ctx_;
    std::shared_ptr<ProvisioningMqttAdapter> mqttAdapter_;
    std::shared_ptr<lopcore::prov::CertificateManager> certManager_;
    bool succeeded_{false};
    std::string lastError_;
    std::string thingName_;
};
