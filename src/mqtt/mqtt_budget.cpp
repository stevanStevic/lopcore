/**
 * @file mqtt_budget.cpp
 * @brief Implementation of MQTT message budgeting
 *
 * @copyright Copyright (c) 2025
 */

#include "lopcore/mqtt/mqtt_budget.hpp"

#include <algorithm>

#include "lopcore/logging/logger.hpp"

static const char *TAG = "mqtt_budget";

namespace lopcore
{
namespace mqtt
{

MqttBudget::MqttBudget(const BudgetConfig &config)
    : config_(config), budget_(config.defaultBudget), reviveTimer_(nullptr)
{
    if (!config_.enabled)
    {
        LOPCORE_LOGW(TAG, "Budget management disabled");
        return;
    }

    LOPCORE_LOGI(TAG, "Budget initialized: default=%d, max=%d, revive=%d per %llds", config_.defaultBudget,
                 config_.maxBudget, config_.reviveCount, config_.revivePeriod.count());
}

MqttBudget::~MqttBudget()
{
    stop();
}

bool MqttBudget::isAvailable() const
{
    if (!config_.enabled)
    {
        return true; // Always available if disabled
    }

    return budget_.load(std::memory_order_relaxed) > 0;
}

bool MqttBudget::consume(uint8_t count)
{
    if (!config_.enabled)
    {
        return true; // Always succeed if disabled
    }

    std::lock_guard<std::mutex> lock(mutex_);

    int32_t currentBudget = budget_.load(std::memory_order_relaxed);

    if (currentBudget < count)
    {
        LOPCORE_LOGW(TAG, "Budget exhausted: requested=%d, available=%d", count, currentBudget);
        return false;
    }

    int32_t newBudget = currentBudget - count;
    budget_.store(newBudget, std::memory_order_relaxed);

    LOPCORE_LOGD(TAG, "Budget consumed: %d -> %d", currentBudget, newBudget);

    return true;
}

void MqttBudget::restore(uint8_t count)
{
    if (!config_.enabled)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    int32_t currentBudget = budget_.load(std::memory_order_relaxed);
    int32_t newBudget = std::min(currentBudget + count, config_.maxBudget);

    budget_.store(newBudget, std::memory_order_relaxed);

    LOPCORE_LOGD(TAG, "Budget restored: %d -> %d", currentBudget, newBudget);
}

int32_t MqttBudget::getRemaining() const
{
    return budget_.load(std::memory_order_relaxed);
}

void MqttBudget::reset()
{
    if (!config_.enabled)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    budget_.store(config_.defaultBudget, std::memory_order_relaxed);

    LOPCORE_LOGI(TAG, "Budget reset to %d", config_.defaultBudget);
}

esp_err_t MqttBudget::start()
{
    if (!config_.enabled)
    {
        return ESP_OK;
    }

    if (reviveTimer_ != nullptr)
    {
        LOPCORE_LOGW(TAG, "Timer already started");
        return ESP_ERR_INVALID_STATE;
    }

    // Create periodic timer
    reviveTimer_ = xTimerCreate("mqtt_budget", pdMS_TO_TICKS(config_.revivePeriod.count() * 1000), // Convert
                                                                                                   // seconds
                                                                                                   // to ms
                                pdTRUE, // Auto-reload
                                this,   // Timer ID (passed to callback)
                                reviveTimerCallback);

    if (reviveTimer_ == nullptr)
    {
        LOPCORE_LOGE(TAG, "Failed to create revive timer");
        return ESP_ERR_NO_MEM;
    }

    if (xTimerStart(reviveTimer_, 0) != pdPASS)
    {
        LOPCORE_LOGE(TAG, "Failed to start revive timer");
        xTimerDelete(reviveTimer_, 0);
        reviveTimer_ = nullptr;
        return ESP_FAIL;
    }

    LOPCORE_LOGI(TAG, "Budget revival started: +%d every %llds", config_.reviveCount,
                 config_.revivePeriod.count());

    return ESP_OK;
}

esp_err_t MqttBudget::stop()
{
    if (reviveTimer_ == nullptr)
    {
        return ESP_OK;
    }

    if (xTimerStop(reviveTimer_, pdMS_TO_TICKS(1000)) != pdPASS)
    {
        LOPCORE_LOGW(TAG, "Failed to stop revive timer");
    }

    xTimerDelete(reviveTimer_, pdMS_TO_TICKS(1000));
    reviveTimer_ = nullptr;

    LOPCORE_LOGI(TAG, "Budget revival stopped");

    return ESP_OK;
}

void MqttBudget::reviveTimerCallback(TimerHandle_t timerHandle)
{
    // Get MqttBudget instance from timer ID
    MqttBudget *budget = static_cast<MqttBudget *>(pvTimerGetTimerID(timerHandle));

    if (budget != nullptr)
    {
        budget->reviveBudget();
    }
}

void MqttBudget::reviveBudget()
{
    restore(config_.reviveCount);
}

} // namespace mqtt
} // namespace lopcore
