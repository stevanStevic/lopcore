/**
 * @file imqtt_client.hpp
 * @brief MQTT Client interface abstraction
 *
 * Provides a platform-independent MQTT client interface that can be implemented
 * by different MQTT client libraries (ESP-MQTT, AWS IoT Core, etc.)
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <esp_err.h>

#include "mqtt_types.hpp"

namespace lopcore
{
namespace mqtt
{

/**
 * @brief Abstract MQTT client interface
 *
 * Defines the contract for MQTT client implementations. Supports:
 * - Publish/Subscribe/Unsubscribe operations
 * - TLS/mTLS connections
 * - Auto-reconnection
 * - Message budgeting
 * - Connection callbacks
 * - Error handling
 *
 * Thread-safe: All methods can be called from multiple threads
 */
class IMqttClient
{
public:
    /**
     * @brief Callback for incoming MQTT messages
     * @param message The received message
     */
    using MessageCallback = std::function<void(const MqttMessage &message)>;

    /**
     * @brief Callback for connection state changes
     * @param connected True if connected, false if disconnected
     */
    using ConnectionCallback = std::function<void(bool connected)>;

    /**
     * @brief Callback for error events
     * @param error The error code
     * @param message Human-readable error description
     */
    using ErrorCallback = std::function<void(MqttError error, const std::string &message)>;

    virtual ~IMqttClient() = default;

    // =============================================================================
    // Connection Management
    // =============================================================================

    /**
     * @brief Connect to MQTT broker
     *
     * Initiates connection to the configured broker. If auto-reconnect is enabled,
     * this will keep retrying on failure.
     *
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_STATE if already connected
     * @return ESP_FAIL on connection failure
     */
    virtual esp_err_t connect() = 0;

    /**
     * @brief Disconnect from MQTT broker
     *
     * Gracefully disconnects from broker. Sends DISCONNECT packet before closing.
     *
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_STATE if not connected
     */
    virtual esp_err_t disconnect() = 0;

    /**
     * @brief Check if client is connected to broker
     * @return True if connected, false otherwise
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Get current connection state
     * @return Connection state enum
     */
    virtual MqttConnectionState getConnectionState() const = 0;

    // =============================================================================
    // Publish/Subscribe Operations
    // =============================================================================

    /**
     * @brief Publish message to topic
     *
     * @param topic Topic to publish to (UTF-8 string, no wildcards)
     * @param payload Message payload (binary data)
     * @param qos Quality of Service level (0, 1, or 2)
     * @param retain Retain flag (broker stores last message)
     *
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_ARG if topic or payload invalid
     * @return ESP_ERR_INVALID_STATE if not connected
     * @return ESP_ERR_NO_MEM if budget exhausted
     * @return ESP_FAIL on publish failure
     *
     * @note For QoS > 0, message is queued for retry on failure
     */
    virtual esp_err_t publish(const std::string &topic,
                              const std::vector<uint8_t> &payload,
                              MqttQos qos = MqttQos::AT_MOST_ONCE,
                              bool retain = false) = 0;

    /**
     * @brief Publish string message to topic
     *
     * Convenience method for publishing string payloads
     *
     * @param topic Topic to publish to
     * @param payload String payload (converted to UTF-8 bytes)
     * @param qos Quality of Service level
     * @param retain Retain flag
     * @return ESP_OK on success, error code otherwise
     */
    virtual esp_err_t publishString(const std::string &topic,
                                    const std::string &payload,
                                    MqttQos qos = MqttQos::AT_MOST_ONCE,
                                    bool retain = false) = 0;

    /**
     * @brief Subscribe to topic with callback
     *
     * Subscribes to topic and registers callback for incoming messages.
     * Supports MQTT wildcards: + (single level), # (multi-level)
     *
     * @param topic Topic filter to subscribe to (can include wildcards)
     * @param callback Function called when message received
     * @param qos Maximum QoS level for subscription
     *
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_ARG if topic invalid
     * @return ESP_ERR_INVALID_STATE if not connected
     * @return ESP_FAIL on subscribe failure
     *
     * @note Callback is invoked from MQTT thread, keep processing brief
     */
    virtual esp_err_t subscribe(const std::string &topic,
                                MessageCallback callback,
                                MqttQos qos = MqttQos::AT_MOST_ONCE) = 0;

    /**
     * @brief Unsubscribe from topic
     *
     * @param topic Topic filter to unsubscribe from
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_ARG if topic invalid
     * @return ESP_ERR_INVALID_STATE if not connected or not subscribed
     * @return ESP_FAIL on unsubscribe failure
     */
    virtual esp_err_t unsubscribe(const std::string &topic) = 0;

    // =============================================================================
    // Event Handlers
    // =============================================================================

    /**
     * @brief Set connection state callback
     *
     * Called when connection state changes (connected/disconnected)
     *
     * @param callback Function to call on state change
     */
    virtual void setConnectionCallback(ConnectionCallback callback) = 0;

    /**
     * @brief Set error callback
     *
     * Called when errors occur (connection failures, publish errors, etc.)
     *
     * @param callback Function to call on error
     */
    virtual void setErrorCallback(ErrorCallback callback) = 0;

    // =============================================================================
    // Configuration & Status
    // =============================================================================

    /**
     * @brief Set Last Will and Testament message
     *
     * Broker will publish this message if client disconnects ungracefully
     * Must be called before connect()
     *
     * @param topic Topic for will message
     * @param payload Will message payload
     * @param qos QoS for will message
     * @param retain Retain flag for will message
     *
     * @return ESP_OK on success
     * @return ESP_ERR_INVALID_STATE if already connected
     * @return ESP_ERR_INVALID_ARG if parameters invalid
     */
    virtual esp_err_t setWillMessage(const std::string &topic,
                                     const std::vector<uint8_t> &payload,
                                     MqttQos qos = MqttQos::AT_MOST_ONCE,
                                     bool retain = false) = 0;

    /**
     * @brief Get MQTT client statistics
     *
     * Returns counters and metrics for monitoring
     *
     * @return Statistics structure with current values
     */
    virtual MqttStatistics getStatistics() const = 0;

    /**
     * @brief Reset statistics counters to zero
     */
    virtual void resetStatistics() = 0;

    /**
     * @brief Get client ID
     * @return Configured MQTT client ID
     */
    virtual std::string getClientId() const = 0;

    /**
     * @brief Get broker hostname/IP
     * @return Configured broker address
     */
    virtual std::string getBroker() const = 0;

    /**
     * @brief Get broker port
     * @return Configured broker port
     */
    virtual uint16_t getPort() const = 0;

protected:
    // Protected constructor - interface cannot be instantiated directly
    IMqttClient() = default;

    // Non-copyable
    IMqttClient(const IMqttClient &) = delete;
    IMqttClient &operator=(const IMqttClient &) = delete;
};

} // namespace mqtt
} // namespace lopcore
