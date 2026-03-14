/**
 * @file application_state_machine.hpp
 * @brief Application state machine for the provisioning example.
 */
#pragma once

#include <memory>
#include <optional>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lopcore/logging/logger.hpp"
#include "lopcore/prov/aws_fleet_provisioner.hpp"
#include "lopcore/prov/certificate_manager.hpp"
#include "lopcore/state_machine/state_machine.hpp"
#include "lopcore/storage/nvs_storage.hpp"

enum class ApplicationState
{
    INIT,
    CONFIGURATION,
    NOMINAL,
    DEGRADED,
    FACTORY_RESET
};

struct ProvisioningContext
{
    std::shared_ptr<lopcore::NvsStorage> awsNvs;

    std::string deviceId;
    std::string ble_service_name{"PROV_EXAMPLE"};
    std::string csr_subject_name{"CN=ExampleDevice"};
    std::string default_aws_endpoint{};

    struct ConfigurationMetrics
    {
        int retryCount{0};
        int maxRetries{3};
        std::string lastPhaseError{};
        uint32_t lastPhaseStartTime{0};

        uint32_t ble_timeout_ms{300000};
        uint32_t aws_timeout_ms{300000};
        uint32_t wifi_stabilization_ms{2000};

        void reset()
        {
            retryCount = 0;
            lastPhaseError.clear();
            lastPhaseStartTime = 0;
        }
    } config;

    struct ProvisioningData
    {
        std::optional<std::string> thing_name;
        std::optional<std::string> aws_endpoint;
        std::optional<std::string> claim_cert;
        std::optional<std::string> claim_key;
        std::optional<std::string> root_ca;
        std::optional<std::string> provisioning_template;
    } prov_data;
};

inline const char *stateToString(ApplicationState state)
{
    switch (state)
    {
        case ApplicationState::INIT:          return "INIT";
        case ApplicationState::CONFIGURATION: return "CONFIGURATION";
        case ApplicationState::NOMINAL:       return "NOMINAL";
        case ApplicationState::DEGRADED:      return "DEGRADED";
        case ApplicationState::FACTORY_RESET: return "FACTORY_RESET";
        default:                              return "UNKNOWN";
    }
}

std::unique_ptr<lopcore::StateMachine<ApplicationState>>
createApplicationStateMachine(ProvisioningContext &ctx);
