/**
 * @file factory_reset_state.hpp
 * @brief FACTORY_RESET state: Erase NVS and restart.
 */

#pragma once

#include "lopcore/state_machine/state_machine.hpp"

#include "application_state_machine.hpp"

class FactoryResetState : public lopcore::IState<ApplicationState>
{
public:
    explicit FactoryResetState(lopcore::StateMachine<ApplicationState> *sm, ProvisioningContext &ctx);

    void onEnter() override;
    void update() override;
    void onExit() override;
    ApplicationState getStateId() const override
    {
        return ApplicationState::FACTORY_RESET;
    }

private:
    lopcore::StateMachine<ApplicationState> *sm_;
    ProvisioningContext &ctx_;
};

#endif // FACTORY_RESET_STATE_HPP
