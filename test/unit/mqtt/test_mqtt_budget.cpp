/**
 * @file test_mqtt_budget.cpp
 * @brief Unit tests for MQTT message budgeting
 *
 * Note: These tests focus on the budget logic without FreeRTOS timer functionality.
 * Timer-based restoration is tested in integration tests on hardware.
 */

#include <atomic>
#include <chrono>
#include <thread>

#include <gtest/gtest.h>
#include <mqtt/mqtt_config.hpp>

using namespace lopcore::mqtt;
using namespace std::chrono_literals;

// Simple budget manager for testing (without FreeRTOS dependencies)
class TestBudget
{
public:
    explicit TestBudget(const BudgetConfig &config)
        : config_(config), budget_(config.enabled ? config.defaultBudget : INT32_MAX),
          enabled_(config.enabled)
    {
    }

    bool isEnabled() const
    {
        return enabled_;
    }

    bool isAvailable() const
    {
        if (!enabled_)
            return true;
        return budget_.load() > 0;
    }

    bool consume(uint8_t count = 1)
    {
        if (!enabled_)
            return true;

        std::lock_guard<std::mutex> lock(mutex_);
        int32_t current = budget_.load();
        if (current < count)
        {
            return false;
        }
        budget_.store(current - count);
        return true;
    }

    void restore(uint8_t count = 1)
    {
        if (!enabled_)
            return;

        std::lock_guard<std::mutex> lock(mutex_);
        int32_t current = budget_.load();
        int32_t newBudget = std::min(current + count, config_.maxBudget);
        budget_.store(newBudget);
    }

    void reset()
    {
        if (!enabled_)
            return;
        budget_.store(config_.defaultBudget);
    }

    int32_t getRemaining() const
    {
        return budget_.load();
    }

private:
    BudgetConfig config_;
    std::atomic<int32_t> budget_;
    std::mutex mutex_;
    bool enabled_;
};

// =============================================================================
// Budget Configuration Tests
// =============================================================================

TEST(MqttBudgetTest, ConstructionWithDefaultConfig)
{
    BudgetConfig config;
    config.enabled = true;
    config.defaultBudget = 100;
    config.maxBudget = 1024;

    TestBudget budget(config);

    EXPECT_TRUE(budget.isEnabled());
    EXPECT_EQ(budget.getRemaining(), 100);
}

TEST(MqttBudgetTest, ConstructionDisabled)
{
    BudgetConfig config;
    config.enabled = false;

    TestBudget budget(config);

    EXPECT_FALSE(budget.isEnabled());
    EXPECT_TRUE(budget.isAvailable()); // Always available when disabled
}

// =============================================================================
// Budget Consumption Tests
// =============================================================================

TEST(MqttBudgetTest, ConsumeSuccess)
{
    BudgetConfig config;
    config.enabled = true;
    config.defaultBudget = 100;
    config.maxBudget = 1024;

    TestBudget budget(config);

    EXPECT_TRUE(budget.consume(1));
    EXPECT_EQ(budget.getRemaining(), 99);

    EXPECT_TRUE(budget.consume(10));
    EXPECT_EQ(budget.getRemaining(), 89);
}

TEST(MqttBudgetTest, ConsumeExhaustion)
{
    BudgetConfig config;
    config.enabled = true;
    config.defaultBudget = 5;
    config.maxBudget = 1024;

    TestBudget budget(config);

    EXPECT_TRUE(budget.consume(3)); // 5 -> 2
    EXPECT_EQ(budget.getRemaining(), 2);

    EXPECT_TRUE(budget.consume(2)); // 2 -> 0
    EXPECT_EQ(budget.getRemaining(), 0);

    EXPECT_FALSE(budget.consume(1)); // Should fail
    EXPECT_EQ(budget.getRemaining(), 0);
}

TEST(MqttBudgetTest, ConsumeWhenDisabled)
{
    BudgetConfig config;
    config.enabled = false;

    TestBudget budget(config);

    // Should always succeed when disabled
    for (int i = 0; i < 1000; i++)
    {
        EXPECT_TRUE(budget.consume(1));
    }
}

// =============================================================================
// Budget Restoration Tests
// =============================================================================

TEST(MqttBudgetTest, ManualRestore)
{
    BudgetConfig config;
    config.enabled = true;
    config.defaultBudget = 50;
    config.maxBudget = 100;

    TestBudget budget(config);

    budget.consume(30); // 50 -> 20
    EXPECT_EQ(budget.getRemaining(), 20);

    budget.restore(10); // 20 -> 30
    EXPECT_EQ(budget.getRemaining(), 30);
}

TEST(MqttBudgetTest, RestoreRespectsCap)
{
    BudgetConfig config;
    config.enabled = true;
    config.defaultBudget = 90;
    config.maxBudget = 100;

    TestBudget budget(config);

    budget.restore(50); // Should cap at maxBudget
    EXPECT_EQ(budget.getRemaining(), 100);

    budget.restore(100); // Should still be capped
    EXPECT_EQ(budget.getRemaining(), 100);
}

TEST(MqttBudgetTest, ResetToDefault)
{
    BudgetConfig config;
    config.enabled = true;
    config.defaultBudget = 100;
    config.maxBudget = 1024;

    TestBudget budget(config);

    budget.consume(50);
    EXPECT_EQ(budget.getRemaining(), 50);

    budget.reset();
    EXPECT_EQ(budget.getRemaining(), 100);
}

// =============================================================================
// Availability Check Tests
// =============================================================================

TEST(MqttBudgetTest, IsAvailableTrue)
{
    BudgetConfig config;
    config.enabled = true;
    config.defaultBudget = 10;
    config.maxBudget = 1024;

    TestBudget budget(config);

    EXPECT_TRUE(budget.isAvailable());

    budget.consume(5);
    EXPECT_TRUE(budget.isAvailable());
}

TEST(MqttBudgetTest, IsAvailableFalse)
{
    BudgetConfig config;
    config.enabled = true;
    config.defaultBudget = 5;
    config.maxBudget = 1024;

    TestBudget budget(config);

    budget.consume(5); // Exhaust budget
    EXPECT_FALSE(budget.isAvailable());
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST(MqttBudgetTest, ConcurrentConsume)
{
    BudgetConfig config;
    config.enabled = true;
    config.defaultBudget = 1000;
    config.maxBudget = 1024;

    TestBudget budget(config);

    const int NUM_THREADS = 10;
    const int CONSUMES_PER_THREAD = 50;

    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < NUM_THREADS; i++)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < CONSUMES_PER_THREAD; j++)
            {
                if (budget.consume(1))
                {
                    successCount++;
                }
                std::this_thread::sleep_for(1ms);
            }
        });
    }

    for (auto &thread : threads)
    {
        thread.join();
    }

    // All consumes should succeed (1000 budget > 500 total consumes)
    EXPECT_EQ(successCount, NUM_THREADS * CONSUMES_PER_THREAD);
    EXPECT_EQ(budget.getRemaining(), 1000 - (NUM_THREADS * CONSUMES_PER_THREAD));
}

TEST(MqttBudgetTest, ConcurrentConsumeAndRestore)
{
    BudgetConfig config;
    config.enabled = true;
    config.defaultBudget = 50;
    config.maxBudget = 100;

    TestBudget budget(config);

    std::atomic<bool> running{true};
    std::atomic<int> consumeCount{0};
    std::atomic<int> restoreCount{0};

    // Consumer thread
    std::thread consumer([&]() {
        while (running)
        {
            if (budget.consume(1))
            {
                consumeCount++;
            }
            std::this_thread::sleep_for(1ms);
        }
    });

    // Restorer thread
    std::thread restorer([&]() {
        while (running)
        {
            budget.restore(2);
            restoreCount++;
            std::this_thread::sleep_for(5ms);
        }
    });

    // Run for a short time
    std::this_thread::sleep_for(100ms);
    running = false;

    consumer.join();
    restorer.join();

    // Should have consumed some messages
    EXPECT_GT(consumeCount, 0);
    EXPECT_GT(restoreCount, 0);

    // Budget should be within valid range
    int32_t remaining = budget.getRemaining();
    EXPECT_GE(remaining, 0);
    EXPECT_LE(remaining, 100);
}

// Note: FreeRTOS timer-based restoration is tested in integration tests
// running on actual hardware with ESP-IDF environment
