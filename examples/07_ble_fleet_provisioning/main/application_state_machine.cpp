/**
 * @file application_state_machine.cpp
 * @brief State machine factory and initialization.
 */

#include "application_state_machine.hpp"

#include "configuration_state.hpp"
#include "factory_reset_state.hpp"
#include "init_state.hpp"
#include "nominal_state.hpp"

std::unique_ptr<lopcore::StateMachine<ApplicationState>>
createApplicationStateMachine(ProvisioningContext &ctx)
{
    auto sm = std::make_unique<lopcore::StateMachine<ApplicationState>>();

    // Register all state implementations
    sm->registerState(std::make_unique<InitState>(sm.get(), ctx));
    sm->registerState(std::make_unique<ConfigurationState>(sm.get(), ctx));
    sm->registerState(std::make_unique<NominalState>(sm.get(), ctx));
    sm->registerState(std::make_unique<FactoryResetState>(sm.get(), ctx));

    return sm;
}
