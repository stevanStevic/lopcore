/**
 * @file mqtt_budget.hpp
 * @brief MQTT message budgeting for flood prevention
 *
 * Inspired by ESP RainMaker's MQTT budgeting system. Prevents message flooding
 * by limiting the number of messages that can be published within a time window.
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <atomic>
#include <mutex>

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

#include "mqtt_config.hpp"

namespace lopcore
{
namespace mqtt
{

/**
 * @brief MQTT message budget manager
 *
 * Implements a token bucket algorithm for rate limiting MQTT publishes.
 * Budget is consumed on each publish and automatically restored over time.
 *
 * Thread-safe: Can be called from multiple tasks concurrently
 */
class MqttBudget
{
public:
    /**
     * @brief Construct budget manager with configuration
     * @param config Budget configuration
     */
    explicit MqttBudget(const BudgetConfig &config);

    /**
     * @brief Destructor - stops revive timer
     */
    ~MqttBudget();

    // Non-copyable
    MqttBudget(const MqttBudget &) = delete;
    MqttBudget &operator=(const MqttBudget &) = delete;

    /**
     * @brief Check if budget is available for publishing
     * @return True if at least 1 message budget available
     */
    bool isAvailable() const;

    /**
     * @brief Consume budget for a message
     * @param count Number of budget units to consume (default 1)
     * @return True if budget consumed successfully, false if insufficient
     */
    bool consume(uint8_t count = 1);

    /**
     * @brief Manually restore budget
     * @param count Number of budget units to restore
     */
    void restore(uint8_t count);

    /**
     * @brief Get remaining budget
     * @return Current budget value
     */
    int32_t getRemaining() const;

    /**
     * @brief Reset budget to default value
     */
    void reset();

    /**
     * @brief Start automatic budget restoration timer
     * @return ESP_OK on success
     */
    esp_err_t start();

    /**
     * @brief Stop automatic budget restoration timer
     * @return ESP_OK on success
     */
    esp_err_t stop();

    /**
     * @brief Check if budget manager is enabled
     * @return True if enabled
     */
    bool isEnabled() const
    {
        return config_.enabled;
    }

private:
    /**
     * @brief Timer callback for budget restoration
     * @param timerHandle FreeRTOS timer handle
     */
    static void reviveTimerCallback(TimerHandle_t timerHandle);

    /**
     * @brief Internal budget restoration logic
     */
    void reviveBudget();

    const BudgetConfig config_;   ///< Budget configuration
    std::atomic<int32_t> budget_; ///< Current budget (atomic for lock-free reads)
    mutable std::mutex mutex_;    ///< Mutex for budget modifications
    TimerHandle_t reviveTimer_;   ///< FreeRTOS timer for auto-restore
};

} // namespace mqtt
} // namespace lopcore
