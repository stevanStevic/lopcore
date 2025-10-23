/**
 * @file istate.hpp
 * @brief State interface for the StateMachine template
 *
 * Part of LopCore - Modern C++ Middleware for ESP32
 *
 * @copyright Copyright (c) 2025 LopTech
 * @license MIT License
 */

#pragma once

namespace lopcore
{

/**
 * @brief Interface for state handlers in the StateMachine
 *
 * Each state in your application should implement this interface.
 * The state machine will call these methods at appropriate times.
 *
 * @tparam StateEnum The enum type representing your application states
 *
 * Example usage:
 * @code
 * enum class MyState { IDLE, RUNNING };
 *
 * class IdleState : public IState<MyState> {
 * public:
 *     void onEnter() override { ESP_LOGI("State", "Entering IDLE"); }
 *     void update() override { // Called every cycle }
 *     void onExit() override { ESP_LOGI("State", "Exiting IDLE"); }
 *     MyState getStateId() const override { return MyState::IDLE; }
 * };
 * @endcode
 */
template<typename StateEnum>
class IState
{
public:
    virtual ~IState() = default;

    /**
     * @brief Called once when entering this state
     *
     * Use this to initialize resources, start timers, subscribe to events, etc.
     */
    virtual void onEnter() = 0;

    /**
     * @brief Called repeatedly while in this state
     *
     * This is where the main state logic runs. It's called from the
     * StateMachine::update() method.
     */
    virtual void update() = 0;

    /**
     * @brief Called once when leaving this state
     *
     * Use this to cleanup resources, stop timers, unsubscribe from events, etc.
     */
    virtual void onExit() = 0;

    /**
     * @brief Get the enum value this state represents
     *
     * @return The StateEnum value for this state
     */
    virtual StateEnum getStateId() const = 0;
};

} // namespace lopcore
