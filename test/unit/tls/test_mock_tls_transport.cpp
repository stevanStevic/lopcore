/**
 * @file test_mock_tls_transport.cpp
 * @brief Unit tests for MockTlsTransport
 *
 * Tests the mock TLS transport implementation for use in unit testing.
 *
 * @copyright Copyright (c) 2025
 */

#include <gtest/gtest.h>

#include "tls/mock_tls_transport.hpp"
#include "tls/tls_config.hpp"

using namespace lopcore::tls;
using namespace lopcore::test;

/**
 * @brief Test fixture for MockTlsTransport tests
 */
class MockTlsTransportTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockTransport = std::make_shared<MockTlsTransport>();
    }

    void TearDown() override
    {
        mockTransport->reset();
    }

    std::shared_ptr<MockTlsTransport> mockTransport;
};

// =============================================================================
// Connection Tests
// =============================================================================

TEST_F(MockTlsTransportTest, Connect_Success)
{
    mockTransport->setConnectResult(ESP_OK);

    TlsConfig config{};
    config.hostname = "test.example.com";
    config.port = 8883;

    esp_err_t result = mockTransport->connect(config);

    EXPECT_EQ(result, ESP_OK);
    EXPECT_TRUE(mockTransport->isConnected());
    EXPECT_EQ(mockTransport->getConnectCallCount(), 1);
}

TEST_F(MockTlsTransportTest, Connect_Failure)
{
    mockTransport->setConnectResult(ESP_FAIL);

    TlsConfig config{};
    config.hostname = "test.example.com";
    config.port = 8883;

    esp_err_t result = mockTransport->connect(config);

    EXPECT_EQ(result, ESP_FAIL);
    EXPECT_FALSE(mockTransport->isConnected());
    EXPECT_EQ(mockTransport->getConnectCallCount(), 1);
}

TEST_F(MockTlsTransportTest, Connect_MultipleAttempts)
{
    mockTransport->setConnectResult(ESP_OK);

    TlsConfig config{};
    config.hostname = "test.example.com";
    config.port = 8883;

    mockTransport->connect(config);
    mockTransport->disconnect();
    mockTransport->connect(config);

    EXPECT_TRUE(mockTransport->isConnected());
    EXPECT_EQ(mockTransport->getConnectCallCount(), 2);
}

// =============================================================================
// Disconnection Tests
// =============================================================================

TEST_F(MockTlsTransportTest, Disconnect_WhenConnected)
{
    mockTransport->setConnectResult(ESP_OK);

    TlsConfig config{};
    mockTransport->connect(config);
    EXPECT_TRUE(mockTransport->isConnected());

    mockTransport->disconnect();

    EXPECT_FALSE(mockTransport->isConnected());
}

TEST_F(MockTlsTransportTest, Disconnect_WhenNotConnected)
{
    EXPECT_FALSE(mockTransport->isConnected());

    mockTransport->disconnect(); // Should not crash

    EXPECT_FALSE(mockTransport->isConnected());
}

// =============================================================================
// Send Tests
// =============================================================================

TEST_F(MockTlsTransportTest, Send_Success)
{
    mockTransport->enqueueSendResult(ESP_OK, 10);

    const char *data = "test data!";
    size_t bytesSent = 0;

    esp_err_t result = mockTransport->send(data, 10, &bytesSent);

    EXPECT_EQ(result, ESP_OK);
    EXPECT_EQ(bytesSent, 10);
    EXPECT_EQ(mockTransport->getSendCallCount(), 1);

    auto sentData = mockTransport->getSentData();
    ASSERT_EQ(sentData.size(), 1);
    EXPECT_EQ(sentData[0], std::string(data, 10));
}

TEST_F(MockTlsTransportTest, Send_PartialSuccess)
{
    mockTransport->enqueueSendResult(ESP_OK, 5); // Only 5 of 10 bytes sent

    const char *data = "test data!";
    size_t bytesSent = 0;

    esp_err_t result = mockTransport->send(data, 10, &bytesSent);

    EXPECT_EQ(result, ESP_OK);
    EXPECT_EQ(bytesSent, 5);
}

TEST_F(MockTlsTransportTest, Send_Failure)
{
    mockTransport->enqueueSendResult(ESP_FAIL, 0);

    const char *data = "test";
    size_t bytesSent = 0;

    esp_err_t result = mockTransport->send(data, 4, &bytesSent);

    EXPECT_EQ(result, ESP_FAIL);
    EXPECT_EQ(bytesSent, 0);
}

TEST_F(MockTlsTransportTest, Send_MultipleCalls)
{
    mockTransport->enqueueSendResult(ESP_OK, 5);
    mockTransport->enqueueSendResult(ESP_OK, 3);
    mockTransport->enqueueSendResult(ESP_FAIL, 0);

    size_t bytesSent = 0;

    EXPECT_EQ(mockTransport->send("hello", 5, &bytesSent), ESP_OK);
    EXPECT_EQ(bytesSent, 5);

    EXPECT_EQ(mockTransport->send("123", 3, &bytesSent), ESP_OK);
    EXPECT_EQ(bytesSent, 3);

    EXPECT_EQ(mockTransport->send("fail", 4, &bytesSent), ESP_FAIL);
    EXPECT_EQ(bytesSent, 0);

    EXPECT_EQ(mockTransport->getSendCallCount(), 3);
}

// =============================================================================
// Receive Tests
// =============================================================================

TEST_F(MockTlsTransportTest, Recv_Success)
{
    std::vector<uint8_t> testData = {'h', 'e', 'l', 'l', 'o'};
    mockTransport->enqueueReceiveData(ESP_OK, testData);

    char buffer[10];
    size_t bytesReceived = 0;

    esp_err_t result = mockTransport->recv(buffer, 10, &bytesReceived);

    EXPECT_EQ(result, ESP_OK);
    EXPECT_EQ(bytesReceived, 5);
    EXPECT_EQ(std::string(buffer, 5), "hello");
    EXPECT_EQ(mockTransport->getReceiveCallCount(), 1);
}

TEST_F(MockTlsTransportTest, Recv_PartialData)
{
    std::vector<uint8_t> testData = {'a', 'b', 'c'};
    mockTransport->enqueueReceiveData(ESP_OK, testData);

    char buffer[10];
    size_t bytesReceived = 0;

    esp_err_t result = mockTransport->recv(buffer, 10, &bytesReceived);

    EXPECT_EQ(result, ESP_OK);
    EXPECT_EQ(bytesReceived, 3);
    EXPECT_EQ(std::string(buffer, 3), "abc");
}

TEST_F(MockTlsTransportTest, Recv_Failure)
{
    mockTransport->enqueueReceiveData(ESP_FAIL, {});

    char buffer[10];
    size_t bytesReceived = 0;

    esp_err_t result = mockTransport->recv(buffer, 10, &bytesReceived);

    EXPECT_EQ(result, ESP_FAIL);
    EXPECT_EQ(bytesReceived, 0);
}

TEST_F(MockTlsTransportTest, Recv_MultipleCalls)
{
    mockTransport->enqueueReceiveData(ESP_OK, {'1', '2', '3'});
    mockTransport->enqueueReceiveData(ESP_OK, {'a', 'b'});
    mockTransport->enqueueReceiveData(ESP_FAIL, {});

    char buffer[10];
    size_t bytesReceived = 0;

    EXPECT_EQ(mockTransport->recv(buffer, 10, &bytesReceived), ESP_OK);
    EXPECT_EQ(bytesReceived, 3);
    EXPECT_EQ(std::string(buffer, 3), "123");

    EXPECT_EQ(mockTransport->recv(buffer, 10, &bytesReceived), ESP_OK);
    EXPECT_EQ(bytesReceived, 2);
    EXPECT_EQ(std::string(buffer, 2), "ab");

    EXPECT_EQ(mockTransport->recv(buffer, 10, &bytesReceived), ESP_FAIL);
    EXPECT_EQ(bytesReceived, 0);

    EXPECT_EQ(mockTransport->getReceiveCallCount(), 3);
}

// =============================================================================
// Network Context Tests
// =============================================================================

TEST_F(MockTlsTransportTest, GetNetworkContext_ReturnsNonNull)
{
    void *ctx = mockTransport->getNetworkContext();
    EXPECT_NE(ctx, nullptr);
}

TEST_F(MockTlsTransportTest, GetNetworkContext_Consistent)
{
    void *ctx1 = mockTransport->getNetworkContext();
    void *ctx2 = mockTransport->getNetworkContext();
    EXPECT_EQ(ctx1, ctx2);
}

// =============================================================================
// Reset Tests
// =============================================================================

TEST_F(MockTlsTransportTest, Reset_ClearsAllState)
{
    // Set up some state
    mockTransport->setConnectResult(ESP_OK);
    mockTransport->enqueueSendResult(ESP_OK, 10);
    mockTransport->enqueueReceiveData(ESP_OK, {'t', 'e', 's', 't'});

    TlsConfig config{};
    mockTransport->connect(config);

    const char *data = "test";
    size_t bytes = 0;
    mockTransport->send(data, 4, &bytes);

    // Verify state exists
    EXPECT_TRUE(mockTransport->isConnected());
    EXPECT_GT(mockTransport->getConnectCallCount(), 0);
    EXPECT_GT(mockTransport->getSendCallCount(), 0);

    // Reset
    mockTransport->reset();

    // Verify state cleared
    EXPECT_FALSE(mockTransport->isConnected());
    EXPECT_EQ(mockTransport->getConnectCallCount(), 0);
    EXPECT_EQ(mockTransport->getSendCallCount(), 0);
    EXPECT_EQ(mockTransport->getReceiveCallCount(), 0);
    EXPECT_TRUE(mockTransport->getSentData().empty());
}

// =============================================================================
// Data Inspection Tests
// =============================================================================

TEST_F(MockTlsTransportTest, GetSentData_TracksAllData)
{
    mockTransport->enqueueSendResult(ESP_OK, 5);
    mockTransport->enqueueSendResult(ESP_OK, 6);

    size_t bytes = 0;
    mockTransport->send("hello", 5, &bytes);
    mockTransport->send("world!", 6, &bytes);

    auto sentData = mockTransport->getSentData();
    ASSERT_EQ(sentData.size(), 2);
    EXPECT_EQ(sentData[0], std::string("hello"));
    EXPECT_EQ(sentData[1], std::string("world!"));
}

TEST_F(MockTlsTransportTest, GetSentData_EmptyWhenNoSends)
{
    auto sentData = mockTransport->getSentData();
    EXPECT_TRUE(sentData.empty());
}
