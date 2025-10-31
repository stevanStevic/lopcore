/**
 * @file esp_mqtt_client.hpp
 * @brief Standalone ESP-IDF MQTT client wrapper
 *
 * Wraps ESP-MQTT library as a standalone component for type safety,
 * testability, and modern C++ patterns.
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <map>
#include <memory>
#include <mutex>

// For host testing, include mock ESP-IDF types before mqtt_client.h
#ifndef ESP_PLATFORM
#include <esp_event.h>
#endif

#include <mqtt_client.h>

#include "mqtt_budget.hpp"
#include "mqtt_config.hpp"
#include "mqtt_types.hpp"

namespace lopcore
{
namespace mqtt
{

/**
 * @brief Standalone ESP-MQTT client
 *
 * Wraps ESP-IDF's mqtt_client component as a standalone component.
 *
 * Features:
 * - Automatic reconnection with exponential backoff
 * - Message budgeting for flood prevention
 * - TLS/SSL with PKCS#11 support
 * - Subscription persistence across reconnects
 * - Thread-safe operation
 * - Async-only (no manual processing)
 *
 * @note Thread-safe: All public methods can be called from multiple tasks
 * @note Does NOT support manual processing (processLoop) - async only
 */
class EspMqttClient
{
public:
    /**
     * @brief Construct ESP-MQTT client
     * @param config MQTT configuration
     */
    explicit EspMqttClient(const MqttConfig &config);

    /**
     * @brief Destructor - cleans up resources
     */
    ~EspMqttClient();

    // Non-copyable, non-movable
    EspMqttClient(const EspMqttClient &) = delete;
    EspMqttClient &operator=(const EspMqttClient &) = delete;
    EspMqttClient(EspMqttClient &&) = delete;
    EspMqttClient &operator=(EspMqttClient &&) = delete;

    // ========================================================================
    // Core MQTT Operations
    // ========================================================================

    esp_err_t connect();
    esp_err_t disconnect();
    bool isConnected() const;
    MqttConnectionState getConnectionState() const;

    esp_err_t
    publish(const std::string &topic, const std::vector<uint8_t> &payload, MqttQos qos, bool retain);

    esp_err_t
    publishString(const std::string &topic, const std::string &payload, MqttQos qos, bool retain);

    esp_err_t subscribe(const std::string &topic, MessageCallback callback, MqttQos qos);

    esp_err_t unsubscribe(const std::string &topic);

    void setConnectionCallback(ConnectionCallback callback);
    void setErrorCallback(ErrorCallback callback);

    esp_err_t setWillMessage(const std::string &topic,
                             const std::vector<uint8_t> &payload,
                             MqttQos qos,
                             bool retain);

    MqttStatistics getStatistics() const;
    void resetStatistics();

    // ========================================================================
    // Accessors (no virtual - plain getters)
    // ========================================================================

    std::string getClientId() const
    {
        return config_.clientId;
    }
    std::string getBroker() const
    {
        return config_.broker;
    }
    uint16_t getPort() const
    {
        return config_.port;
    }

private:
    // ========================================================================
    // ESP-MQTT Event Handling
    // ========================================================================

    /**
     * @brief ESP-MQTT event handler (static callback)
     * @param handlerArgs User context (EspMqttClient instance)
     * @param base Event base
     * @param eventId Event ID
     * @param eventData Event data
     */
    static void eventHandler(void *handlerArgs, esp_event_base_t base, int32_t eventId, void *eventData);

    /**
     * @brief Handle MQTT_EVENT_CONNECTED
     */
    void handleConnected();

    /**
     * @brief Handle MQTT_EVENT_DISCONNECTED
     */
    void handleDisconnected();

    /**
     * @brief Handle MQTT_EVENT_DATA
     * @param event MQTT event with message data
     */
    void handleData(esp_mqtt_event_handle_t event);

    /**
     * @brief Handle MQTT_EVENT_ERROR
     * @param event MQTT event with error info
     */
    void handleError(esp_mqtt_event_handle_t event);

    /**
     * @brief Handle MQTT_EVENT_SUBSCRIBED
     * @param event MQTT event with subscription ack
     */
    void handleSubscribed(esp_mqtt_event_handle_t event);

    /**
     * @brief Handle MQTT_EVENT_UNSUBSCRIBED
     * @param event MQTT event with unsubscription ack
     */
    void handleUnsubscribed(esp_mqtt_event_handle_t event);

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /**
     * @brief Resubscribe to all topics after reconnection
     */
    void resubscribeAll();

    /**
     * @brief Check if topic matches subscription pattern
     * @param pattern Subscription pattern (may contain wildcards)
     * @param topic Incoming message topic
     * @return True if topic matches pattern
     */
    bool topicMatches(const std::string &pattern, const std::string &topic) const;

    /**
     * @brief Convert ESP-MQTT error to MqttError
     * @param espError ESP error code
     * @return MqttError enum
     */
    MqttError convertEspError(esp_err_t espError) const;

    /**
     * @brief Update connection state and notify callback
     * @param newState New connection state
     */
    void updateConnectionState(MqttConnectionState newState);

    // ========================================================================
    // Member Variables
    // ========================================================================

    const MqttConfig config_;                              ///< Configuration
    esp_mqtt_client_handle_t mqttHandle_;                  ///< ESP-MQTT client handle
    std::atomic<MqttConnectionState> state_;               ///< Current connection state
    std::unique_ptr<MqttBudget> budget_;                   ///< Message budget (optional)
    mutable MqttStatistics statistics_;                    ///< Connection statistics
    mutable std::mutex statisticsMutex_;                   ///< Protects statistics_
    ConnectionCallback connectionCallback_;                ///< Connection state callback
    ErrorCallback errorCallback_;                          ///< Error callback
    std::map<std::string, MessageCallback> subscriptions_; ///< Topic -> callback map
    mutable std::mutex subscriptionsMutex_;                ///< Protects subscriptions_
    mutable std::mutex operationMutex_;                    ///< Protects connect/disconnect operations
    std::string alpnProtocol_;                             ///< ALPN protocol string (lifetime management)
    const char *alpnProtocolPtr_[2] = {nullptr, nullptr};  ///< Null-terminated array for ESP-MQTT API
};

} // namespace mqtt
} // namespace lopcore
