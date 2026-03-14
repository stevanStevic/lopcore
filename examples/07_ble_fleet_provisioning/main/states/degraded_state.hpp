/**
 * @file degraded_state.hpp
 * @brief DEGRADED state — handles WiFi reconnection.
 *
 * Entered when WiFi connection is lost (e.g., from NOMINAL state on disconnect event).
 * Attempts to reconnect WiFi in a polling loop.
 * On reconnect success → NOMINAL.
 * On timeout (default 30s) → INIT (which re-checks provisioning and routes accordingly).
 * Does NOT transition to FACTORY_RESET on timeout.
 */
#pragma once

#include "lopcore/state_machine/istate.hpp"
#include "application_state_machine.hpp"

class DegradedState : public lopcore::IState<ApplicationState>
{
public:
    DegradedState(lopcore::StateMachine<ApplicationState> *sm, ProvisioningContext &ctx);

    void onEnter() override;
    void update() override;
    void onExit() override;
    ApplicationState getStateId() const override { return ApplicationState::DEGRADED; }

private:
    lopcore::StateMachine<ApplicationState> *sm_;
    ProvisioningContext &ctx_;
    uint32_t enterTimeMs_{0};

    // Timeout before giving up and returning to INIT (default 30s)
    static constexpr uint32_t RECONNECT_TIMEOUT_MS = 30000;
};
