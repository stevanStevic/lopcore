/**
 * @file test_mqtt_types.cpp
 * @brief Unit tests for MQTT types and enums
 */

#include <gtest/gtest.h>
#include <mqtt/mqtt_types.hpp>

using namespace lopcore::mqtt;

// =============================================================================
// QoS Conversion Tests
// =============================================================================

TEST(MqttTypesTest, QosToIntConversion)
{
    EXPECT_EQ(qosToInt(MqttQos::AT_MOST_ONCE), 0);
    EXPECT_EQ(qosToInt(MqttQos::AT_LEAST_ONCE), 1);
    EXPECT_EQ(qosToInt(MqttQos::EXACTLY_ONCE), 2);
}

TEST(MqttTypesTest, IntToQosConversion)
{
    EXPECT_EQ(intToQos(0), MqttQos::AT_MOST_ONCE);
    EXPECT_EQ(intToQos(1), MqttQos::AT_LEAST_ONCE);
    EXPECT_EQ(intToQos(2), MqttQos::EXACTLY_ONCE);

    // Invalid values default to AT_MOST_ONCE
    EXPECT_EQ(intToQos(3), MqttQos::AT_MOST_ONCE);
    EXPECT_EQ(intToQos(255), MqttQos::AT_MOST_ONCE);
}

// =============================================================================
// Error String Conversion Tests
// =============================================================================

TEST(MqttTypesTest, ErrorToString)
{
    EXPECT_STREQ(errorToString(MqttError::NONE), "No error");
    EXPECT_STREQ(errorToString(MqttError::CONNECTION_REFUSED), "Connection refused");
    EXPECT_STREQ(errorToString(MqttError::CONNECTION_LOST), "Connection lost");
    EXPECT_STREQ(errorToString(MqttError::TIMEOUT), "Timeout");
    EXPECT_STREQ(errorToString(MqttError::AUTH_FAILED), "Authentication failed");
    EXPECT_STREQ(errorToString(MqttError::TLS_HANDSHAKE_FAILED), "TLS handshake failed");
    EXPECT_STREQ(errorToString(MqttError::BUDGET_EXHAUSTED), "Message budget exhausted");
}

// =============================================================================
// Connection State String Conversion Tests
// =============================================================================

TEST(MqttTypesTest, StateToString)
{
    EXPECT_STREQ(stateToString(MqttConnectionState::DISCONNECTED), "Disconnected");
    EXPECT_STREQ(stateToString(MqttConnectionState::CONNECTING), "Connecting");
    EXPECT_STREQ(stateToString(MqttConnectionState::CONNECTED), "Connected");
    EXPECT_STREQ(stateToString(MqttConnectionState::RECONNECTING), "Reconnecting");
    EXPECT_STREQ(stateToString(MqttConnectionState::DISCONNECTING), "Disconnecting");
    EXPECT_STREQ(stateToString(MqttConnectionState::ERROR), "Error");
}

// =============================================================================
// Client Type String Conversion Tests
// =============================================================================

TEST(MqttTypesTest, ClientTypeToString)
{
    EXPECT_STREQ(clientTypeToString(MqttClientType::ESP_MQTT), "ESP-MQTT");
    EXPECT_STREQ(clientTypeToString(MqttClientType::AWS_IOT), "AWS-IOT");
    EXPECT_STREQ(clientTypeToString(MqttClientType::MOCK), "MOCK");
}

// =============================================================================
// MqttMessage Tests
// =============================================================================

TEST(MqttTypesTest, MqttMessageGetPayloadAsString)
{
    MqttMessage msg;
    msg.topic = "test/topic";
    msg.payload = {'H', 'e', 'l', 'l', 'o'};
    msg.qos = MqttQos::AT_LEAST_ONCE;
    msg.retained = false;
    msg.messageId = 12345;

    EXPECT_EQ(msg.getPayloadAsString(), "Hello");
}

TEST(MqttTypesTest, MqttMessageEmptyPayload)
{
    MqttMessage msg;
    msg.payload = {};

    EXPECT_TRUE(msg.getPayloadAsString().empty());
}

TEST(MqttTypesTest, MqttMessageBinaryPayload)
{
    MqttMessage msg;
    msg.payload = {0x01, 0x02, 0x03, 0xFF};

    std::string str = msg.getPayloadAsString();
    EXPECT_EQ(str.size(), 4);
    EXPECT_EQ(static_cast<uint8_t>(str[0]), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(str[1]), 0x02);
    EXPECT_EQ(static_cast<uint8_t>(str[2]), 0x03);
    EXPECT_EQ(static_cast<uint8_t>(str[3]), 0xFF);
}

// =============================================================================
// MqttStatistics Tests
// =============================================================================

TEST(MqttTypesTest, StatisticsDefaultValues)
{
    MqttStatistics stats;

    EXPECT_EQ(stats.messagesPublished, 0);
    EXPECT_EQ(stats.messagesReceived, 0);
    EXPECT_EQ(stats.publishErrors, 0);
    EXPECT_EQ(stats.reconnectCount, 0);
    EXPECT_EQ(stats.subscriptionCount, 0);
    EXPECT_EQ(stats.averagePublishLatency.count(), 0);
}

TEST(MqttTypesTest, StatisticsReset)
{
    MqttStatistics stats;
    stats.messagesPublished = 100;
    stats.messagesReceived = 50;
    stats.publishErrors = 5;
    stats.reconnectCount = 3;
    stats.subscriptionCount = 10;
    stats.averagePublishLatency = std::chrono::milliseconds(25);

    stats.reset();

    EXPECT_EQ(stats.messagesPublished, 0);
    EXPECT_EQ(stats.messagesReceived, 0);
    EXPECT_EQ(stats.publishErrors, 0);
    EXPECT_EQ(stats.reconnectCount, 0);
    EXPECT_EQ(stats.subscriptionCount, 0);
    EXPECT_EQ(stats.averagePublishLatency.count(), 0);
}

TEST(MqttTypesTest, StatisticsAccumulation)
{
    MqttStatistics stats;

    stats.messagesPublished++;
    stats.messagesPublished++;
    stats.messagesReceived++;

    EXPECT_EQ(stats.messagesPublished, 2);
    EXPECT_EQ(stats.messagesReceived, 1);
}
