/**
 * @file mqtt_types.hpp
 * @brief Core MQTT types, enums, and data structures
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace lopcore
{
namespace mqtt
{

/**
 * @brief MQTT Quality of Service levels
 */
enum class MqttQos : uint8_t
{
    AT_MOST_ONCE = 0,  ///< QoS 0: Fire and forget
    AT_LEAST_ONCE = 1, ///< QoS 1: Acknowledged delivery
    EXACTLY_ONCE = 2   ///< QoS 2: Assured delivery (not supported by all brokers)
};

/**
 * @brief MQTT error codes
 */
enum class MqttError
{
    NONE = 0,
    CONNECTION_REFUSED,
    CONNECTION_LOST,
    TIMEOUT,
    AUTH_FAILED,
    TLS_HANDSHAKE_FAILED,
    INVALID_TOPIC,
    INVALID_PAYLOAD,
    BUFFER_OVERFLOW,
    BUDGET_EXHAUSTED,
    NOT_CONNECTED,
    ALREADY_CONNECTED,
    SUBSCRIBE_FAILED,
    UNSUBSCRIBE_FAILED,
    PUBLISH_FAILED,
    UNKNOWN
};

/**
 * @brief MQTT client implementation type
 */
enum class MqttClientType
{
    AUTO,     ///< Automatically select based on broker endpoint
    ESP_MQTT, ///< ESP-IDF native MQTT client
    AWS_IOT,  ///< AWS IoT Core MQTT (coreMQTT)
    MOCK      ///< Mock client for testing
};

/**
 * @brief MQTT message container
 */
struct MqttMessage
{
    std::string topic;            ///< Topic the message was received on
    std::vector<uint8_t> payload; ///< Message payload
    MqttQos qos;                  ///< Quality of Service level
    bool retained;                ///< Retained message flag
    uint32_t messageId;           ///< Unique message identifier (for QoS > 0)

    /**
     * @brief Get payload as string
     * @return String representation of payload
     */
    std::string getPayloadAsString() const
    {
        return std::string(payload.begin(), payload.end());
    }
};

/**
 * @brief MQTT connection state
 */
enum class MqttConnectionState
{
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    DISCONNECTING,
    ERROR
};

/**
 * @brief MQTT statistics for monitoring
 */
struct MqttStatistics
{
    uint64_t messagesPublished{0};                          ///< Total messages published
    uint64_t messagesReceived{0};                           ///< Total messages received
    uint64_t publishErrors{0};                              ///< Failed publish attempts
    uint64_t reconnectCount{0};                             ///< Number of reconnections
    uint64_t subscriptionCount{0};                          ///< Active subscriptions
    std::chrono::milliseconds averagePublishLatency{0};     ///< Average publish latency
    std::chrono::system_clock::time_point lastConnected;    ///< Last connection time
    std::chrono::system_clock::time_point lastDisconnected; ///< Last disconnection time

    /**
     * @brief Reset all statistics to zero
     */
    void reset()
    {
        messagesPublished = 0;
        messagesReceived = 0;
        publishErrors = 0;
        reconnectCount = 0;
        subscriptionCount = 0;
        averagePublishLatency = std::chrono::milliseconds(0);
    }
};

/**
 * @brief Convert QoS enum to integer
 */
inline uint8_t qosToInt(MqttQos qos)
{
    return static_cast<uint8_t>(qos);
}

/**
 * @brief Convert integer to QoS enum
 */
inline MqttQos intToQos(uint8_t value)
{
    switch (value)
    {
        case 0:
            return MqttQos::AT_MOST_ONCE;
        case 1:
            return MqttQos::AT_LEAST_ONCE;
        case 2:
            return MqttQos::EXACTLY_ONCE;
        default:
            return MqttQos::AT_MOST_ONCE;
    }
}

/**
 * @brief Convert error enum to string
 */
inline const char *errorToString(MqttError error)
{
    switch (error)
    {
        case MqttError::NONE:
            return "No error";
        case MqttError::CONNECTION_REFUSED:
            return "Connection refused";
        case MqttError::CONNECTION_LOST:
            return "Connection lost";
        case MqttError::TIMEOUT:
            return "Timeout";
        case MqttError::AUTH_FAILED:
            return "Authentication failed";
        case MqttError::TLS_HANDSHAKE_FAILED:
            return "TLS handshake failed";
        case MqttError::INVALID_TOPIC:
            return "Invalid topic";
        case MqttError::INVALID_PAYLOAD:
            return "Invalid payload";
        case MqttError::BUFFER_OVERFLOW:
            return "Buffer overflow";
        case MqttError::BUDGET_EXHAUSTED:
            return "Message budget exhausted";
        case MqttError::NOT_CONNECTED:
            return "Not connected";
        case MqttError::ALREADY_CONNECTED:
            return "Already connected";
        case MqttError::SUBSCRIBE_FAILED:
            return "Subscribe failed";
        case MqttError::UNSUBSCRIBE_FAILED:
            return "Unsubscribe failed";
        case MqttError::PUBLISH_FAILED:
            return "Publish failed";
        case MqttError::UNKNOWN:
            return "Unknown error";
        default:
            return "Undefined error";
    }
}

/**
 * @brief Convert connection state to string
 */
inline const char *stateToString(MqttConnectionState state)
{
    switch (state)
    {
        case MqttConnectionState::DISCONNECTED:
            return "Disconnected";
        case MqttConnectionState::CONNECTING:
            return "Connecting";
        case MqttConnectionState::CONNECTED:
            return "Connected";
        case MqttConnectionState::RECONNECTING:
            return "Reconnecting";
        case MqttConnectionState::DISCONNECTING:
            return "Disconnecting";
        case MqttConnectionState::ERROR:
            return "Error";
        default:
            return "Unknown";
    }
}

/**
 * @brief Convert client type to string
 */
inline const char *clientTypeToString(MqttClientType type)
{
    switch (type)
    {
        case MqttClientType::ESP_MQTT:
            return "ESP-MQTT";
        case MqttClientType::AWS_IOT:
            return "AWS-IOT";
        case MqttClientType::MOCK:
            return "MOCK";
        default:
            return "Unknown";
    }
}

} // namespace mqtt
} // namespace lopcore
