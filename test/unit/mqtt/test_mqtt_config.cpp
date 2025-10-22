/**
 * @file test_mqtt_config.cpp
 * @brief Unit tests for MQTT configuration and builder pattern
 */

#include <gtest/gtest.h>
#include <mqtt/mqtt_config.hpp>
#include <mqtt/mqtt_types.hpp>

using namespace lopcore::mqtt;

// =============================================================================
// TlsConfig Tests
// =============================================================================

TEST(MqttConfigTest, TlsConfigValidation_Disabled)
{
    TlsConfig config;
    config.enabled = false;

    EXPECT_EQ(config.validate(), ESP_OK);
}

TEST(MqttConfigTest, TlsConfigValidation_MissingCaCert)
{
    TlsConfig config;
    config.enabled = true;
    config.caCertPath = "";

    EXPECT_EQ(config.validate(), ESP_ERR_INVALID_ARG);
}

TEST(MqttConfigTest, TlsConfigValidation_MissingClientCreds)
{
    TlsConfig config;
    config.enabled = true;
    config.caCertPath = "/spiffs/certs/ca.crt";
    config.verifyPeer = true;
    config.clientCertLabel = "";
    config.clientKeyLabel = "";

    EXPECT_EQ(config.validate(), ESP_ERR_INVALID_ARG);
}

TEST(MqttConfigTest, TlsConfigValidation_Valid)
{
    TlsConfig config;
    config.enabled = true;
    config.caCertPath = "/spiffs/certs/ca.crt";
    config.clientCertLabel = "device_cert";
    config.clientKeyLabel = "device_key";
    config.verifyPeer = true;

    EXPECT_EQ(config.validate(), ESP_OK);
}

// =============================================================================
// BudgetConfig Tests
// =============================================================================

TEST(MqttConfigTest, BudgetConfigValidation_NegativeValues)
{
    BudgetConfig config;
    config.defaultBudget = -1;
    config.maxBudget = 100;

    EXPECT_EQ(config.validate(), ESP_ERR_INVALID_ARG);
}

TEST(MqttConfigTest, BudgetConfigValidation_DefaultExceedsMax)
{
    BudgetConfig config;
    config.defaultBudget = 200;
    config.maxBudget = 100;

    EXPECT_EQ(config.validate(), ESP_ERR_INVALID_ARG);
}

TEST(MqttConfigTest, BudgetConfigValidation_ZeroRevive)
{
    BudgetConfig config;
    config.defaultBudget = 100;
    config.maxBudget = 1024;
    config.reviveCount = 0;

    EXPECT_EQ(config.validate(), ESP_ERR_INVALID_ARG);
}

TEST(MqttConfigTest, BudgetConfigValidation_Valid)
{
    BudgetConfig config;
    config.enabled = true;
    config.defaultBudget = 100;
    config.maxBudget = 1024;
    config.reviveCount = 1;
    config.revivePeriod = std::chrono::seconds(5);

    EXPECT_EQ(config.validate(), ESP_OK);
}

// =============================================================================
// ReconnectConfig Tests
// =============================================================================

TEST(MqttConfigTest, ReconnectConfigValidation_InvalidDelays)
{
    ReconnectConfig config;
    config.initialDelay = std::chrono::milliseconds(5000);
    config.maxDelay = std::chrono::milliseconds(1000);

    EXPECT_EQ(config.validate(), ESP_ERR_INVALID_ARG);
}

TEST(MqttConfigTest, ReconnectConfigValidation_InvalidMultiplier)
{
    ReconnectConfig config;
    config.initialDelay = std::chrono::milliseconds(1000);
    config.maxDelay = std::chrono::milliseconds(60000);
    config.backoffMultiplier = 0.5f;

    EXPECT_EQ(config.validate(), ESP_ERR_INVALID_ARG);
}

TEST(MqttConfigTest, ReconnectConfigValidation_InvalidJitter)
{
    ReconnectConfig config;
    config.initialDelay = std::chrono::milliseconds(1000);
    config.maxDelay = std::chrono::milliseconds(60000);
    config.jitterFactor = 1.5f;

    EXPECT_EQ(config.validate(), ESP_ERR_INVALID_ARG);
}

TEST(MqttConfigTest, ReconnectConfigValidation_Valid)
{
    ReconnectConfig config;
    config.autoReconnect = true;
    config.initialDelay = std::chrono::milliseconds(1000);
    config.maxDelay = std::chrono::milliseconds(60000);
    config.maxAttempts = 10;
    config.exponentialBackoff = true;
    config.backoffMultiplier = 2.0f;
    config.jitterFactor = 0.25f;

    EXPECT_EQ(config.validate(), ESP_OK);
}

// =============================================================================
// WillConfig Tests
// =============================================================================

TEST(MqttConfigTest, WillConfigIsConfigured)
{
    WillConfig config;
    EXPECT_FALSE(config.isConfigured());

    config.topic = "device/status";
    EXPECT_TRUE(config.isConfigured());
}

TEST(MqttConfigTest, WillConfigValidation_Wildcards)
{
    WillConfig config;
    config.topic = "device/#";

    EXPECT_EQ(config.validate(), ESP_ERR_INVALID_ARG);

    config.topic = "device/+/status";
    EXPECT_EQ(config.validate(), ESP_ERR_INVALID_ARG);
}

TEST(MqttConfigTest, WillConfigValidation_Valid)
{
    WillConfig config;
    config.topic = "device/status";
    config.payload = {'o', 'f', 'f', 'l', 'i', 'n', 'e'};
    config.qos = MqttQos::AT_LEAST_ONCE;
    config.retain = true;

    EXPECT_EQ(config.validate(), ESP_OK);
}

// =============================================================================
// MqttConfig Tests
// =============================================================================

TEST(MqttConfigTest, MqttConfigValidation_MissingBroker)
{
    MqttConfig config;
    config.broker = "";
    config.port = 1883;
    config.clientId = "test-client";

    EXPECT_EQ(config.validate(), ESP_ERR_INVALID_ARG);
}

TEST(MqttConfigTest, MqttConfigValidation_InvalidPort)
{
    MqttConfig config;
    config.broker = "mqtt.example.com";
    config.port = 0;
    config.clientId = "test-client";

    EXPECT_EQ(config.validate(), ESP_ERR_INVALID_ARG);
}

TEST(MqttConfigTest, MqttConfigValidation_MissingClientId)
{
    MqttConfig config;
    config.broker = "mqtt.example.com";
    config.port = 1883;
    config.clientId = "";

    EXPECT_EQ(config.validate(), ESP_ERR_INVALID_ARG);
}

TEST(MqttConfigTest, MqttConfigValidation_InvalidBufferSize)
{
    MqttConfig config;
    config.broker = "mqtt.example.com";
    config.port = 1883;
    config.clientId = "test-client";
    config.networkBufferSize = 512; // Too small

    EXPECT_EQ(config.validate(), ESP_ERR_INVALID_ARG);
}

TEST(MqttConfigTest, MqttConfigValidation_InvalidProcessLoopDelay_Zero)
{
    MqttConfig config;
    config.broker = "mqtt.example.com";
    config.port = 1883;
    config.clientId = "test-client";
    config.processLoopDelayMs = 0; // Too low

    EXPECT_EQ(config.validate(), ESP_ERR_INVALID_ARG);
}

TEST(MqttConfigTest, MqttConfigValidation_InvalidProcessLoopDelay_TooHigh)
{
    MqttConfig config;
    config.broker = "mqtt.example.com";
    config.port = 1883;
    config.clientId = "test-client";
    config.processLoopDelayMs = 1001; // Too high

    EXPECT_EQ(config.validate(), ESP_ERR_INVALID_ARG);
}

TEST(MqttConfigTest, MqttConfigValidation_ValidProcessLoopDelay_Minimum)
{
    MqttConfig config;
    config.broker = "mqtt.example.com";
    config.port = 1883;
    config.clientId = "test-client";
    config.processLoopDelayMs = 1; // Minimum valid

    EXPECT_EQ(config.validate(), ESP_OK);
}

TEST(MqttConfigTest, MqttConfigValidation_ValidProcessLoopDelay_Maximum)
{
    MqttConfig config;
    config.broker = "mqtt.example.com";
    config.port = 1883;
    config.clientId = "test-client";
    config.processLoopDelayMs = 1000; // Maximum valid

    EXPECT_EQ(config.validate(), ESP_OK);
}

TEST(MqttConfigTest, MqttConfigValidation_ValidProcessLoopDelay_Default)
{
    MqttConfig config;
    config.broker = "mqtt.example.com";
    config.port = 1883;
    config.clientId = "test-client";
    // processLoopDelayMs = 10 (default)

    EXPECT_EQ(config.validate(), ESP_OK);
    EXPECT_EQ(config.processLoopDelayMs, 10);
}

TEST(MqttConfigTest, MqttConfigValidation_Valid)
{
    MqttConfig config;
    config.broker = "mqtt.example.com";
    config.port = 1883;
    config.clientId = "test-client";
    config.keepAlive = std::chrono::seconds(60);
    config.cleanSession = true;
    config.networkBufferSize = 4096;

    EXPECT_EQ(config.validate(), ESP_OK);
}

// =============================================================================
// Builder Pattern Tests
// =============================================================================

TEST(MqttConfigTest, BuilderBasicConfiguration)
{
    auto config = MqttConfig::builder()
                      .broker("mqtt.example.com")
                      .port(1883)
                      .clientId("test-client-123")
                      .keepAlive(std::chrono::seconds(60))
                      .cleanSession(true)
                      .build();

    EXPECT_EQ(config.broker, "mqtt.example.com");
    EXPECT_EQ(config.port, 1883);
    EXPECT_EQ(config.clientId, "test-client-123");
    EXPECT_EQ(config.keepAlive.count(), 60);
    EXPECT_TRUE(config.cleanSession);
    EXPECT_EQ(config.validate(), ESP_OK);
}

TEST(MqttConfigTest, BuilderWithAuthentication)
{
    auto config = MqttConfig::builder()
                      .broker("mqtt.example.com")
                      .port(1883)
                      .clientId("test-client")
                      .username("user123")
                      .password("secret")
                      .build();

    EXPECT_EQ(config.username, "user123");
    EXPECT_EQ(config.password, "secret");
}

TEST(MqttConfigTest, BuilderWithTls)
{
    auto tlsConfig = TlsConfigBuilder()
                         .enableTls(true)
                         .caCertPath("/spiffs/certs/ca.crt")
                         .clientCertLabel("device_cert")
                         .clientKeyLabel("device_key")
                         .alpn("x-amzn-mqtt-ca")
                         .verifyPeer(true)
                         .build();

    auto config = MqttConfig::builder()
                      .broker("mqtt.example.com")
                      .port(8883)
                      .clientId("test-client")
                      .tlsConfig(tlsConfig)
                      .build();

    EXPECT_TRUE(config.tls.enabled);
    EXPECT_EQ(config.tls.caCertPath, "/spiffs/certs/ca.crt");
    EXPECT_EQ(config.tls.clientCertLabel, "device_cert");
    EXPECT_EQ(config.tls.clientKeyLabel, "device_key");
    EXPECT_EQ(config.tls.alpn.value(), "x-amzn-mqtt-ca");
    EXPECT_TRUE(config.tls.verifyPeer);
}

TEST(MqttConfigTest, BuilderWithBudgeting)
{
    auto budgetConfig = BudgetConfigBuilder()
                            .enabled(true)
                            .defaultBudget(100)
                            .maxBudget(1024)
                            .reviveCount(1)
                            .revivePeriod(std::chrono::seconds(5))
                            .build();

    auto config = MqttConfig::builder()
                      .broker("mqtt.example.com")
                      .port(1883)
                      .clientId("test-client")
                      .budgetConfig(budgetConfig)
                      .build();

    EXPECT_TRUE(config.budget.enabled);
    EXPECT_EQ(config.budget.defaultBudget, 100);
    EXPECT_EQ(config.budget.maxBudget, 1024);
    EXPECT_EQ(config.budget.reviveCount, 1);
    EXPECT_EQ(config.budget.revivePeriod.count(), 5);
}

TEST(MqttConfigTest, BuilderWithReconnection)
{
    auto reconnectConfig = ReconnectConfigBuilder()
                               .autoReconnect(true)
                               .initialDelay(std::chrono::milliseconds(1000))
                               .maxDelay(std::chrono::milliseconds(60000))
                               .maxAttempts(10)
                               .exponentialBackoff(true)
                               .backoffMultiplier(2.0f)
                               .jitterFactor(0.25f)
                               .build();

    auto config = MqttConfig::builder()
                      .broker("mqtt.example.com")
                      .port(1883)
                      .clientId("test-client")
                      .reconnectConfig(reconnectConfig)
                      .build();

    EXPECT_TRUE(config.reconnect.autoReconnect);
    EXPECT_EQ(config.reconnect.initialDelay.count(), 1000);
    EXPECT_EQ(config.reconnect.maxDelay.count(), 60000);
    EXPECT_EQ(config.reconnect.maxAttempts, 10);
    EXPECT_TRUE(config.reconnect.exponentialBackoff);
    EXPECT_FLOAT_EQ(config.reconnect.backoffMultiplier, 2.0f);
    EXPECT_FLOAT_EQ(config.reconnect.jitterFactor, 0.25f);
}

TEST(MqttConfigTest, BuilderWithWill)
{
    std::vector<uint8_t> payload = {'o', 'f', 'f', 'l', 'i', 'n', 'e'};

    auto config = MqttConfig::builder()
                      .broker("mqtt.example.com")
                      .port(1883)
                      .clientId("test-client")
                      .willTopic("device/status")
                      .willPayload(payload)
                      .willQos(MqttQos::AT_LEAST_ONCE)
                      .willRetain(true)
                      .build();

    EXPECT_TRUE(config.will.isConfigured());
    EXPECT_EQ(config.will.topic, "device/status");
    EXPECT_EQ(config.will.payload, payload);
    EXPECT_EQ(config.will.qos, MqttQos::AT_LEAST_ONCE);
    EXPECT_TRUE(config.will.retain);
}

TEST(MqttConfigTest, BuilderCompleteConfiguration)
{
    auto config = MqttConfig::builder()
                      .broker("a1234567890abc-ats.iot.us-east-1.amazonaws.com")
                      .port(8883)
                      .clientId("device-12345")
                      .keepAlive(std::chrono::seconds(60))
                      .cleanSession(true)
                      .networkBufferSize(8192)
                      .tlsConfig(TlsConfigBuilder()
                                     .enableTls(true)
                                     .caCertPath("/spiffs/certs/AmazonRootCA1.crt")
                                     .clientCertLabel("device_cert")
                                     .clientKeyLabel("device_key")
                                     .alpn("x-amzn-mqtt-ca")
                                     .verifyPeer(true)
                                     .timeout(10000)
                                     .build())
                      .budgetConfig(BudgetConfigBuilder()
                                        .enabled(true)
                                        .defaultBudget(100)
                                        .maxBudget(1024)
                                        .reviveCount(1)
                                        .revivePeriod(std::chrono::seconds(5))
                                        .build())
                      .reconnectConfig(ReconnectConfigBuilder()
                                           .autoReconnect(true)
                                           .initialDelay(std::chrono::milliseconds(1000))
                                           .maxDelay(std::chrono::milliseconds(60000))
                                           .maxAttempts(0) // Infinite
                                           .exponentialBackoff(true)
                                           .build())
                      .willTopic("device/12345/status")
                      .willPayload({'o', 'f', 'f', 'l', 'i', 'n', 'e'})
                      .willQos(MqttQos::AT_LEAST_ONCE)
                      .willRetain(true)
                      .build();

    // Validate entire configuration
    EXPECT_EQ(config.validate(), ESP_OK);

    // Verify all components
    EXPECT_EQ(config.broker, "a1234567890abc-ats.iot.us-east-1.amazonaws.com");
    EXPECT_EQ(config.port, 8883);
    EXPECT_TRUE(config.tls.enabled);
    EXPECT_TRUE(config.budget.enabled);
    EXPECT_TRUE(config.reconnect.autoReconnect);
    EXPECT_TRUE(config.will.isConfigured());
}
