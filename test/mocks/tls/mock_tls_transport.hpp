/**
 * @file mock_tls_transport.hpp
 * @brief Mock TLS transport for unit testing
 *
 * Provides a fake TLS transport implementation that doesn't require network I/O.
 * Useful for testing MQTT clients, factories, and other components that depend
 * on TLS transport without actually connecting to a server.
 *
 * Features:
 * - Configurable connection success/failure
 * - Simulated send/receive operations
 * - Connection state tracking
 * - Call counting for verification
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <cstring>
#include <queue>
#include <vector>

#include "tls/tls_config.hpp"
#include "tls/tls_transport.hpp"

namespace lopcore
{
namespace test
{

/**
 * @brief Mock TLS transport for testing
 *
 * This mock allows testing components that depend on ITlsTransport without
 * requiring actual network connections or TLS handshakes.
 *
 * Usage:
 * @code
 * auto mockTransport = std::make_shared<MockTlsTransport>();
 * mockTransport->setConnectResult(ESP_OK);  // Simulate successful connection
 * mockTransport->enqueueSendResult(100);    // Next send returns 100 bytes
 * mockTransport->enqueueReceiveData("HELLO"); // Next receive returns "HELLO"
 *
 * // Use in tests with direct construction
 * auto client = std::make_unique<CoreMqttClient>(config, mockTransport);
 * @endcode
 */
class MockTlsTransport : public lopcore::tls::ITlsTransport
{
public:
    MockTlsTransport() = default;
    ~MockTlsTransport() override = default;

    // =================================================================
    // ITlsTransport Interface Implementation
    // =================================================================

    /**
     * @brief Simulate TLS connection
     * @return Pre-configured result (default: ESP_OK)
     */
    esp_err_t connect(const lopcore::tls::TlsConfig &config) override
    {
        connectCallCount_++;
        lastConnectConfig_ = config;
        isConnected_ = (connectResult_ == ESP_OK);
        return connectResult_;
    }

    /**
     * @brief Simulate TLS disconnection
     */
    void disconnect() noexcept override
    {
        disconnectCallCount_++;
        isConnected_ = false;
    }

    /**
     * @brief Check if mock transport is "connected"
     */
    bool isConnected() const noexcept override
    {
        return isConnected_;
    }

    /**
     * @brief Simulate sending data
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t send(const void *data, size_t size, size_t *bytesSent) override
    {
        sendCallCount_++;

        if (!data || !bytesSent)
        {
            return ESP_ERR_INVALID_ARG;
        }

        // Store sent data for verification
        const uint8_t *bytes = static_cast<const uint8_t *>(data);
        sentData_.insert(sentData_.end(), bytes, bytes + size);

        // Return queued result or default to success
        if (!sendResults_.empty())
        {
            auto result = sendResults_.front();
            sendResults_.pop();
            *bytesSent = result.bytesSent;
            return result.status;
        }

        *bytesSent = size; // Default: all bytes sent
        return ESP_OK;
    }

    /**
     * @brief Simulate receiving data
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t recv(void *buffer, size_t size, size_t *bytesReceived) override
    {
        receiveCallCount_++;

        if (!buffer || !bytesReceived)
        {
            return ESP_ERR_INVALID_ARG;
        }

        // Return queued data if available
        if (!receiveDataQueue_.empty())
        {
            const auto &data = receiveDataQueue_.front();
            size_t copySize = std::min(size, data.size());
            std::memcpy(buffer, data.data(), copySize);
            receiveDataQueue_.pop();
            *bytesReceived = copySize;
            return ESP_OK;
        }

        // Return queued error if available
        if (!receiveResults_.empty())
        {
            auto result = receiveResults_.front();
            receiveResults_.pop();
            *bytesReceived = result.bytesReceived;
            return result.status;
        }

        // Default: no data available
        *bytesReceived = 0;
        return ESP_OK;
    }

    /**
     * @brief Get opaque network context (returns nullptr for mock)
     */
    void *getNetworkContext() noexcept override
    {
        return nullptr; // Mock doesn't have real network context
    }

    // =================================================================
    // Mock Configuration Methods
    // =================================================================

    /**
     * @brief Result structure for send/recv operations
     */
    struct TransportResult
    {
        esp_err_t status;
        size_t bytesSent;
        size_t bytesReceived;
    };

    /**
     * @brief Set the result that connect() will return
     * @param result ESP_OK for success, error code for failure
     */
    void setConnectResult(esp_err_t result)
    {
        connectResult_ = result;
    }

    /**
     * @brief Queue a result for the next send() call
     * @param status ESP_OK for success, error code for failure
     * @param bytesSent Number of bytes successfully sent
     */
    void enqueueSendResult(esp_err_t status, size_t bytesSent = 0)
    {
        sendResults_.push({status, bytesSent, 0});
    }

    /**
     * @brief Queue a result for the next receive() call
     * @param status ESP_OK for success, error code for failure
     * @param bytesReceived Number of bytes successfully received
     */
    void enqueueReceiveResult(esp_err_t status, size_t bytesReceived = 0)
    {
        receiveResults_.push({status, 0, bytesReceived});
    }

    /**
     * @brief Queue data to be returned by receive()
     * @param data Data bytes to return
     */
    void enqueueReceiveData(const std::vector<uint8_t> &data)
    {
        receiveDataQueue_.push(data);
    }

    /**
     * @brief Queue string data to be returned by receive()
     * @param str String to return (converted to bytes)
     */
    void enqueueReceiveData(const std::string &str)
    {
        std::vector<uint8_t> data(str.begin(), str.end());
        receiveDataQueue_.push(data);
    }

    // =================================================================
    // Verification Methods
    // =================================================================

    /**
     * @brief Get number of times connect() was called
     */
    int getConnectCallCount() const
    {
        return connectCallCount_;
    }

    /**
     * @brief Get number of times disconnect() was called
     */
    int getDisconnectCallCount() const
    {
        return disconnectCallCount_;
    }

    /**
     * @brief Get number of times send() was called
     */
    int getSendCallCount() const
    {
        return sendCallCount_;
    }

    /**
     * @brief Get number of times receive() was called
     */
    int getReceiveCallCount() const
    {
        return receiveCallCount_;
    }

    /**
     * @brief Get all data that was sent via send()
     */
    const std::vector<uint8_t> &getSentData() const
    {
        return sentData_;
    }

    /**
     * @brief Get the configuration passed to last connect() call
     */
    const lopcore::tls::TlsConfig &getLastConnectConfig() const
    {
        return lastConnectConfig_;
    }

    /**
     * @brief Reset all mock state (call counts, queues, etc.)
     */
    void reset()
    {
        connectCallCount_ = 0;
        disconnectCallCount_ = 0;
        sendCallCount_ = 0;
        receiveCallCount_ = 0;

        isConnected_ = false;
        connectResult_ = ESP_OK;

        sentData_.clear();

        while (!sendResults_.empty())
            sendResults_.pop();
        while (!receiveResults_.empty())
            receiveResults_.pop();
        while (!receiveDataQueue_.empty())
            receiveDataQueue_.pop();

        lastConnectConfig_ = lopcore::tls::TlsConfig{};
    }

private:
    // State
    bool isConnected_{false};
    esp_err_t connectResult_{ESP_OK};

    // Call counters
    int connectCallCount_{0};
    int disconnectCallCount_{0};
    int sendCallCount_{0};
    int receiveCallCount_{0};

    // Data tracking
    std::vector<uint8_t> sentData_;
    lopcore::tls::TlsConfig lastConnectConfig_;

    // Queued results/data
    std::queue<TransportResult> sendResults_;
    std::queue<TransportResult> receiveResults_;
    std::queue<std::vector<uint8_t>> receiveDataQueue_;
};

} // namespace test
} // namespace lopcore
