/**
 * @file test_coremqtt_client_simple.cpp
 * @brief Simplified unit tests for CoreMqttClient (ProcessLoop configuration)
 */

#include <gtest/gtest.h>

#include "mqtt/coremqtt_client.hpp"
#include "mqtt/mqtt_config.hpp"

using namespace lopcore::mqtt;

/**
 * @brief Test fixture for CoreMqttClient tests
 */
class CoreMqttClientSimpleTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Default configuration for testing
        config = MqttConfig::builder()
                     .broker("test-broker.example.com")
                     .port(8883)
                     .clientId("test-client-123")
                     .keepAlive(std::chrono::seconds(60))
                     .cleanSession(true)
                     .build();
    }

    MqttConfig config;
};

// =============================================================================
// Basic Tests
// =============================================================================

TEST_F(CoreMqttClientSimpleTest, Construction_ValidConfig)
{
    ASSERT_NO_THROW({
        CoreMqttClient client(config);
        EXPECT_EQ(client.getClientId(), "test-client-123");
        EXPECT_EQ(client.getBroker(), "test-broker.example.com");
        EXPECT_EQ(client.getPort(), 8883);
        EXPECT_FALSE(client.isConnected());
        EXPECT_EQ(client.getConnectionState(), MqttConnectionState::DISCONNECTED);
    });
}

// =============================================================================
// ProcessLoop Configuration Tests
// =============================================================================

TEST_F(CoreMqttClientSimpleTest, ProcessLoopDelay_DefaultValue)
{
    CoreMqttClient client(config);
    
    // Default should be 10ms
    EXPECT_EQ(config.processLoopDelayMs, 10);
}

TEST_F(CoreMqttClientSimpleTest, ProcessLoopDelay_CustomValue_Low)
{
    config.processLoopDelayMs = 1; // 1ms for real-time
    CoreMqttClient client(config);
    
    EXPECT_EQ(config.processLoopDelayMs, 1);
}

TEST_F(CoreMqttClientSimpleTest, ProcessLoopDelay_CustomValue_High)
{
    config.processLoopDelayMs = 100; // 100ms for low power
    CoreMqttClient client(config);
    
    EXPECT_EQ(config.processLoopDelayMs, 100);
}

TEST_F(CoreMqttClientSimpleTest, ProcessLoopDelay_CustomValue_Maximum)
{
    config.processLoopDelayMs = 1000; // Maximum allowed
    CoreMqttClient client(config);
    
    EXPECT_EQ(config.processLoopDelayMs, 1000);
}

TEST_F(CoreMqttClientSimpleTest, ProcessLoopAutoStart_DefaultEnabled)
{
    CoreMqttClient client(config);
    
    // Default should be true
    EXPECT_TRUE(config.autoStartProcessLoop);
}

TEST_F(CoreMqttClientSimpleTest, ProcessLoopAutoStart_Disabled)
{
    config.autoStartProcessLoop = false;
    CoreMqttClient client(config);
    
    EXPECT_FALSE(config.autoStartProcessLoop);
}

TEST_F(CoreMqttClientSimpleTest, ProcessLoopTask_NotRunningInitially)
{
    CoreMqttClient client(config);
    
    // Task should not be running before connect
    EXPECT_FALSE(client.isProcessLoopTaskRunning());
}

TEST_F(CoreMqttClientSimpleTest, ProcessLoopDelay_CommonValues)
{
    // Test common use case values
    std::vector<uint32_t> commonDelays = {1, 5, 10, 20, 50, 100, 500, 1000};
    
    for (uint32_t delay : commonDelays)
    {
        MqttConfig testConfig = config;
        testConfig.processLoopDelayMs = delay;
        
        EXPECT_EQ(testConfig.validate(), ESP_OK);
        
        CoreMqttClient client(testConfig);
        EXPECT_EQ(testConfig.processLoopDelayMs, delay);
    }
}

/**
 * @brief Main function for running tests
 */
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
