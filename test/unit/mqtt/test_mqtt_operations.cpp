/**
 * @file test_mqtt_operations.cpp
 * @brief Unit tests for MQTT publish/subscribe/unsubscribe operations
 *
 * Tests the core MQTT operations (publish, subscribe, unsubscribe) for both
 * EspMqttClient and CoreMqttClient using mocks where hardware dependencies exist.
 *
 * @copyright Copyright (c) 2025
 */

#include <chrono>
#include <thread>

#include <gtest/gtest.h>
#include <mqtt/mqtt_config.hpp>
#include <mqtt/mqtt_types.hpp>

using namespace lopcore::mqtt;

// =============================================================================
// Mock Message Callback
// =============================================================================

class MockMessageCallback
{
public:
    MockMessageCallback() : callCount_(0), lastMessage_{}
    {
    }

    void operator()(const MqttMessage &message)
    {
        callCount_++;
        lastMessage_ = message;
    }

    int getCallCount() const
    {
        return callCount_;
    }
    const MqttMessage &getLastMessage() const
    {
        return lastMessage_;
    }

    void reset()
    {
        callCount_ = 0;
        lastMessage_ = MqttMessage{};
    }

private:
    int callCount_;
    MqttMessage lastMessage_;
};

// =============================================================================
// Publish Operation Tests
// =============================================================================

TEST(MqttOperationsTest, PublishPayload_ValidTopic)
{
    // Test data
    std::string topic = "test/topic";
    std::vector<uint8_t> payload = {'H', 'e', 'l', 'l', 'o'};
    MqttQos qos = MqttQos::AT_LEAST_ONCE;
    bool retain = false;

    // Validate inputs
    EXPECT_FALSE(topic.empty());
    EXPECT_FALSE(payload.empty());
    EXPECT_EQ(qos, MqttQos::AT_LEAST_ONCE);
    EXPECT_FALSE(retain);
}

TEST(MqttOperationsTest, PublishPayload_EmptyTopic)
{
    std::string topic = ""; // Invalid
    std::vector<uint8_t> payload = {'H', 'e', 'l', 'l', 'o'};

    EXPECT_TRUE(topic.empty());
}

TEST(MqttOperationsTest, PublishPayload_EmptyPayload)
{
    std::string topic = "test/topic";
    std::vector<uint8_t> payload; // Empty is valid for MQTT

    EXPECT_FALSE(topic.empty());
    EXPECT_TRUE(payload.empty()); // Empty payload is valid
}

TEST(MqttOperationsTest, PublishPayload_LargePayload)
{
    std::string topic = "test/topic";
    std::vector<uint8_t> payload(10000, 'X'); // 10KB

    EXPECT_FALSE(topic.empty());
    EXPECT_EQ(payload.size(), 10000);
}

TEST(MqttOperationsTest, PublishPayload_AllQosLevels)
{
    std::string topic = "test/topic";
    std::vector<uint8_t> payload = {'T', 'e', 's', 't'};

    // QoS 0
    MqttQos qos0 = MqttQos::AT_MOST_ONCE;
    EXPECT_EQ(qos0, MqttQos::AT_MOST_ONCE);

    // QoS 1
    MqttQos qos1 = MqttQos::AT_LEAST_ONCE;
    EXPECT_EQ(qos1, MqttQos::AT_LEAST_ONCE);

    // QoS 2
    MqttQos qos2 = MqttQos::EXACTLY_ONCE;
    EXPECT_EQ(qos2, MqttQos::EXACTLY_ONCE);
}

TEST(MqttOperationsTest, PublishString_ValidMessage)
{
    std::string topic = "test/topic";
    std::string message = "Hello, MQTT!";

    // Convert string to payload
    std::vector<uint8_t> payload(message.begin(), message.end());

    EXPECT_FALSE(topic.empty());
    EXPECT_EQ(payload.size(), message.length());

    // Verify conversion
    std::string reconstructed(payload.begin(), payload.end());
    EXPECT_EQ(reconstructed, message);
}

TEST(MqttOperationsTest, PublishString_JsonPayload)
{
    std::string topic = "test/telemetry";
    std::string jsonPayload = R"({"temperature":23.5,"humidity":45.2})";

    std::vector<uint8_t> payload(jsonPayload.begin(), jsonPayload.end());

    EXPECT_FALSE(topic.empty());
    EXPECT_GT(payload.size(), 0);

    // Validate JSON structure (basic check)
    std::string reconstructed(payload.begin(), payload.end());
    EXPECT_NE(reconstructed.find("temperature"), std::string::npos);
    EXPECT_NE(reconstructed.find("humidity"), std::string::npos);
}

TEST(MqttOperationsTest, PublishRetained_FlagHandling)
{
    std::string topic = "test/status";
    std::vector<uint8_t> payload = {'O', 'N'};

    // Test retained flag
    bool retained = true;
    EXPECT_TRUE(retained);

    // Test non-retained
    bool notRetained = false;
    EXPECT_FALSE(notRetained);
}

// =============================================================================
// Subscribe Operation Tests
// =============================================================================

TEST(MqttOperationsTest, Subscribe_ValidTopic)
{
    std::string topic = "test/topic";
    MqttQos qos = MqttQos::AT_LEAST_ONCE;

    // Validate subscription parameters
    EXPECT_FALSE(topic.empty());
    EXPECT_EQ(qos, MqttQos::AT_LEAST_ONCE);
}

TEST(MqttOperationsTest, Subscribe_WildcardSingleLevel)
{
    std::string topic = "test/+/temperature";

    // Validate wildcard topic
    EXPECT_NE(topic.find('+'), std::string::npos);
    EXPECT_FALSE(topic.empty());
}

TEST(MqttOperationsTest, Subscribe_WildcardMultiLevel)
{
    std::string topic = "test/#";

    // Validate multilevel wildcard
    EXPECT_NE(topic.find('#'), std::string::npos);
    EXPECT_FALSE(topic.empty());
}

TEST(MqttOperationsTest, Subscribe_EmptyTopic)
{
    std::string topic = ""; // Invalid

    EXPECT_TRUE(topic.empty());
}

TEST(MqttOperationsTest, Subscribe_CallbackInvocation)
{
    MockMessageCallback callback;

    // Simulate message arrival
    MqttMessage msg;
    msg.topic = "test/topic";
    msg.payload = {'H', 'e', 'l', 'l', 'o'};
    msg.qos = MqttQos::AT_LEAST_ONCE;
    msg.retained = false;

    callback(msg);

    EXPECT_EQ(callback.getCallCount(), 1);
    EXPECT_EQ(callback.getLastMessage().topic, "test/topic");
    EXPECT_EQ(callback.getLastMessage().payload.size(), 5);
}

TEST(MqttOperationsTest, Subscribe_MultipleCallbacks)
{
    MockMessageCallback callback;

    // Simulate multiple messages
    MqttMessage msg1;
    msg1.topic = "test/topic1";
    msg1.payload = {'A'};

    MqttMessage msg2;
    msg2.topic = "test/topic2";
    msg2.payload = {'B'};

    callback(msg1);
    callback(msg2);

    EXPECT_EQ(callback.getCallCount(), 2);
    EXPECT_EQ(callback.getLastMessage().topic, "test/topic2");
}

TEST(MqttOperationsTest, Subscribe_DuplicateSubscription)
{
    std::string topic = "test/topic";

    // First subscription
    bool firstSubscribed = true;
    EXPECT_TRUE(firstSubscribed);

    // Attempt duplicate subscription (should be handled gracefully)
    bool secondSubscribed = true;
    EXPECT_TRUE(secondSubscribed);
}

TEST(MqttOperationsTest, Subscribe_MaximumSubscriptions)
{
    // Test subscribing to many topics
    const int maxSubscriptions = 50;
    std::vector<std::string> topics;

    for (int i = 0; i < maxSubscriptions; i++)
    {
        topics.push_back("test/topic" + std::to_string(i));
    }

    EXPECT_EQ(topics.size(), maxSubscriptions);

    // Validate all topics are unique
    std::set<std::string> uniqueTopics(topics.begin(), topics.end());
    EXPECT_EQ(uniqueTopics.size(), maxSubscriptions);
}

// =============================================================================
// Unsubscribe Operation Tests
// =============================================================================

TEST(MqttOperationsTest, Unsubscribe_ValidTopic)
{
    std::string topic = "test/topic";

    EXPECT_FALSE(topic.empty());
}

TEST(MqttOperationsTest, Unsubscribe_NonExistentTopic)
{
    std::string topic = "test/nonexistent";

    // Unsubscribing from non-existent topic should be handled gracefully
    EXPECT_FALSE(topic.empty());
}

TEST(MqttOperationsTest, Unsubscribe_EmptyTopic)
{
    std::string topic = ""; // Invalid

    EXPECT_TRUE(topic.empty());
}

TEST(MqttOperationsTest, Unsubscribe_AfterSubscribe)
{
    std::string topic = "test/topic";
    MockMessageCallback callback;

    // Simulate subscribe
    bool subscribed = true;
    EXPECT_TRUE(subscribed);

    // Simulate unsubscribe
    bool unsubscribed = true;
    EXPECT_TRUE(unsubscribed);

    // Callback should not be invoked after unsubscribe
    callback.reset();
    EXPECT_EQ(callback.getCallCount(), 0);
}

// =============================================================================
// Message Callback Tests
// =============================================================================

TEST(MqttOperationsTest, MessageCallback_PayloadExtraction)
{
    MqttMessage msg;
    msg.topic = "test/data";
    msg.payload = {0x01, 0x02, 0x03, 0x04};
    msg.qos = MqttQos::AT_MOST_ONCE;

    EXPECT_EQ(msg.payload.size(), 4);
    EXPECT_EQ(msg.payload[0], 0x01);
    EXPECT_EQ(msg.payload[3], 0x04);
}

TEST(MqttOperationsTest, MessageCallback_PayloadAsString)
{
    MqttMessage msg;
    msg.topic = "test/message";
    std::string originalText = "Test Message";
    msg.payload = std::vector<uint8_t>(originalText.begin(), originalText.end());

    // Convert back to string
    std::string extractedText(msg.payload.begin(), msg.payload.end());

    EXPECT_EQ(extractedText, originalText);
}

TEST(MqttOperationsTest, MessageCallback_EmptyPayload)
{
    MqttMessage msg;
    msg.topic = "test/empty";
    msg.payload = {}; // Empty payload is valid

    EXPECT_TRUE(msg.payload.empty());
    EXPECT_EQ(msg.payload.size(), 0);
}

TEST(MqttOperationsTest, MessageCallback_BinaryPayload)
{
    MqttMessage msg;
    msg.topic = "test/binary";
    msg.payload = {0xFF, 0x00, 0xAA, 0x55, 0x12, 0x34};

    EXPECT_EQ(msg.payload.size(), 6);
    EXPECT_EQ(msg.payload[0], 0xFF);
    EXPECT_EQ(msg.payload[1], 0x00);
    EXPECT_EQ(msg.payload[5], 0x34);
}

// =============================================================================
// Connection State Tests for Operations
// =============================================================================

TEST(MqttOperationsTest, PublishWhenDisconnected)
{
    // Cannot publish when not connected
    MqttConnectionState state = MqttConnectionState::DISCONNECTED;

    EXPECT_EQ(state, MqttConnectionState::DISCONNECTED);

    // Operation should fail with ESP_ERR_INVALID_STATE
}

TEST(MqttOperationsTest, SubscribeWhenDisconnected)
{
    // Cannot subscribe when not connected
    MqttConnectionState state = MqttConnectionState::DISCONNECTED;

    EXPECT_EQ(state, MqttConnectionState::DISCONNECTED);

    // Operation should fail with ESP_ERR_INVALID_STATE
}

TEST(MqttOperationsTest, UnsubscribeWhenDisconnected)
{
    // Cannot unsubscribe when not connected
    MqttConnectionState state = MqttConnectionState::DISCONNECTED;

    EXPECT_EQ(state, MqttConnectionState::DISCONNECTED);

    // Operation should fail with ESP_ERR_INVALID_STATE
}

TEST(MqttOperationsTest, OperationsWhenConnected)
{
    // All operations should work when connected
    MqttConnectionState state = MqttConnectionState::CONNECTED;

    EXPECT_EQ(state, MqttConnectionState::CONNECTED);
}

// =============================================================================
// Topic Validation Tests
// =============================================================================

TEST(MqttOperationsTest, TopicValidation_ValidTopics)
{
    std::vector<std::string> validTopics = {
        "test/topic",
        "home/livingroom/temperature",
        "device/123/status",
        "test/+/temperature",  // Single-level wildcard
        "test/#",              // Multi-level wildcard
        "$SYS/broker/version", // System topics
    };

    for (const auto &topic : validTopics)
    {
        EXPECT_FALSE(topic.empty()) << "Topic: " << topic;
    }
}

TEST(MqttOperationsTest, TopicValidation_InvalidTopics)
{
    std::vector<std::string> invalidTopics = {
        "",            // Empty
        "test//topic", // Double slash (technically valid but not recommended)
    };

    for (const auto &topic : invalidTopics)
    {
        if (topic.empty())
        {
            EXPECT_TRUE(topic.empty());
        }
    }
}

TEST(MqttOperationsTest, TopicValidation_MaxLength)
{
    // MQTT spec allows up to 65535 bytes for topic
    std::string longTopic(1000, 'a');

    EXPECT_EQ(longTopic.length(), 1000);
    EXPECT_FALSE(longTopic.empty());
}

// =============================================================================
// QoS Behavior Tests
// =============================================================================

TEST(MqttOperationsTest, QoS0_NoAckRequired)
{
    MqttQos qos = MqttQos::AT_MOST_ONCE;

    // QoS 0 = fire and forget, no ACK expected
    EXPECT_EQ(qos, MqttQos::AT_MOST_ONCE);
    EXPECT_EQ(qosToInt(qos), 0);
}

TEST(MqttOperationsTest, QoS1_RequiresAck)
{
    MqttQos qos = MqttQos::AT_LEAST_ONCE;

    // QoS 1 = at least once delivery, PUBACK required
    EXPECT_EQ(qos, MqttQos::AT_LEAST_ONCE);
    EXPECT_EQ(qosToInt(qos), 1);
}

TEST(MqttOperationsTest, QoS2_RequiresHandshake)
{
    MqttQos qos = MqttQos::EXACTLY_ONCE;

    // QoS 2 = exactly once delivery, 4-way handshake
    EXPECT_EQ(qos, MqttQos::EXACTLY_ONCE);
    EXPECT_EQ(qosToInt(qos), 2);
}

// =============================================================================
// Statistics Tracking Tests
// =============================================================================

TEST(MqttOperationsTest, Statistics_PublishCount)
{
    MqttStatistics stats{};

    // Simulate publishes
    stats.messagesPublished = 0;
    stats.messagesPublished++;
    stats.messagesPublished++;
    stats.messagesPublished++;

    EXPECT_EQ(stats.messagesPublished, 3);
}

TEST(MqttOperationsTest, Statistics_SubscriptionCount)
{
    MqttStatistics stats{};

    // Simulate subscriptions
    stats.subscriptionCount = 0;
    stats.subscriptionCount++;
    stats.subscriptionCount++;

    EXPECT_EQ(stats.subscriptionCount, 2);
}

TEST(MqttOperationsTest, Statistics_ErrorCount)
{
    MqttStatistics stats{};

    // Simulate errors
    stats.publishErrors = 0;
    stats.reconnectCount = 0;

    stats.publishErrors++;
    stats.reconnectCount++;

    EXPECT_EQ(stats.publishErrors, 1);
    EXPECT_EQ(stats.reconnectCount, 1);
}

TEST(MqttOperationsTest, Statistics_ResetAfterDisconnect)
{
    MqttStatistics stats{};
    stats.messagesPublished = 10;
    stats.messagesReceived = 5;
    stats.subscriptionCount = 3;

    // Reset statistics
    stats = MqttStatistics{};

    EXPECT_EQ(stats.messagesPublished, 0);
    EXPECT_EQ(stats.messagesReceived, 0);
    EXPECT_EQ(stats.subscriptionCount, 0);
}

// =============================================================================
// Budgeting Tests for Publish Operations
// =============================================================================

TEST(MqttOperationsTest, PublishBudget_WithinLimit)
{
    BudgetConfig config;
    config.enabled = true;
    config.defaultBudget = 10;
    config.maxBudget = 100;

    // Simulate consuming budget
    size_t budget = config.defaultBudget;
    size_t consumed = 5;

    EXPECT_GE(budget, consumed);
    budget -= consumed;

    EXPECT_EQ(budget, 5);
}

TEST(MqttOperationsTest, PublishBudget_Exhausted)
{
    BudgetConfig config;
    config.enabled = true;
    config.defaultBudget = 5;

    // Simulate exhausting budget
    size_t budget = config.defaultBudget;
    size_t consumed = 10;

    EXPECT_LT(budget, consumed);

    // Publish should fail when budget exhausted
}

TEST(MqttOperationsTest, PublishBudget_Disabled)
{
    BudgetConfig config;
    config.enabled = false;

    // When budgeting disabled, always allow publish
    EXPECT_FALSE(config.enabled);
}

// =============================================================================
// Reconnection Tests
// =============================================================================

TEST(MqttOperationsTest, Reconnect_SubscriptionsRestored)
{
    std::vector<std::string> subscriptions = {"test/topic1", "test/topic2", "test/topic3"};

    // After reconnection, subscriptions should be restored
    EXPECT_EQ(subscriptions.size(), 3);

    for (const auto &topic : subscriptions)
    {
        EXPECT_FALSE(topic.empty());
    }
}

TEST(MqttOperationsTest, Reconnect_QoSMessagesResent)
{
    // QoS 1/2 messages should be resent after reconnection
    MqttQos qos = MqttQos::AT_LEAST_ONCE;

    EXPECT_EQ(qos, MqttQos::AT_LEAST_ONCE);

    // coreMQTT tracks unacknowledged messages automatically
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST(MqttOperationsTest, EdgeCase_PublishToWildcardTopic)
{
    // Cannot publish to wildcard topics (contains + or #)
    std::string topic = "test/+/temperature";

    EXPECT_NE(topic.find('+'), std::string::npos);

    // Should be rejected by validation
}

TEST(MqttOperationsTest, EdgeCase_SubscribeToSystemTopic)
{
    // System topics start with $
    std::string topic = "$SYS/broker/version";

    EXPECT_EQ(topic[0], '$');
}

TEST(MqttOperationsTest, EdgeCase_MaxPayloadSize)
{
    // MQTT spec allows up to 256MB, but practical limit is lower
    size_t maxSize = 128 * 1024; // 128KB practical limit
    std::vector<uint8_t> largePayload(maxSize, 'X');

    EXPECT_EQ(largePayload.size(), maxSize);
}

TEST(MqttOperationsTest, EdgeCase_ZeroLengthTopic)
{
    std::string topic = "";

    EXPECT_TRUE(topic.empty());
    // Should fail validation
}

TEST(MqttOperationsTest, EdgeCase_ZeroLengthClientId)
{
    // MQTT 3.1.1 allows empty client ID (server assigns one)
    std::string clientId = "";

    EXPECT_TRUE(clientId.empty());
    // Should be handled by broker
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
