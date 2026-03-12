/**
 * @file init_state.hpp
 * @brief INIT state: System startup and provisioning status check.
 */

#pragma once

#include "lopcore/state_machine/state_machine.hpp"

#include "application_state_machine.hpp"

class InitState : public lopcore::IState<ApplicationState>
{
public:
    explicit InitState(lopcore::StateMachine<ApplicationState> *sm, ProvisioningContext &ctx);

    void onEnter() override;
    void update() override;
    void onExit() override;
    ApplicationState getStateId() const override
    {
        return ApplicationState::INIT;
    }

private:
    lopcore::StateMachine<ApplicationState> *sm_;
    ProvisioningContext &ctx_;
};

#endif // INIT_STATE_HPP
