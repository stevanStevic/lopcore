/**
 * @file main.cpp
 * @brief State Machine example demonstrating INTERNAL and EXTERNAL transitions
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>

#include "lopcore/logging/console_sink.hpp"
#include "lopcore/logging/logger.hpp"
#include "lopcore/state_machine/state_machine.hpp"

static const char *TAG = "state_machine_example";

enum class AppState
{
    INIT,
    RUNNING,
    PAUSED,
    ERROR,
    SHUTDOWN
};

const char *stateToString(AppState state)
{
    switch (state)
    {
        case AppState::INIT:
            return "INIT";
        case AppState::RUNNING:
            return "RUNNING";
        case AppState::PAUSED:
            return "PAUSED";
        case AppState::ERROR:
            return "ERROR";
        case AppState::SHUTDOWN:
            return "SHUTDOWN";
        default:
            return "UNKNOWN";
    }
}

// INIT: INTERNAL transition after 3 steps
class InitState : public lopcore::IState<AppState>
{
public:
    explicit InitState(lopcore::StateMachine<AppState> *sm) : sm_(sm)
    {
    }

    void onEnter() override
    {
        LOPCORE_LOGI(TAG, "[INIT] Enter");
        steps_ = 0;
    }

    void update() override
    {
        steps_++;
        LOPCORE_LOGI(TAG, "[INIT] Step %d/3", steps_);
        if (steps_ >= 3)
        {
            LOPCORE_LOGI(TAG, "[INIT] INTERNAL transition -> RUNNING");
            sm_->transition(AppState::RUNNING);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    void onExit() override
    {
        LOPCORE_LOGI(TAG, "[INIT] Exit\n");
    }

    AppState getStateId() const override
    {
        return AppState::INIT;
    }

private:
    lopcore::StateMachine<AppState> *sm_;
    int steps_{0};
};

// RUNNING: NO internal transition, externally controlled
class RunningState : public lopcore::IState<AppState>
{
public:
    void onEnter() override
    {
        LOPCORE_LOGI(TAG, "[RUNNING] Enter");
        cycles_ = 0;
    }

    void update() override
    {
        cycles_++;
        LOPCORE_LOGI(TAG, "[RUNNING] Cycle %d", cycles_);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    void onExit() override
    {
        LOPCORE_LOGI(TAG, "[RUNNING] Exit (%d cycles)\n", cycles_);
    }

    AppState getStateId() const override
    {
        return AppState::RUNNING;
    }

private:
    int cycles_{0};
};

// PAUSED: NO internal transition, externally controlled
class PausedState : public lopcore::IState<AppState>
{
public:
    void onEnter() override
    {
        LOPCORE_LOGI(TAG, "[PAUSED] Enter");
    }

    void update() override
    {
        LOPCORE_LOGI(TAG, "[PAUSED] Waiting...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    void onExit() override
    {
        LOPCORE_LOGI(TAG, "[PAUSED] Exit\n");
    }

    AppState getStateId() const override
    {
        return AppState::PAUSED;
    }
};

// ERROR: INTERNAL transition after 3 recovery attempts
class ErrorState : public lopcore::IState<AppState>
{
public:
    explicit ErrorState(lopcore::StateMachine<AppState> *sm) : sm_(sm)
    {
    }

    void onEnter() override
    {
        LOPCORE_LOGE(TAG, "[ERROR] Enter - recovering");
        attempts_ = 0;
    }

    void update() override
    {
        attempts_++;
        LOPCORE_LOGW(TAG, "[ERROR] Recovery %d/3", attempts_);
        if (attempts_ >= 3)
        {
            LOPCORE_LOGI(TAG, "[ERROR] INTERNAL transition -> RUNNING");
            sm_->transition(AppState::RUNNING);
        }
        vTaskDelay(pdMS_TO_TICKS(800));
    }

    void onExit() override
    {
        LOPCORE_LOGI(TAG, "[ERROR] Exit\n");
    }

    AppState getStateId() const override
    {
        return AppState::ERROR;
    }

private:
    lopcore::StateMachine<AppState> *sm_;
    int attempts_{0};
};

// SHUTDOWN: Final state
class ShutdownState : public lopcore::IState<AppState>
{
public:
    void onEnter() override
    {
        LOPCORE_LOGI(TAG, "[SHUTDOWN] Enter");
    }

    void update() override
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    void onExit() override
    {
        LOPCORE_LOGI(TAG, "[SHUTDOWN] Exit");
    }

    AppState getStateId() const override
    {
        return AppState::SHUTDOWN;
    }
};

extern "C" void app_main(void)
{
    nvs_flash_init();

    auto &logger = lopcore::Logger::getInstance();
    logger.addSink(std::make_unique<lopcore::ConsoleSink>());
    // Logger level controlled via Kconfig

    LOPCORE_LOGI(TAG, "=== State Machine Example ===");
    LOPCORE_LOGI(TAG, "INTERNAL: States self-transition");
    LOPCORE_LOGI(TAG, "EXTERNAL: Main loop controls\n");

    lopcore::StateMachine<AppState> sm(AppState::INIT);

    sm.registerState(AppState::INIT, std::make_unique<InitState>(&sm));
    sm.registerState(AppState::RUNNING, std::make_unique<RunningState>());
    sm.registerState(AppState::PAUSED, std::make_unique<PausedState>());
    sm.registerState(AppState::ERROR, std::make_unique<ErrorState>(&sm));
    sm.registerState(AppState::SHUTDOWN, std::make_unique<ShutdownState>());

    sm.addTransitionRule(AppState::INIT, AppState::RUNNING);
    sm.addTransitionRule(AppState::RUNNING, AppState::PAUSED);
    sm.addTransitionRule(AppState::RUNNING, AppState::ERROR);
    sm.addTransitionRule(AppState::RUNNING, AppState::SHUTDOWN);
    sm.addTransitionRule(AppState::PAUSED, AppState::RUNNING);
    sm.addTransitionRule(AppState::ERROR, AppState::RUNNING);

    sm.addObserver([](AppState from, AppState to) {
        LOPCORE_LOGI(TAG, "\n>>> %s -> %s <<<\n", stateToString(from), stateToString(to));
    });

    // Phase 1: INIT (INTERNAL)
    LOPCORE_LOGI(TAG, "Phase 1: INIT auto-transitions");
    while (sm.getCurrentState() == AppState::INIT)
    {
        sm.update();
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Phase 2: RUNNING (EXTERNAL control)
    LOPCORE_LOGI(TAG, "Phase 2: RUNNING (EXTERNAL control)");
    for (int i = 0; i < 3; i++)
    {
        sm.update();
    }
    LOPCORE_LOGI(TAG, "Main: EXTERNAL transition RUNNING -> PAUSED");
    sm.transition(AppState::PAUSED);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Phase 3: PAUSED (EXTERNAL control)
    LOPCORE_LOGI(TAG, "Phase 3: PAUSED (EXTERNAL control)");
    for (int i = 0; i < 2; i++)
    {
        sm.update();
    }
    LOPCORE_LOGI(TAG, "Main: EXTERNAL transition PAUSED -> RUNNING");
    sm.transition(AppState::RUNNING);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Phase 4: RUNNING then ERROR
    LOPCORE_LOGI(TAG, "Phase 4: RUNNING then error");
    for (int i = 0; i < 2; i++)
    {
        sm.update();
    }
    LOPCORE_LOGI(TAG, "Main: EXTERNAL transition RUNNING -> ERROR");
    sm.transition(AppState::ERROR);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Phase 5: ERROR (INTERNAL recovery)
    LOPCORE_LOGI(TAG, "Phase 5: ERROR auto-recovers");
    while (sm.getCurrentState() == AppState::ERROR)
    {
        sm.update();
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Phase 6: Final RUNNING then SHUTDOWN
    LOPCORE_LOGI(TAG, "Phase 6: Final RUNNING then shutdown");
    sm.update();
    LOPCORE_LOGI(TAG, "Main: EXTERNAL transition RUNNING -> SHUTDOWN");
    sm.transition(AppState::SHUTDOWN);
    sm.update();

    LOPCORE_LOGI(TAG, "\n=== Complete ===");
    auto history = sm.getHistory();
    LOPCORE_LOGI(TAG, "State History (%zu):", history.size());
    for (size_t i = 0; i < history.size(); i++)
    {
        LOPCORE_LOGI(TAG, "  %zu. %s", i + 1, stateToString(history[i]));
    }
    LOPCORE_LOGI(TAG, "\nINTERNAL: INIT->RUNNING, ERROR->RUNNING");
    LOPCORE_LOGI(TAG, "EXTERNAL: All others");

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
