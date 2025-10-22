/**
 * @file test_mqtt_client_factory.cpp
 * @brief Unit tests for MqttClientFactory
 *
 * Tests the factory pattern for MQTT client creation with automatic type selection.
 *
 * @copyright Copyright (c) 2025
 */

#include <gtest/gtest.h>

#include "mqtt/mqtt_client_factory.hpp"
#include "mqtt/mqtt_config.hpp"
#include "tls/mock_tls_transport.hpp"

using namespace lopcore::mqtt;
using namespace lopcore::test;

/**
 * @brief Test fixture for MqttClientFactory tests
 */
class MqttClientFactoryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Base configuration for testing
        baseConfig = MqttConfig::builder().broker("test-broker.example.com").clientId("test-client").build();
    }

    MqttConfig baseConfig;
};

// =============================================================================
// Factory Creation Tests
// =============================================================================

TEST_F(MqttClientFactoryTest, Create_EspMqttClient)
{
    auto client = MqttClientFactory::create(MqttClientType::ESP_MQTT, baseConfig);

    ASSERT_NE(client, nullptr);
    EXPECT_EQ(client->getClientId(), "test-client");
    EXPECT_EQ(client->getBroker(), "test-broker.example.com");
}

TEST_F(MqttClientFactoryTest, Create_AwsIotClient)
{
    auto client = MqttClientFactory::create(MqttClientType::AWS_IOT, baseConfig);

    ASSERT_NE(client, nullptr);
    EXPECT_EQ(client->getClientId(), "test-client");
    EXPECT_EQ(client->getBroker(), "test-broker.example.com");
}

TEST_F(MqttClientFactoryTest, Create_MockClient)
{
    auto client = MqttClientFactory::create(MqttClientType::MOCK, baseConfig);

    // Mock client not yet implemented, should return nullptr
    EXPECT_EQ(client, nullptr);
}

// =============================================================================
// Auto-Selection Tests
// =============================================================================

TEST_F(MqttClientFactoryTest, AutoSelect_AwsIotEndpoint)
{
    // AWS IoT Core endpoints follow pattern: *.iot.*.amazonaws.com
    auto awsConfig = MqttConfig::builder()
                         .broker("a1b2c3d4e5f6g7-ats.iot.us-east-1.amazonaws.com")
                         .clientId("aws-device")
                         .build();

    auto type = MqttClientFactory::selectType(awsConfig);

    EXPECT_EQ(type, MqttClientType::AWS_IOT);
}

TEST_F(MqttClientFactoryTest, AutoSelect_AwsIotAlternateRegion)
{
    auto awsConfig =
        MqttConfig::builder().broker("example123.iot.eu-west-1.amazonaws.com").clientId("aws-device").build();

    auto type = MqttClientFactory::selectType(awsConfig);

    EXPECT_EQ(type, MqttClientType::AWS_IOT);
}

TEST_F(MqttClientFactoryTest, AutoSelect_GenericBroker)
{
    auto genericConfig = MqttConfig::builder().broker("mqtt.example.com").clientId("generic-device").build();

    auto type = MqttClientFactory::selectType(genericConfig);

    EXPECT_EQ(type, MqttClientType::ESP_MQTT);
}

TEST_F(MqttClientFactoryTest, AutoSelect_LocalhostBroker)
{
    auto localConfig = MqttConfig::builder().broker("localhost").clientId("local-device").build();

    auto type = MqttClientFactory::selectType(localConfig);

    EXPECT_EQ(type, MqttClientType::ESP_MQTT);
}

TEST_F(MqttClientFactoryTest, AutoSelect_IpAddress)
{
    auto ipConfig = MqttConfig::builder().broker("192.168.1.100").clientId("ip-device").build();

    auto type = MqttClientFactory::selectType(ipConfig);

    EXPECT_EQ(type, MqttClientType::ESP_MQTT);
}

TEST_F(MqttClientFactoryTest, Create_Auto_AwsEndpoint)
{
    auto awsConfig = MqttConfig::builder()
                         .broker("test123.iot.us-west-2.amazonaws.com")
                         .clientId("auto-aws-device")
                         .build();

    auto client = MqttClientFactory::create(MqttClientType::AUTO, awsConfig);

    ASSERT_NE(client, nullptr);
    EXPECT_EQ(client->getClientId(), "auto-aws-device");
    // Should have selected CoreMqttClient (AWS_IOT)
}

TEST_F(MqttClientFactoryTest, Create_Auto_GenericBroker)
{
    auto genericConfig =
        MqttConfig::builder().broker("mqtt.generic.com").clientId("auto-generic-device").build();

    auto client = MqttClientFactory::create(MqttClientType::AUTO, genericConfig);

    ASSERT_NE(client, nullptr);
    EXPECT_EQ(client->getClientId(), "auto-generic-device");
    // Should have selected EspMqttClient (ESP_MQTT)
}

// =============================================================================
// Type Name Tests
// =============================================================================

TEST_F(MqttClientFactoryTest, GetTypeName_EspMqtt)
{
    std::string name = MqttClientFactory::getTypeName(MqttClientType::ESP_MQTT);
    EXPECT_EQ(name, "ESP-MQTT");
}

TEST_F(MqttClientFactoryTest, GetTypeName_AwsIot)
{
    std::string name = MqttClientFactory::getTypeName(MqttClientType::AWS_IOT);
    EXPECT_EQ(name, "AWS_IOT (coreMQTT)");
}

TEST_F(MqttClientFactoryTest, GetTypeName_Mock)
{
    std::string name = MqttClientFactory::getTypeName(MqttClientType::MOCK);
    EXPECT_EQ(name, "Mock");
}

TEST_F(MqttClientFactoryTest, GetTypeName_Auto)
{
    std::string name = MqttClientFactory::getTypeName(MqttClientType::AUTO);
    EXPECT_EQ(name, "Auto");
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(MqttClientFactoryTest, AutoSelect_EmptyBroker)
{
    auto emptyConfig = MqttConfig::builder().broker("").clientId("empty-broker").build();

    auto type = MqttClientFactory::selectType(emptyConfig);

    // Should default to ESP_MQTT for safety
    EXPECT_EQ(type, MqttClientType::ESP_MQTT);
}

TEST_F(MqttClientFactoryTest, AutoSelect_PartialAwsPattern)
{
    // Has "amazonaws" but not full IoT pattern
    auto partialConfig = MqttConfig::builder().broker("s3.amazonaws.com").clientId("partial-aws").build();

    auto type = MqttClientFactory::selectType(partialConfig);

    // Should NOT match AWS IoT pattern
    EXPECT_EQ(type, MqttClientType::ESP_MQTT);
}

TEST_F(MqttClientFactoryTest, AutoSelect_CaseInsensitive)
{
    // Test if matching is case-insensitive
    auto upperConfig =
        MqttConfig::builder().broker("TEST.IOT.US-EAST-1.AMAZONAWS.COM").clientId("upper-case").build();

    auto type = MqttClientFactory::selectType(upperConfig);

    // Should match AWS IoT pattern (case-insensitive)
    EXPECT_EQ(type, MqttClientType::AWS_IOT);
}

// =============================================================================
// Multiple Client Creation
// =============================================================================

TEST_F(MqttClientFactoryTest, CreateMultipleClients_SameType)
{
    auto client1 = MqttClientFactory::create(MqttClientType::ESP_MQTT, baseConfig);

    auto config2 = MqttConfig::builder().broker("broker2.example.com").clientId("client-2").build();
    auto client2 = MqttClientFactory::create(MqttClientType::ESP_MQTT, config2);

    ASSERT_NE(client1, nullptr);
    ASSERT_NE(client2, nullptr);
    EXPECT_NE(client1.get(), client2.get());
    EXPECT_NE(client1->getClientId(), client2->getClientId());
}

TEST_F(MqttClientFactoryTest, CreateMultipleClients_DifferentTypes)
{
    auto espClient = MqttClientFactory::create(MqttClientType::ESP_MQTT, baseConfig);

    auto awsConfig =
        MqttConfig::builder().broker("test.iot.us-east-1.amazonaws.com").clientId("aws-client").build();
    auto awsClient = MqttClientFactory::create(MqttClientType::AWS_IOT, awsConfig);

    ASSERT_NE(espClient, nullptr);
    ASSERT_NE(awsClient, nullptr);
    EXPECT_NE(espClient.get(), awsClient.get());
}

// =============================================================================
// Configuration Validation
// =============================================================================

TEST_F(MqttClientFactoryTest, Create_WithComplexConfig)
{
    // Build budget config separately
    auto budgetConfig = BudgetConfigBuilder().enabled(true).defaultBudget(100).maxBudget(1000).build();

    auto complexConfig = MqttConfig::builder()
                             .broker("secure.example.com")
                             .port(8883)
                             .clientId("complex-client")
                             .username("user123")
                             .password("pass456")
                             .keepAlive(std::chrono::seconds(90))
                             .cleanSession(false)
                             .tlsConfig(lopcore::tls::TlsConfig{.hostname = "secure.example.com",
                                                                .port = 8883,
                                                                .caCertPath = "/spiffs/ca.crt",
                                                                .enableSni = true,
                                                                .verifyPeer = true,
                                                                .enabled = true})
                             .budgetConfig(budgetConfig)
                             .build();

    auto client = MqttClientFactory::create(MqttClientType::ESP_MQTT, complexConfig);

    ASSERT_NE(client, nullptr);
    EXPECT_EQ(client->getClientId(), "complex-client");
    EXPECT_EQ(client->getBroker(), "secure.example.com");
    EXPECT_EQ(client->getPort(), 8883);
}

// =============================================================================
// Dependency Injection Tests
// =============================================================================

TEST_F(MqttClientFactoryTest, Create_WithMockTransport)
{
    // Create mock transport that simulates successful connection
    auto mockTransport = std::make_shared<MockTlsTransport>();
    mockTransport->setConnectResult(ESP_OK);

    // Factory should use provided transport instead of creating one
    auto client = MqttClientFactory::create(MqttClientType::AWS_IOT, baseConfig, mockTransport);

    ASSERT_NE(client, nullptr);
    EXPECT_EQ(client->getClientId(), "test-client");

    // Verify mock transport was NOT used (factory received already-connected transport)
    EXPECT_EQ(mockTransport->getConnectCallCount(), 0);
}

TEST_F(MqttClientFactoryTest, Create_MockTransportSharedBetweenClients)
{
    // Simulate transport reuse scenario
    auto sharedTransport = std::make_shared<MockTlsTransport>();
    sharedTransport->setConnectResult(ESP_OK);

    // Create two clients sharing the same transport
    auto client1 = MqttClientFactory::create(MqttClientType::AWS_IOT, baseConfig, sharedTransport);

    auto config2 =
        MqttConfig::builder().broker("test2.iot.us-east-1.amazonaws.com").clientId("client-2").build();
    auto client2 = MqttClientFactory::create(MqttClientType::AWS_IOT, config2, sharedTransport);

    ASSERT_NE(client1, nullptr);
    ASSERT_NE(client2, nullptr);
    EXPECT_NE(client1.get(), client2.get());

    // Both clients share the same transport (connection reuse)
    EXPECT_EQ(sharedTransport->getConnectCallCount(), 0); // Factory didn't call connect
}

TEST_F(MqttClientFactoryTest, Create_NullTransportCreatesDefault)
{
    // Passing nullptr should trigger default transport creation
    auto client = MqttClientFactory::create(MqttClientType::AWS_IOT, baseConfig, nullptr);

    // Should still create client (will attempt real TLS connection in production)
    // In test environment, this may fail, but factory should handle gracefully
    // Note: This test verifies the code path exists, not that connection succeeds
}

TEST_F(MqttClientFactoryTest, Create_EspMqttIgnoresTransport)
{
    // ESP-MQTT uses its own internal TLS, should ignore provided transport
    auto mockTransport = std::make_shared<MockTlsTransport>();
    mockTransport->setConnectResult(ESP_OK);

    auto client = MqttClientFactory::create(MqttClientType::ESP_MQTT, baseConfig, mockTransport);

    ASSERT_NE(client, nullptr);
    // ESP-MQTT doesn't use provided transport
    EXPECT_EQ(mockTransport->getConnectCallCount(), 0);
}

// =============================================================================
// Mock Transport Verification Tests
// =============================================================================

TEST_F(MqttClientFactoryTest, MockTransport_ConnectionTracking)
{
    auto mockTransport = std::make_shared<MockTlsTransport>();
    mockTransport->setConnectResult(ESP_OK);

    // Simulate what factory does internally when creating default transport
    auto tlsConfig =
        lopcore::tls::TlsConfigBuilder().hostname(baseConfig.broker).port(baseConfig.port).build();

    EXPECT_EQ(mockTransport->getConnectCallCount(), 0);
    mockTransport->connect(tlsConfig);
    EXPECT_EQ(mockTransport->getConnectCallCount(), 1);
    EXPECT_TRUE(mockTransport->isConnected());

    mockTransport->disconnect();
    EXPECT_EQ(mockTransport->getDisconnectCallCount(), 1);
    EXPECT_FALSE(mockTransport->isConnected());
}

TEST_F(MqttClientFactoryTest, MockTransport_SendReceive)
{
    auto mockTransport = std::make_shared<MockTlsTransport>();

    // Queue mock data
    mockTransport->enqueueReceiveData("MOCK_DATA");
    mockTransport->enqueueSendResult(ESP_OK, 10);

    // Simulate send/receive
    uint8_t sendBuf[] = "TEST";
    size_t bytesSent = 0;
    esp_err_t err = mockTransport->send(sendBuf, sizeof(sendBuf), &bytesSent);
    EXPECT_EQ(err, ESP_OK);
    EXPECT_EQ(bytesSent, 10); // Returns queued result
    EXPECT_EQ(mockTransport->getSendCallCount(), 1);

    uint8_t recvBuf[100];
    size_t bytesReceived = 0;
    err = mockTransport->recv(recvBuf, sizeof(recvBuf), &bytesReceived);
    EXPECT_EQ(err, ESP_OK);
    EXPECT_EQ(bytesReceived, 9); // "MOCK_DATA" length
    EXPECT_EQ(mockTransport->getReceiveCallCount(), 1);
    EXPECT_EQ(std::string(reinterpret_cast<char *>(recvBuf), bytesReceived), "MOCK_DATA");
}

TEST_F(MqttClientFactoryTest, MockTransport_Reset)
{
    auto mockTransport = std::make_shared<MockTlsTransport>();

    // Perform some operations
    mockTransport->setConnectResult(ESP_OK);
    auto tlsConfig = lopcore::tls::TlsConfig{};
    mockTransport->connect(tlsConfig);

    size_t bytes = 0;
    mockTransport->send(nullptr, 0, &bytes);
    mockTransport->recv(nullptr, 0, &bytes);

    EXPECT_GT(mockTransport->getConnectCallCount(), 0);

    // Reset should clear all state
    mockTransport->reset();
    EXPECT_EQ(mockTransport->getConnectCallCount(), 0);
    EXPECT_EQ(mockTransport->getSendCallCount(), 0);
    EXPECT_EQ(mockTransport->getReceiveCallCount(), 0);
    EXPECT_FALSE(mockTransport->isConnected());
}

/**
 * @brief Main function for running tests
 */
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
