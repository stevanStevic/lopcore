/**
 * @file nominal_state.hpp
 * @brief NOMINAL state: Normal operation, application-ready.
 */

#pragma once

#include "lopcore/state_machine/state_machine.hpp"

#include "application_state_machine.hpp"

class NominalState : public lopcore::IState<ApplicationState>
{
public:
    explicit NominalState(lopcore::StateMachine<ApplicationState> *sm, ProvisioningContext &ctx);

    void onEnter() override;
    void update() override;
    void onExit() override;
    ApplicationState getStateId() const override
    {
        return ApplicationState::NOMINAL;
    }

private:
    lopcore::StateMachine<ApplicationState> *sm_;
    ProvisioningContext &ctx_;
    uint32_t loopCount_{0};
};

#endif // NOMINAL_STATE_HPP
