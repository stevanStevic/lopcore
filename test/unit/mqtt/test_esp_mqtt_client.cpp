/**
 * @file test_esp_mqtt_client.cpp
 * @brief Unit tests for ESP-MQTT client wrapper
 *
 * Note: These tests focus on interface compliance and logic validation.
 * Full ESP-MQTT integration tests run on hardware.
 */

#include <gtest/gtest.h>
#include <mqtt/mqtt_config.hpp>
#include <mqtt/mqtt_types.hpp>

using namespace lopcore::mqtt;

// =============================================================================
// Configuration Tests
// =============================================================================

TEST(EspMqttClientTest, ConfigurationValidation)
{
    MqttConfig config;
    config.broker = "mqtt.example.com";
    config.port = 1883;
    config.clientId = "test-client";

    EXPECT_EQ(config.validate(), ESP_OK);
}

TEST(EspMqttClientTest, ConfigurationValidation_InvalidBroker)
{
    MqttConfig config;
    config.broker = ""; // Invalid: empty
    config.port = 1883;
    config.clientId = "test-client";

    EXPECT_NE(config.validate(), ESP_OK);
}

TEST(EspMqttClientTest, ConfigurationWithTls)
{
    TlsConfig tls;
    tls.caCertPath = "/certs/ca.crt";
    tls.clientCertLabel = "device_cert";
    tls.clientKeyLabel = "device_key";
    tls.verifyPeer = true;

    EXPECT_EQ(tls.validate(), ESP_OK);
    EXPECT_EQ(tls.caCertPath, "/certs/ca.crt");
    EXPECT_EQ(tls.clientCertLabel, "device_cert");
    EXPECT_TRUE(tls.verifyPeer);
}

TEST(EspMqttClientTest, ConfigurationWithBudgeting)
{
    BudgetConfig budget;
    budget.enabled = true;
    budget.defaultBudget = 100;
    budget.maxBudget = 1024;
    budget.reviveCount = 1;
    budget.revivePeriod = std::chrono::seconds(5);

    EXPECT_EQ(budget.validate(), ESP_OK);
    EXPECT_TRUE(budget.enabled);
    EXPECT_EQ(budget.defaultBudget, 100);
}

// =============================================================================
// Connection State Tests
// =============================================================================

TEST(EspMqttClientTest, ConnectionStateTransitions)
{
    // Test valid state transitions
    EXPECT_STREQ(stateToString(MqttConnectionState::DISCONNECTED), "Disconnected");
    EXPECT_STREQ(stateToString(MqttConnectionState::CONNECTING), "Connecting");
    EXPECT_STREQ(stateToString(MqttConnectionState::CONNECTED), "Connected");
    EXPECT_STREQ(stateToString(MqttConnectionState::RECONNECTING), "Reconnecting");
    EXPECT_STREQ(stateToString(MqttConnectionState::DISCONNECTING), "Disconnecting");
    EXPECT_STREQ(stateToString(MqttConnectionState::ERROR), "Error");
}

// =============================================================================
// Message Tests
// =============================================================================

TEST(EspMqttClientTest, MqttMessageCreation)
{
    MqttMessage msg;
    msg.topic = "test/topic";
    msg.payload = {'H', 'e', 'l', 'l', 'o'};
    msg.qos = MqttQos::AT_LEAST_ONCE;
    msg.retained = false;
    msg.messageId = 123;

    EXPECT_EQ(msg.topic, "test/topic");
    EXPECT_EQ(msg.payload.size(), 5);
    EXPECT_EQ(msg.qos, MqttQos::AT_LEAST_ONCE);
    EXPECT_FALSE(msg.retained);
    EXPECT_EQ(msg.messageId, 123);
}

TEST(EspMqttClientTest, MqttMessagePayloadAsString)
{
    MqttMessage msg;
    msg.payload = {'W', 'o', 'r', 'l', 'd'};

    std::string str = msg.getPayloadAsString();
    EXPECT_EQ(str, "World");
}

// =============================================================================
// QoS Conversion Tests
// =============================================================================

TEST(EspMqttClientTest, QosConversion)
{
    EXPECT_EQ(qosToInt(MqttQos::AT_MOST_ONCE), 0);
    EXPECT_EQ(qosToInt(MqttQos::AT_LEAST_ONCE), 1);
    EXPECT_EQ(qosToInt(MqttQos::EXACTLY_ONCE), 2);

    EXPECT_EQ(intToQos(0), MqttQos::AT_MOST_ONCE);
    EXPECT_EQ(intToQos(1), MqttQos::AT_LEAST_ONCE);
    EXPECT_EQ(intToQos(2), MqttQos::EXACTLY_ONCE);
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST(EspMqttClientTest, StatisticsInitialization)
{
    MqttStatistics stats;

    EXPECT_EQ(stats.messagesPublished, 0);
    EXPECT_EQ(stats.messagesReceived, 0);
    EXPECT_EQ(stats.publishErrors, 0);
    EXPECT_EQ(stats.reconnectCount, 0);
    EXPECT_EQ(stats.subscriptionCount, 0);
}

TEST(EspMqttClientTest, StatisticsReset)
{
    MqttStatistics stats;
    stats.messagesPublished = 100;
    stats.messagesReceived = 50;
    stats.publishErrors = 5;
    stats.reconnectCount = 3;

    stats.reset();

    EXPECT_EQ(stats.messagesPublished, 0);
    EXPECT_EQ(stats.messagesReceived, 0);
    EXPECT_EQ(stats.publishErrors, 0);
    EXPECT_EQ(stats.reconnectCount, 0);
}

// =============================================================================
// Builder Pattern Tests
// =============================================================================

TEST(EspMqttClientTest, CompleteConfigurationBuilder)
{
    auto config = MqttConfig::builder()
                      .broker("mqtt.example.com")
                      .port(8883)
                      .clientId("test-device-001")
                      .username("user")
                      .password("pass")
                      .keepAlive(std::chrono::seconds(60))
                      .cleanSession(false)
                      .build();

    EXPECT_EQ(config.validate(), ESP_OK);
    EXPECT_EQ(config.broker, "mqtt.example.com");
    EXPECT_EQ(config.port, 8883);
    EXPECT_EQ(config.clientId, "test-device-001");
    EXPECT_EQ(config.username, "user");
    EXPECT_EQ(config.password, "pass");
}

TEST(EspMqttClientTest, ConfigurationWithSubConfigs)
{
    MqttConfig config;
    config.broker = "mqtt.example.com";
    config.port = 8883;
    config.clientId = "test-client";

    // TLS configuration
    config.tls.caCertPath = "/certs/ca.crt";
    config.tls.verifyPeer = true;

    // Budget configuration
    config.budget.enabled = true;
    config.budget.defaultBudget = 100;

    // Reconnect configuration
    config.reconnect.autoReconnect = true;
    config.reconnect.exponentialBackoff = true;

    // Will configuration
    config.will.topic = "device/status";
    config.will.payload = {'o', 'f', 'f', 'l', 'i', 'n', 'e'};
    config.will.qos = MqttQos::AT_LEAST_ONCE;

    EXPECT_EQ(config.validate(), ESP_OK);
    EXPECT_EQ(config.tls.caCertPath, "/certs/ca.crt");
    EXPECT_TRUE(config.budget.enabled);
    EXPECT_TRUE(config.reconnect.autoReconnect);
    EXPECT_EQ(config.will.topic, "device/status");
}

// =============================================================================
// Error Conversion Tests
// =============================================================================

TEST(EspMqttClientTest, ErrorToString)
{
    EXPECT_STREQ(errorToString(MqttError::NONE), "No error");
    EXPECT_STREQ(errorToString(MqttError::CONNECTION_REFUSED), "Connection refused");
    EXPECT_STREQ(errorToString(MqttError::TIMEOUT), "Timeout");
    EXPECT_STREQ(errorToString(MqttError::BUDGET_EXHAUSTED), "Message budget exhausted");
    EXPECT_STREQ(errorToString(MqttError::PUBLISH_FAILED), "Publish failed");
}

// Note: Actual ESP-MQTT client tests (connect, publish, subscribe) require
// hardware and are tested in integration tests. These unit tests validate
// the configuration, types, and interface compliance.
