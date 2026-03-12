/**
 * @file configuration_state.hpp
 * @brief CONFIGURATION state: All provisioning (BLE + AWS) with retry logic.
 *
 * This state encapsulates the entire provisioning workflow:
 *  - Phase 1: BLE WiFi + AWS credential delivery
 *  - Phase 2: AWS IoT Fleet Provisioning
 *  - Retry: On failure, retry up to maxRetries times before moving to FACTORY_RESET
 *
 * Internally uses a Phase enum to track provisioning steps.
 */

#pragma once

#include <memory>

#include "lopcore/state_machine/state_machine.hpp"

#include "application_state_machine.hpp"
#include "provisioning_helpers.hpp"

class ConfigurationState : public lopcore::IState<ApplicationState>
{
public:
    explicit ConfigurationState(lopcore::StateMachine<ApplicationState> *sm, ProvisioningContext &ctx);
    ~ConfigurationState();

    void onEnter() override;
    void update() override;
    void onExit() override;
    ApplicationState getStateId() const override
    {
        return ApplicationState::CONFIGURATION;
    }

private:
    enum class Phase
    {
        BLE_PROVISIONING,  // Phase 1: BLE + WiFi provisioning
        AWAITING_WIFI,     // Brief stabilization
        AWS_PROVISIONING,  // Phase 2: AWS Fleet Provisioning
        SUCCESS_CLEANUP,   // Delete claim credentials
        FAILED_RETRY_WAIT, // Wait before retry
        ABORTED            // Max retries exceeded
    };

    // Phase management
    Phase currentPhase_{Phase::BLE_PROVISIONING};
    uint32_t phaseStartTime_{0};

    // State machine reference
    lopcore::StateMachine<ApplicationState> *sm_;
    ProvisioningContext &ctx_;

    // Helpers
    std::unique_ptr<BleProvisioningHelper> bleHelper_;
    std::unique_ptr<AwsProvisioningHelper> awsHelper_;

    // Helper methods
    bool hasElapsed(uint32_t ms) const;
    bool isPhaseTimedOut(uint32_t timeoutMs) const;
    void startPhase(Phase phase);
    void handlePhaseFailure(const std::string &reason);
    void logPhaseTransition(Phase from, Phase to);
};

#endif // CONFIGURATION_STATE_HPP
