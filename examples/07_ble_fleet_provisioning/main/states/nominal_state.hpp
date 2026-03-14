#pragma once
#include "lopcore/state_machine/istate.hpp"
#include "application_state_machine.hpp"

class NominalState : public lopcore::IState<ApplicationState>
{
public:
    NominalState(lopcore::StateMachine<ApplicationState> *sm, ProvisioningContext &ctx);
    void onEnter() override;
    void update() override;
    void onExit() override;
    ApplicationState getStateId() const override { return ApplicationState::NOMINAL; }

private:
    lopcore::StateMachine<ApplicationState> *sm_;
    ProvisioningContext &ctx_;
    uint32_t loopCount_{0};
};
