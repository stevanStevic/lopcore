#pragma once
#include "lopcore/state_machine/istate.hpp"
#include "application_state_machine.hpp"

class InitState : public lopcore::IState<ApplicationState>
{
public:
    InitState(lopcore::StateMachine<ApplicationState> *sm, ProvisioningContext &ctx);

    void onEnter() override;
    void update() override;
    void onExit() override;
    ApplicationState getStateId() const override { return ApplicationState::INIT; }

private:
    lopcore::StateMachine<ApplicationState> *sm_;
    ProvisioningContext &ctx_;
};
