#pragma once
#include <memory>
#include <string>
#include "lopcore/state_machine/istate.hpp"
#include "application_state_machine.hpp"
#include "provisioning/ble_helper.hpp"
#include "provisioning/aws_helper.hpp"

class ConfigurationState : public lopcore::IState<ApplicationState>
{
public:
    ConfigurationState(lopcore::StateMachine<ApplicationState> *sm, ProvisioningContext &ctx);
    ~ConfigurationState() override;

    void onEnter() override;
    void update() override;
    void onExit() override;
    ApplicationState getStateId() const override { return ApplicationState::CONFIGURATION; }

private:
    enum class Phase
    {
        BLE_PROVISIONING,
        AWAITING_WIFI,
        AWS_PROVISIONING,
        SUCCESS_CLEANUP,
        FAILED_RETRY_WAIT,
        ABORTED
    };

    lopcore::StateMachine<ApplicationState> *sm_;
    ProvisioningContext &ctx_;
    std::unique_ptr<BleProvisioningHelper> bleHelper_;
    std::unique_ptr<AwsProvisioningHelper> awsHelper_;
    Phase currentPhase_{Phase::BLE_PROVISIONING};
    uint32_t phaseStartTime_{0};

    void startPhase(Phase phase);
    bool hasElapsed(uint32_t ms) const;
    bool isPhaseTimedOut(uint32_t timeoutMs) const;
    void handlePhaseFailure(const std::string &reason);
    void logPhaseTransition(Phase from, Phase to);
};
