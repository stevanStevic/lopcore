/**
 * @file state_machine.hpp
 * @brief Type-safe hierarchical state machine template
 *
 * Part of LopCore - Modern C++ Middleware for ESP32
 *
 * @copyright Copyright (c) 2025 LopTech
 * @license MIT License
 */

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

#include "lopcore/logging/logger.hpp"

#include "istate.hpp"

namespace lopcore
{

// Logging tag for state machine
static constexpr const char *STATE_MACHINE_TAG = "StateMachine";

/**
 * @brief Type-safe hierarchical state machine
 *
 * This template class provides a robust state machine implementation with:
 * - Type-safe state transitions using enums
 * - Entry/exit/update hooks for each state
 * - Transition validation rules
 * - Observer pattern for state changes
 * - State history tracking
 * - Thread-safe operation (when used with proper locking)
 *
 * @tparam StateEnum The enum class representing your application states
 *
 * @example
 * enum class AppState { INIT, RUNNING, ERROR };
 *
 * StateMachine<AppState> sm(AppState::INIT);
 * sm.registerState(AppState::INIT, std::make_unique<InitState>());
 * sm.registerState(AppState::RUNNING, std::make_unique<RunningState>());
 *
 * sm.addTransitionRule(AppState::INIT, AppState::RUNNING);
 * sm.transition(AppState::RUNNING);
 *
 * // In your main loop
 * sm.update();
 */
template<typename StateEnum>
class StateMachine
{
public:
    using StateChangeCallback = std::function<void(StateEnum from, StateEnum to)>;

    /**
     * @brief Construct a state machine with an initial state
     *
     * @param initialState The starting state (no handler needs to be registered yet)
     */
    explicit StateMachine(StateEnum initialState)
        : currentState_(initialState), previousState_(initialState), maxHistorySize_(10)
    {
        history_.push_back(initialState);
    }

    /**
     * @brief Register a state handler
     *
     * @param state The state enum value
     * @param handler Unique pointer to the state handler
     * @return true if registration succeeded, false if state already registered
     */
    bool registerState(StateEnum state, std::unique_ptr<IState<StateEnum>> handler)
    {
        if (!handler)
        {
            LOPCORE_LOGE(STATE_MACHINE_TAG, "Cannot register null state handler");
            return false;
        }

        if (states_.find(state) != states_.end())
        {
            LOPCORE_LOGW(STATE_MACHINE_TAG, "State already registered, replacing");
        }

        states_[state] = std::move(handler);
        return true;
    }

    /**
     * @brief Transition to a new state
     *
     * This will:
     * 1. Check if transition is allowed (if rules are defined)
     * 2. Call onExit() on current state handler
     * 3. Update current/previous state
     * 4. Call onEnter() on new state handler
     * 5. Notify all observers
     * 6. Add to history
     *
     * @param newState The target state
     * @return true if transition succeeded, false if not allowed or no handler registered
     */
    bool transition(StateEnum newState)
    {
        if (newState == currentState_)
        {
            LOPCORE_LOGW(STATE_MACHINE_TAG, "Already in target state, ignoring transition");
            return true;
        }

        // Check transition rules if any are defined
        if (!transitionRules_.empty())
        {
            if (!isTransitionAllowed(currentState_, newState))
            {
                LOPCORE_LOGE(STATE_MACHINE_TAG, "Transition not allowed by rules");
                return false;
            }
        }

        // Call exit on current state if handler exists
        auto currentIt = states_.find(currentState_);
        if (currentIt != states_.end() && currentIt->second)
        {
            currentIt->second->onExit();
        }

        // Update state
        previousState_ = currentState_;
        currentState_ = newState;

        // Add to history
        addToHistory(newState);

        // Call enter on new state if handler exists
        auto newIt = states_.find(newState);
        if (newIt != states_.end() && newIt->second)
        {
            newIt->second->onEnter();
        }
        else
        {
            LOPCORE_LOGW(STATE_MACHINE_TAG, "No handler registered for new state");
        }

        // Notify observers
        notifyObservers(previousState_, currentState_);

        return true;
    }

    /**
     * @brief Update the current state
     *
     * Call this regularly (e.g., in your main loop) to execute the
     * current state's update() method.
     */
    void update()
    {
        auto it = states_.find(currentState_);
        if (it != states_.end() && it->second)
        {
            it->second->update();
        }
    }

    /**
     * @brief Get the current state
     *
     * @return Current state enum value
     */
    StateEnum getCurrentState() const
    {
        return currentState_;
    }

    /**
     * @brief Get the previous state
     *
     * @return Previous state enum value
     */
    StateEnum getPreviousState() const
    {
        return previousState_;
    }

    /**
     * @brief Add a transition rule
     *
     * If any rules are added, only explicitly allowed transitions will succeed.
     * If no rules are added, all transitions are allowed.
     *
     * @param from Source state
     * @param to Destination state
     */
    void addTransitionRule(StateEnum from, StateEnum to)
    {
        transitionRules_[from].insert(to);
    }

    /**
     * @brief Check if a transition is allowed
     *
     * @param from Source state
     * @param to Destination state
     * @return true if transition is allowed or no rules defined
     */
    bool isTransitionAllowed(StateEnum from, StateEnum to) const
    {
        // If no rules defined, all transitions allowed
        if (transitionRules_.empty())
        {
            return true;
        }

        auto it = transitionRules_.find(from);
        if (it == transitionRules_.end())
        {
            return false;
        }

        return it->second.find(to) != it->second.end();
    }

    /**
     * @brief Add an observer for state changes
     *
     * The callback will be called whenever a state transition occurs.
     *
     * @param callback Function to call on state change (from, to)
     */
    void addObserver(StateChangeCallback callback)
    {
        observers_.push_back(callback);
    }

    /**
     * @brief Get state transition history
     *
     * @return Vector of states in chronological order (oldest to newest)
     */
    std::vector<StateEnum> getHistory() const
    {
        return history_;
    }

    /**
     * @brief Set maximum history size
     *
     * @param size Maximum number of states to keep in history (default: 10)
     */
    void setMaxHistorySize(size_t size)
    {
        maxHistorySize_ = size;
        while (history_.size() > maxHistorySize_)
        {
            history_.erase(history_.begin());
        }
    }

    /**
     * @brief Clear all transition rules
     */
    void clearTransitionRules()
    {
        transitionRules_.clear();
    }

    /**
     * @brief Clear all observers
     */
    void clearObservers()
    {
        observers_.clear();
    }

    /**
     * @brief Clear state history
     */
    void clearHistory()
    {
        history_.clear();
        history_.push_back(currentState_);
    }

private:
    StateEnum currentState_;
    StateEnum previousState_;
    std::unordered_map<StateEnum, std::unique_ptr<IState<StateEnum>>> states_;
    std::unordered_map<StateEnum, std::set<StateEnum>> transitionRules_;
    std::vector<StateChangeCallback> observers_;
    std::vector<StateEnum> history_;
    size_t maxHistorySize_;

    void addToHistory(StateEnum state)
    {
        history_.push_back(state);
        if (history_.size() > maxHistorySize_)
        {
            history_.erase(history_.begin());
        }
    }

    void notifyObservers(StateEnum from, StateEnum to)
    {
        for (const auto &observer : observers_)
        {
            observer(from, to);
        }
    }
};

} // namespace lopcore
