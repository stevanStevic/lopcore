/**
 * @file application_state_machine.hpp
 * @brief Application state machine for the provisioning example.
 *
 * Defines the application lifecycle states:
 *  - INIT: System startup, check provisioning status
 *  - CONFIGURATION: BLE WiFi provisioning + AWS Fleet Provisioning (with retry)
 *  - NOMINAL: Normal operation (app-ready)
 *  - DEGRADED: [Optional] Fallback when WiFi unavailable
 *  - FACTORY_RESET: Erase NVS and restart
 *
 * The pattern mirrors production systems: centralized state machine with
 * phases encoded as an enum within CONFIGURATION state.
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

// Forward declarations
class ProvisioningMqttAdapter;

// ============================================================================
// Application State Enumeration
// ============================================================================

enum class ApplicationState
{
    INIT,
    CONFIGURATION, // All provisioning (Phase 1 BLE + Phase 2 AWS) with retry
    NOMINAL,       // Normal operation
    DEGRADED,      // [Optional] Fallback when WiFi unavailable
    FACTORY_RESET
};

// ============================================================================
// Provisioning Context
//
// Shared state/resources passed to all states. States read/write this to
// coordinate provisioning flow and track progress.
// ============================================================================

struct ProvisioningContext
{
    // Shared NVS storage instances
    std::shared_ptr<lopcore::NvsStorage> awsNvs;

    // Configuration values (read from NVS or app)
    std::string deviceId;
    std::string ble_service_name{"PROV_EXAMPLE"};
    std::string csr_subject_name{"CN=ExampleDevice"};
    std::string default_aws_endpoint{};

    // Provisioning state tracking
    struct ConfigurationMetrics
    {
        int retryCount{0};
        int maxRetries{3};
        std::string lastPhaseError{};
        uint32_t lastPhaseStartTime{0};

        // Timeouts (milliseconds)
        uint32_t ble_timeout_ms{300000};      // 5 minutes
        uint32_t aws_timeout_ms{300000};      // 5 minutes
        uint32_t wifi_stabilization_ms{2000}; // 2 seconds

        void reset()
        {
            retryCount = 0;
            lastPhaseError.clear();
            lastPhaseStartTime = 0;
        }
    } config;

    // Loaded provisioning data (from NVS or BLE)
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

// ============================================================================
// Helper function to convert state to string
// ============================================================================

inline const char *stateToString(ApplicationState state)
{
    switch (state)
    {
        case ApplicationState::INIT:
            return "INIT";
        case ApplicationState::CONFIGURATION:
            return "CONFIGURATION";
        case ApplicationState::NOMINAL:
            return "NOMINAL";
        case ApplicationState::DEGRADED:
            return "DEGRADED";
        case ApplicationState::FACTORY_RESET:
            return "FACTORY_RESET";
        default:
            return "UNKNOWN";
    }
}

// ============================================================================
// State Machine Factory
//
// Create and initialize the application state machine with all state
// implementations registered.
// ============================================================================

std::unique_ptr<lopcore::StateMachine<ApplicationState>>
createApplicationStateMachine(ProvisioningContext &ctx);

#endif // APPLICATION_STATE_MACHINE_HPP
