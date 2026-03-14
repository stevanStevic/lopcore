/**
 * @file application_state_machine.cpp
 * @brief State machine factory: registers states, transition rules, and observer.
 */
#include "application_state_machine.hpp"

#include "esp_timer.h"

#include "states/init_state.hpp"
#include "states/configuration_state.hpp"
#include "states/nominal_state.hpp"
#include "states/degraded_state.hpp"
#include "states/factory_reset_state.hpp"

std::unique_ptr<lopcore::StateMachine<ApplicationState>>
createApplicationStateMachine(ProvisioningContext &ctx)
{
    // StateMachine constructor takes the initial state.
    // NOTE: It does NOT call onEnter() on the initial state.
    // Banner logging lives in app_main() instead of InitState::onEnter().
    auto sm = std::make_unique<lopcore::StateMachine<ApplicationState>>(ApplicationState::INIT);

    // Register all state handlers
    sm->registerState(ApplicationState::INIT,
                      std::make_unique<InitState>(sm.get(), ctx));
    sm->registerState(ApplicationState::CONFIGURATION,
                      std::make_unique<ConfigurationState>(sm.get(), ctx));
    sm->registerState(ApplicationState::NOMINAL,
                      std::make_unique<NominalState>(sm.get(), ctx));
    sm->registerState(ApplicationState::DEGRADED,
                      std::make_unique<DegradedState>(sm.get(), ctx));
    sm->registerState(ApplicationState::FACTORY_RESET,
                      std::make_unique<FactoryResetState>(sm.get(), ctx));

    // Register allowed transitions.
    // Once any rule is added, ONLY listed transitions are permitted.
    sm->addTransitionRule(ApplicationState::INIT,          ApplicationState::CONFIGURATION);
    sm->addTransitionRule(ApplicationState::INIT,          ApplicationState::NOMINAL);
    sm->addTransitionRule(ApplicationState::CONFIGURATION, ApplicationState::NOMINAL);
    sm->addTransitionRule(ApplicationState::CONFIGURATION, ApplicationState::FACTORY_RESET);
    sm->addTransitionRule(ApplicationState::NOMINAL,       ApplicationState::DEGRADED);
    sm->addTransitionRule(ApplicationState::NOMINAL,       ApplicationState::FACTORY_RESET);
    sm->addTransitionRule(ApplicationState::DEGRADED,      ApplicationState::NOMINAL);
    sm->addTransitionRule(ApplicationState::DEGRADED,      ApplicationState::INIT);

    // Register logging observer — logs every transition with elapsed time.
    sm->addObserver([](ApplicationState from, ApplicationState to) {
        static uint32_t lastTransitionMs = 0;
        uint32_t nowMs = static_cast<uint32_t>(esp_timer_get_time() / 1000);
        uint32_t elapsed = (lastTransitionMs > 0) ? (nowMs - lastTransitionMs) : 0;
        LOPCORE_LOGI("FSM", "[FSM] %s → %s  (+%ums)",
                     stateToString(from), stateToString(to), elapsed);
        lastTransitionMs = nowMs;
    });

    return sm;
}
