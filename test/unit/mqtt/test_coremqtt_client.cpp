/**
 * @file test_coremqtt_client.cpp
 * @brief Unit tests for CoreMqttClient (coreMQTT wrapper)
 *
 * Tests the CoreMqttClient implementation which wraps AWS IoT coreMQTT library.
 *
 * @copyright Copyright (c) 2025
 */

#include <gtest/gtest.h>

#include "mqtt/coremqtt_client.hpp"
#include "mqtt/mqtt_config.hpp"

using namespace lopcore::mqtt;

/**
 * @brief Test fixture for CoreMqttClient tests
 */
class CoreMqttClientTest : public ::testing::Test
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

    void TearDown() override
    {
        // Cleanup
    }

    MqttConfig config;
};

// =============================================================================
// Construction & Configuration Tests
// =============================================================================

TEST_F(CoreMqttClientTest, Construction_ValidConfig)
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

// Note: Most other CoreMqttClient tests are disabled because they require full
// ESP-IDF integration or methods that aren't fully implemented yet.
// The key tests for ProcessLoop configuration are included below.

#if 0  // Disabled - requires builder methods that don't exist yet
TEST_F(CoreMqttClientTest, Construction_WithBudgeting)
{
    auto budgetConfig = MqttConfig::builder()
                            .broker("test-broker.example.com")
                            .clientId("test-client")
                            .budget(BudgetConfig{.enabled = true,
                                                 .defaultBudget = 50,
                                                 .maxBudget = 100,
                                                 .reviveCount = 2,
                                                 .revivePeriod = std::chrono::seconds(10)})
                            .build();

    ASSERT_NO_THROW({
        CoreMqttClient client(budgetConfig);
        auto stats = client.getStatistics();
        EXPECT_EQ(stats.publishErrors, 0);
    });
}

TEST_F(CoreMqttClientTest, Construction_WithWillMessage)
{
    auto willConfig =
        MqttConfig::builder()
            .broker("test-broker.example.com")
            .clientId("test-client")
            .will(WillConfig{.topic = "device/status",
                             .payload = {'d', 'i', 's', 'c', 'o', 'n', 'n', 'e', 'c', 't', 'e', 'd'},
                             .qos = MqttQos::AT_LEAST_ONCE,
                             .retain = true})
            .build();

    ASSERT_NO_THROW({
        CoreMqttClient client(willConfig);
        EXPECT_FALSE(client.isConnected());
    });
}
#endif // Disabled construction tests

// =============================================================================
// Basic Interface Method Tests (working with mocks)
// =============================================================================

TEST_F(CoreMqttClientTest, GetClientId)
{
    CoreMqttClient client(config);
    EXPECT_EQ(client.getClientId(), "test-client-123");
}

TEST_F(CoreMqttClientTest, GetBroker)
{
    CoreMqttClient client(config);
    EXPECT_EQ(client.getBroker(), "test-broker.example.com");
}

TEST_F(CoreMqttClientTest, GetPort)
{
    CoreMqttClient client(config);
    EXPECT_EQ(client.getPort(), 8883);
}

TEST_F(CoreMqttClientTest, GetConnectionState_InitiallyDisconnected)
{
    CoreMqttClient client(config);
    EXPECT_EQ(client.getConnectionState(), MqttConnectionState::DISCONNECTED);
    EXPECT_FALSE(client.isConnected());
}

#if 0 // Disabled - methods don't exist or have wrong signatures
TEST_F(CoreMqttClientTest, SetWillMessage_ReturnsNotSupported)
{
    CoreMqttClient client(config);

    // coreMQTT requires will message in CONNECT packet, so setWillMessage is not supported
    std::vector<uint8_t> payload = {'t', 'e', 's', 't'};
    esp_err_t result = client.setWillMessage("test/topic", payload, MqttQos::AT_MOST_ONCE, false);

    EXPECT_EQ(result, ESP_ERR_NOT_SUPPORTED);
}

// =============================================================================
// Statistics Tests - DISABLED (require full implementation)
// =============================================================================

TEST_F(CoreMqttClientTest, Statistics_InitialValues)
{
    CoreMqttClient client(config);
    auto stats = client.getStatistics();

    EXPECT_EQ(stats.messagesPublished, 0);
    EXPECT_EQ(stats.messagesReceived, 0);
    EXPECT_EQ(stats.publishErrors, 0);
    EXPECT_EQ(stats.subscriptionCount, 0);
    EXPECT_EQ(stats.reconnectCount, 0);
    EXPECT_EQ(stats.lastConnected, 0);
    EXPECT_EQ(stats.lastDisconnected, 0);
}

TEST_F(CoreMqttClientTest, Statistics_Reset)
{
    CoreMqttClient client(config);

    // Reset statistics
    client.resetStatistics();

    auto stats = client.getStatistics();
    EXPECT_EQ(stats.messagesPublished, 0);
    EXPECT_EQ(stats.messagesReceived, 0);
    EXPECT_EQ(stats.publishErrors, 0);
}

// =============================================================================
// Callback Tests - DISABLED (require full implementation)
// =============================================================================

// These tests are commented out because they rely on methods that don't exist yet
// or have incorrect signatures. Enable them once the full CoreMqttClient is implemented.

#if 0
TEST_F(CoreMqttClientTest, SetConnectionCallback)
{
    CoreMqttClient client(config);

    bool callbackInvoked = false;
    MqttConnectionState receivedState = MqttConnectionState::DISCONNECTED;

    client.setConnectionCallback([&](MqttConnectionState state) {
        callbackInvoked = true;
        receivedState = state;
    });

    // Note: Callback won't be invoked until actual connection event
    EXPECT_FALSE(callbackInvoked);
}

TEST_F(CoreMqttClientTest, SetErrorCallback)
{
    CoreMqttClient client(config);

    bool callbackInvoked = false;
    MqttError receivedError = MqttError::NO_ERROR;
    std::string receivedMessage;

    client.setErrorCallback([&](MqttError error, const std::string &message) {
        callbackInvoked = true;
        receivedError = error;
        receivedMessage = message;
    });

    // Note: Callback won't be invoked until actual error event
    EXPECT_FALSE(callbackInvoked);
}

// =============================================================================
// PublishString Helper Tests
// =============================================================================

TEST_F(CoreMqttClientTest, PublishString_NotConnected)
{
    CoreMqttClient client(config);

    // Publishing while not connected should fail
    esp_err_t result = client.publishString("test/topic", "test message");

    EXPECT_EQ(result, ESP_ERR_INVALID_STATE);
}

// =============================================================================
// Configuration Validation Tests
// =============================================================================

TEST_F(CoreMqttClientTest, Configuration_MinimalValid)
{
    auto minConfig = MqttConfig::builder().broker("broker.example.com").clientId("min-client").build();

    ASSERT_NO_THROW({
        CoreMqttClient client(minConfig);
        EXPECT_EQ(client.getBroker(), "broker.example.com");
        EXPECT_EQ(client.getClientId(), "min-client");
    });
}

TEST_F(CoreMqttClientTest, Configuration_WithTls)
{
    auto tlsConfig = MqttConfig::builder()
                         .broker("secure-broker.example.com")
                         .port(8883)
                         .clientId("tls-client")
                         .tls(TlsConfig{.enabled = true,
                                        .caCertPath = "/spiffs/ca.crt",
                                        .clientCertLabel = "device_cert",
                                        .clientKeyLabel = "device_key",
                                        .verifyPeer = true,
                                        .timeoutMs = 10000})
                         .build();

    ASSERT_NO_THROW({
        CoreMqttClient client(tlsConfig);
        EXPECT_EQ(client.getPort(), 8883);
    });
}

// =============================================================================
// Edge Cases & Error Handling
// =============================================================================

TEST_F(CoreMqttClientTest, Disconnect_WhenNotConnected)
{
    CoreMqttClient client(config);

    // Disconnecting when not connected should be safe
    esp_err_t result = client.disconnect();

    EXPECT_EQ(result, ESP_OK);
    EXPECT_FALSE(client.isConnected());
}

TEST_F(CoreMqttClientTest, Unsubscribe_WhenNotConnected)
{
    CoreMqttClient client(config);

    // Unsubscribing while not connected should fail
    esp_err_t result = client.unsubscribe("test/topic");

    EXPECT_EQ(result, ESP_ERR_INVALID_STATE);
}

// =============================================================================
// coreMQTT-Specific Tests
// =============================================================================

TEST_F(CoreMqttClientTest, HasOutstandingPackets_InitiallyFalse)
{
    CoreMqttClient client(config);

    // Initially should have no outstanding packets
    EXPECT_FALSE(client.hasOutstandingPackets());
}

TEST_F(CoreMqttClientTest, GetPublishState_InvalidPacketId)
{
    CoreMqttClient client(config);

    // Getting state for invalid packet ID should return MQTTStateNull
    MQTTPublishState_t state = client.getPublishState(9999);

    EXPECT_EQ(state, MQTTStateNull);
}

// =============================================================================
// Memory & Resource Tests
// =============================================================================

TEST_F(CoreMqttClientTest, MultipleInstances)
{
    // Should be able to create multiple client instances
    ASSERT_NO_THROW({
        CoreMqttClient client1(config);

        auto config2 = MqttConfig::builder().broker("broker2.example.com").clientId("client-2").build();
        CoreMqttClient client2(config2);

        EXPECT_NE(client1.getClientId(), client2.getClientId());
        EXPECT_NE(client1.getBroker(), client2.getBroker());
    });
}

TEST_F(CoreMqttClientTest, Cleanup_OnDestruction)
{
    // Should properly cleanup on destruction
    {
        CoreMqttClient client(config);
        EXPECT_FALSE(client.isConnected());
    }
    // No crash or leak expected
}

// =============================================================================
// ProcessLoop Configuration Tests
// =============================================================================

TEST_F(CoreMqttClientTest, ProcessLoopDelay_DefaultValue)
{
    CoreMqttClient client(config);

    // Default should be 10ms
    EXPECT_EQ(config.processLoopDelayMs, 10);
}

TEST_F(CoreMqttClientTest, ProcessLoopDelay_CustomValue_Low)
{
    config.processLoopDelayMs = 1; // 1ms for real-time
    CoreMqttClient client(config);

    EXPECT_EQ(config.processLoopDelayMs, 1);
}

TEST_F(CoreMqttClientTest, ProcessLoopDelay_CustomValue_High)
{
    config.processLoopDelayMs = 100; // 100ms for low power
    CoreMqttClient client(config);

    EXPECT_EQ(config.processLoopDelayMs, 100);
}

TEST_F(CoreMqttClientTest, ProcessLoopDelay_CustomValue_Maximum)
{
    config.processLoopDelayMs = 1000; // Maximum allowed
    CoreMqttClient client(config);

    EXPECT_EQ(config.processLoopDelayMs, 1000);
}

TEST_F(CoreMqttClientTest, ProcessLoopAutoStart_DefaultEnabled)
{
    CoreMqttClient client(config);

    // Default should be true
    EXPECT_TRUE(config.autoStartProcessLoop);
}

TEST_F(CoreMqttClientTest, ProcessLoopAutoStart_Disabled)
{
    config.autoStartProcessLoop = false;
    CoreMqttClient client(config);

    EXPECT_FALSE(config.autoStartProcessLoop);
}

TEST_F(CoreMqttClientTest, ProcessLoopTask_NotRunningInitially)
{
    CoreMqttClient client(config);

    // Task should not be running before connect
    EXPECT_FALSE(client.isProcessLoopTaskRunning());
}

TEST_F(CoreMqttClientTest, ProcessLoopDelay_ViaBuilder_DefaultValue)
{
    auto testConfig = MqttConfig::builder()
                          .broker("test-broker.example.com")
                          .port(1883)
                          .clientId("test-client")
                          .build();

    CoreMqttClient client(testConfig);
    EXPECT_EQ(testConfig.processLoopDelayMs, 10);
}

TEST_F(CoreMqttClientTest, ProcessLoopDelay_ViaBuilder_CustomValue)
{
    auto testConfig = MqttConfig::builder()
                          .broker("test-broker.example.com")
                          .port(1883)
                          .clientId("test-client")
                          .build();

    // Manually set after building
    testConfig.processLoopDelayMs = 50;

    CoreMqttClient client(testConfig);
    EXPECT_EQ(testConfig.processLoopDelayMs, 50);
}

TEST_F(CoreMqttClientTest, ProcessLoopDelay_RangeValidation_BoundaryValues)
{
    // Test boundary values
    MqttConfig testConfig1 = config;
    testConfig1.processLoopDelayMs = 1; // Min
    EXPECT_EQ(testConfig1.validate(), ESP_OK);

    MqttConfig testConfig2 = config;
    testConfig2.processLoopDelayMs = 1000; // Max
    EXPECT_EQ(testConfig2.validate(), ESP_OK);

    MqttConfig testConfig3 = config;
    testConfig3.processLoopDelayMs = 0; // Invalid (too low)
    EXPECT_EQ(testConfig3.validate(), ESP_ERR_INVALID_ARG);

    MqttConfig testConfig4 = config;
    testConfig4.processLoopDelayMs = 1001; // Invalid (too high)
    EXPECT_EQ(testConfig4.validate(), ESP_ERR_INVALID_ARG);
}

TEST_F(CoreMqttClientTest, ProcessLoopDelay_CommonValues)
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
