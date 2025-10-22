/**
 * @file esp_mqtt_client.cpp
 * @brief Implementation of ESP-MQTT client wrapper
 *
 * @copyright Copyright (c) 2025
 */

#include "lopcore/mqtt/esp_mqtt_client.hpp"

#include <algorithm>
#include <cstring>

#include "lopcore/logging/logger.hpp"

static const char *TAG = "esp_mqtt_client";

namespace lopcore
{
namespace mqtt
{

// =============================================================================
// Construction & Destruction
// =============================================================================

EspMqttClient::EspMqttClient(const MqttConfig &config)
    : config_(config), mqttHandle_(nullptr), state_(MqttConnectionState::DISCONNECTED), budget_(nullptr)
{
    // Validate configuration
    esp_err_t err = config_.validate();
    if (err != ESP_OK)
    {
        LOPCORE_LOGE(TAG, "Invalid MQTT configuration: %s", esp_err_to_name(err));
        // Cannot throw with -fno-exceptions
        return;
    }

    // Initialize message budgeting if enabled
    if (config_.budget.enabled)
    {
        budget_ = std::make_unique<MqttBudget>(config_.budget);
        LOPCORE_LOGI(TAG, "Message budgeting enabled");
    }

    // Configure ESP-MQTT client
    esp_mqtt_client_config_t mqttConfig = {};

    // Broker configuration
    mqttConfig.broker.address.uri = config_.broker.c_str();
    mqttConfig.broker.address.port = config_.port;

    // Client identification
    mqttConfig.credentials.client_id = config_.clientId.c_str();

    // Authentication
    if (!config_.username.empty())
    {
        mqttConfig.credentials.username = config_.username.c_str();
    }
    if (!config_.password.empty())
    {
        mqttConfig.credentials.authentication.password = config_.password.c_str();
    }

    // TLS configuration (if provided)
    if (config_.tls.has_value())
    {
        const auto &tls = config_.tls.value();

        if (!tls.caCertPath.empty())
        {
            mqttConfig.broker.verification.certificate = tls.caCertPath.c_str();
        }

        if (!tls.clientCertLabel.empty())
        {
            // PKCS#11 client certificate
            mqttConfig.credentials.authentication.certificate = tls.clientCertLabel.c_str();
            mqttConfig.credentials.authentication.use_secure_element = true;
        }

        if (!tls.clientKeyLabel.empty())
        {
            // PKCS#11 private key
            mqttConfig.credentials.authentication.key = tls.clientKeyLabel.c_str();
        }

        mqttConfig.broker.verification.skip_cert_common_name_check = !tls.verifyPeer;

        // Network timeout from TLS config
        mqttConfig.network.timeout_ms = tls.timeoutMs;

        // ALPN protocols - ESP-MQTT API limitation: only supports single protocol
        // The alpn_protos field is const char** but ESP-MQTT only uses the first entry
        // Note: We need to maintain lifetime of both the string and the pointer array
        if (!tls.alpnProtocols.empty())
        {
            alpnProtocol_ = tls.alpnProtocols[0];
            alpnProtocolPtr_[0] = alpnProtocol_.c_str();
            alpnProtocolPtr_[1] = nullptr;
            mqttConfig.broker.verification.alpn_protos = alpnProtocolPtr_;
        }
    }

    // Session configuration
    mqttConfig.session.keepalive = config_.keepAlive.count();
    mqttConfig.session.disable_clean_session = !config_.cleanSession;
    mqttConfig.buffer.size = config_.networkBufferSize;
    mqttConfig.buffer.out_size = config_.networkBufferSize;

    // Reconnection configuration
    const auto &reconnect = config_.reconnect;
    mqttConfig.network.reconnect_timeout_ms = reconnect.initialDelay.count();
    mqttConfig.network.disable_auto_reconnect = !reconnect.autoReconnect;

    // Last Will and Testament
    if (config_.will.isConfigured())
    {
        const auto &will = config_.will;
        mqttConfig.session.last_will.topic = will.topic.c_str();
        mqttConfig.session.last_will.msg = reinterpret_cast<const char *>(will.payload.data());
        mqttConfig.session.last_will.msg_len = will.payload.size();
        mqttConfig.session.last_will.qos = qosToInt(will.qos);
        mqttConfig.session.last_will.retain = will.retain ? 1 : 0;
    }

    // Create MQTT client
    mqttHandle_ = esp_mqtt_client_init(&mqttConfig);
    if (mqttHandle_ == nullptr)
    {
        LOPCORE_LOGE(TAG, "Failed to create ESP-MQTT client");
        // Cannot throw with -fno-exceptions
        return;
    }

    // Register event handler
    esp_mqtt_client_register_event(mqttHandle_, MQTT_EVENT_ANY, eventHandler, this);

    LOPCORE_LOGI(TAG, "ESP-MQTT client created: broker=%s:%d, clientId=%s", config_.broker.c_str(),
                 config_.port, config_.clientId.c_str());
}

EspMqttClient::~EspMqttClient()
{
    if (mqttHandle_ != nullptr)
    {
        disconnect();
        esp_mqtt_client_destroy(mqttHandle_);
        mqttHandle_ = nullptr;
    }

    LOPCORE_LOGI(TAG, "ESP-MQTT client destroyed");
}

// =============================================================================
// Connection Management
// =============================================================================

esp_err_t EspMqttClient::connect()
{
    std::lock_guard<std::mutex> lock(operationMutex_);

    if (state_ == MqttConnectionState::CONNECTED)
    {
        LOPCORE_LOGW(TAG, "Already connected");
        return ESP_OK;
    }

    if (state_ == MqttConnectionState::CONNECTING)
    {
        LOPCORE_LOGW(TAG, "Connection in progress");
        return ESP_ERR_INVALID_STATE;
    }

    updateConnectionState(MqttConnectionState::CONNECTING);

    esp_err_t err = esp_mqtt_client_start(mqttHandle_);
    if (err != ESP_OK)
    {
        LOPCORE_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        updateConnectionState(MqttConnectionState::ERROR);
        return err;
    }

    // Start budget restoration timer if enabled
    if (budget_ != nullptr)
    {
        budget_->start();
    }

    LOPCORE_LOGI(TAG, "MQTT connection initiated");
    return ESP_OK;
}

esp_err_t EspMqttClient::disconnect()
{
    std::lock_guard<std::mutex> lock(operationMutex_);

    if (state_ == MqttConnectionState::DISCONNECTED)
    {
        return ESP_OK;
    }

    updateConnectionState(MqttConnectionState::DISCONNECTING);

    // Stop budget timer
    if (budget_ != nullptr)
    {
        budget_->stop();
    }

    esp_err_t err = esp_mqtt_client_stop(mqttHandle_);
    if (err != ESP_OK)
    {
        LOPCORE_LOGE(TAG, "Failed to stop MQTT client: %s", esp_err_to_name(err));
        return err;
    }

    updateConnectionState(MqttConnectionState::DISCONNECTED);
    LOPCORE_LOGI(TAG, "MQTT client disconnected");

    return ESP_OK;
}

bool EspMqttClient::isConnected() const
{
    return state_.load() == MqttConnectionState::CONNECTED;
}

MqttConnectionState EspMqttClient::getConnectionState() const
{
    return state_.load();
}

// =============================================================================
// Publishing
// =============================================================================

esp_err_t EspMqttClient::publish(const std::string &topic,
                                 const std::vector<uint8_t> &payload,
                                 MqttQos qos,
                                 bool retain)
{
    if (!isConnected())
    {
        LOPCORE_LOGW(TAG, "Cannot publish: not connected");
        return ESP_ERR_INVALID_STATE;
    }

    // Check message budget
    if (budget_ != nullptr && !budget_->consume(1))
    {
        LOPCORE_LOGW(TAG, "Publish rejected: budget exhausted");
        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.publishErrors++;
        return ESP_ERR_NO_MEM; // Budget exhausted
    }

    int msgId = esp_mqtt_client_publish(mqttHandle_, topic.c_str(),
                                        reinterpret_cast<const char *>(payload.data()), payload.size(),
                                        qosToInt(qos), retain ? 1 : 0);

    if (msgId < 0)
    {
        LOPCORE_LOGE(TAG, "Failed to publish to topic '%s'", topic.c_str());

        // Restore budget on failure
        if (budget_ != nullptr)
        {
            budget_->restore(1);
        }

        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.publishErrors++;
        return ESP_FAIL;
    }

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.messagesPublished++;
    }

    LOPCORE_LOGD(TAG, "Published to '%s': %zu bytes, QoS%d, msgId=%d", topic.c_str(), payload.size(),
                 qosToInt(qos), msgId);

    return ESP_OK;
}

esp_err_t
EspMqttClient::publishString(const std::string &topic, const std::string &payload, MqttQos qos, bool retain)
{
    std::vector<uint8_t> data(payload.begin(), payload.end());
    return publish(topic, data, qos, retain);
}

// =============================================================================
// Subscription Management
// =============================================================================

esp_err_t EspMqttClient::subscribe(const std::string &topic, MessageCallback callback, MqttQos qos)
{
    if (!isConnected())
    {
        LOPCORE_LOGW(TAG, "Cannot subscribe: not connected");
        return ESP_ERR_INVALID_STATE;
    }

    // Store subscription for resubscription on reconnect
    {
        std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        subscriptions_[topic] = callback;
    }

    int msgId = esp_mqtt_client_subscribe(mqttHandle_, topic.c_str(), qosToInt(qos));
    if (msgId < 0)
    {
        LOPCORE_LOGE(TAG, "Failed to subscribe to topic '%s'", topic.c_str());
        return ESP_FAIL;
    }

    LOPCORE_LOGI(TAG, "Subscribed to '%s', QoS%d, msgId=%d", topic.c_str(), qosToInt(qos), msgId);

    return ESP_OK;
}

esp_err_t EspMqttClient::unsubscribe(const std::string &topic)
{
    if (!isConnected())
    {
        LOPCORE_LOGW(TAG, "Cannot unsubscribe: not connected");
        return ESP_ERR_INVALID_STATE;
    }

    // Remove from subscription map
    {
        std::lock_guard<std::mutex> lock(subscriptionsMutex_);
        subscriptions_.erase(topic);
    }

    int msgId = esp_mqtt_client_unsubscribe(mqttHandle_, topic.c_str());
    if (msgId < 0)
    {
        LOPCORE_LOGE(TAG, "Failed to unsubscribe from topic '%s'", topic.c_str());
        return ESP_FAIL;
    }

    LOPCORE_LOGI(TAG, "Unsubscribed from '%s', msgId=%d", topic.c_str(), msgId);

    return ESP_OK;
}

// =============================================================================
// Callbacks
// =============================================================================

void EspMqttClient::setConnectionCallback(ConnectionCallback callback)
{
    connectionCallback_ = callback;
}

void EspMqttClient::setErrorCallback(ErrorCallback callback)
{
    errorCallback_ = callback;
}

esp_err_t EspMqttClient::setWillMessage(const std::string &topic,
                                        const std::vector<uint8_t> &payload,
                                        MqttQos qos,
                                        bool retain)
{
    // Note: ESP-MQTT requires will message to be set during init
    // This is a limitation of the ESP-MQTT API
    LOPCORE_LOGW(TAG, "Will message must be set via MqttConfig before client creation");
    return ESP_ERR_NOT_SUPPORTED;
}

// =============================================================================
// Statistics
// =============================================================================

MqttStatistics EspMqttClient::getStatistics() const
{
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    return statistics_;
}

void EspMqttClient::resetStatistics()
{
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_.reset();
    LOPCORE_LOGI(TAG, "Statistics reset");
}

// =============================================================================
// Event Handling
// =============================================================================

void EspMqttClient::eventHandler(void *handlerArgs, esp_event_base_t base, int32_t eventId, void *eventData)
{
    auto *client = static_cast<EspMqttClient *>(handlerArgs);
    auto *event = static_cast<esp_mqtt_event_handle_t>(eventData);

    switch (static_cast<esp_mqtt_event_id_t>(eventId))
    {
        case MQTT_EVENT_CONNECTED:
            client->handleConnected();
            break;

        case MQTT_EVENT_DISCONNECTED:
            client->handleDisconnected();
            break;

        case MQTT_EVENT_DATA:
            client->handleData(event);
            break;

        case MQTT_EVENT_ERROR:
            client->handleError(event);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            client->handleSubscribed(event);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            client->handleUnsubscribed(event);
            break;

        default:
            // Other events (PUBLISHED, BEFORE_CONNECT, etc.) are logged but not handled
            LOPCORE_LOGD(TAG, "MQTT event: %d", eventId);
            break;
    }
}

void EspMqttClient::handleConnected()
{
    LOPCORE_LOGI(TAG, "MQTT connected");

    updateConnectionState(MqttConnectionState::CONNECTED);

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.lastConnected = std::chrono::system_clock::now();
    }

    // Resubscribe to all topics
    resubscribeAll();

    // Notify application
    if (connectionCallback_)
    {
        connectionCallback_(true);
    }
}

void EspMqttClient::handleDisconnected()
{
    LOPCORE_LOGW(TAG, "MQTT disconnected");

    MqttConnectionState oldState = state_.load();

    if (oldState == MqttConnectionState::DISCONNECTING)
    {
        updateConnectionState(MqttConnectionState::DISCONNECTED);
    }
    else
    {
        updateConnectionState(MqttConnectionState::RECONNECTING);

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(statisticsMutex_);
            statistics_.lastDisconnected = std::chrono::system_clock::now();
            statistics_.reconnectCount++;
        }
    }

    // Notify application
    if (connectionCallback_)
    {
        connectionCallback_(false);
    }
}

void EspMqttClient::handleData(esp_mqtt_event_handle_t event)
{
    // Extract topic and payload
    std::string topic(event->topic, event->topic_len);
    std::vector<uint8_t> payload(event->data, event->data + event->data_len);

    LOPCORE_LOGD(TAG, "Received message on '%s': %d bytes", topic.c_str(), event->data_len);

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(statisticsMutex_);
        statistics_.messagesReceived++;
    }

    // Create message object
    MqttMessage msg;
    msg.topic = topic;
    msg.payload = payload;
    msg.qos = intToQos(event->qos);
    msg.retained = event->retain;
    msg.messageId = event->msg_id;

    // Find matching subscriptions and invoke callbacks
    std::lock_guard<std::mutex> lock(subscriptionsMutex_);
    for (const auto &[pattern, callback] : subscriptions_)
    {
        if (topicMatches(pattern, topic))
        {
            if (callback)
            {
                callback(msg);
            }
        }
    }
}

void EspMqttClient::handleError(esp_mqtt_event_handle_t event)
{
    LOPCORE_LOGE(TAG, "MQTT error occurred");

    updateConnectionState(MqttConnectionState::ERROR);

    // Invoke error callback if registered
    if (errorCallback_)
    {
        MqttError error = convertEspError(event->error_handle->esp_transport_sock_errno);
        std::string errorMsg = "MQTT transport error";
        errorCallback_(error, errorMsg);
    }
}

void EspMqttClient::handleSubscribed(esp_mqtt_event_handle_t event)
{
    LOPCORE_LOGI(TAG, "Subscription acknowledged: msgId=%d", event->msg_id);

    // Update statistics
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    statistics_.subscriptionCount++;
}

void EspMqttClient::handleUnsubscribed(esp_mqtt_event_handle_t event)
{
    LOPCORE_LOGI(TAG, "Unsubscription acknowledged: msgId=%d", event->msg_id);

    // Update statistics
    std::lock_guard<std::mutex> lock(statisticsMutex_);
    if (statistics_.subscriptionCount > 0)
    {
        statistics_.subscriptionCount--;
    }
}

// =============================================================================
// Helper Methods
// =============================================================================

void EspMqttClient::resubscribeAll()
{
    std::lock_guard<std::mutex> lock(subscriptionsMutex_);

    if (subscriptions_.empty())
    {
        LOPCORE_LOGD(TAG, "No subscriptions to restore");
        return;
    }

    LOPCORE_LOGI(TAG, "Resubscribing to %zu topics", subscriptions_.size());

    for (const auto &[topic, callback] : subscriptions_)
    {
        int msgId = esp_mqtt_client_subscribe(mqttHandle_, topic.c_str(), qosToInt(MqttQos::AT_LEAST_ONCE));
        if (msgId < 0)
        {
            LOPCORE_LOGE(TAG, "Failed to resubscribe to '%s'", topic.c_str());
        }
        else
        {
            LOPCORE_LOGD(TAG, "Resubscribed to '%s'", topic.c_str());
        }
    }
}

bool EspMqttClient::topicMatches(const std::string &pattern, const std::string &topic) const
{
    // Implement MQTT wildcard matching
    // '+' matches exactly one level (but not empty)
    // '#' matches zero or more levels (must be at end)

    size_t patternPos = 0;
    size_t topicPos = 0;

    while (patternPos < pattern.length() && topicPos < topic.length())
    {
        if (pattern[patternPos] == '#')
        {
            // '#' must be at the end and preceded by '/' or be the only character
            if (patternPos == pattern.length() - 1 && (patternPos == 0 || pattern[patternPos - 1] == '/'))
            {
                return true; // '#' matches everything remaining
            }
            return false; // Invalid '#' placement
        }
        else if (pattern[patternPos] == '+')
        {
            // '+' matches a single level
            // Must be surrounded by '/' or at start/end
            if ((patternPos > 0 && pattern[patternPos - 1] != '/') ||
                (patternPos < pattern.length() - 1 && pattern[patternPos + 1] != '/'))
            {
                return false; // Invalid '+' placement
            }

            // Skip to the next '/' in topic
            while (topicPos < topic.length() && topic[topicPos] != '/')
            {
                topicPos++;
            }
            patternPos++;
        }
        else if (pattern[patternPos] == topic[topicPos])
        {
            // Characters match
            patternPos++;
            topicPos++;
        }
        else
        {
            // Mismatch
            return false;
        }
    }

    // Both must be fully consumed (except for trailing '#')
    if (patternPos < pattern.length())
    {
        // Check if remaining pattern is just "/#"
        return (pattern.substr(patternPos) == "/#" || pattern[patternPos] == '#');
    }

    return topicPos == topic.length();
}

MqttError EspMqttClient::convertEspError(esp_err_t espError) const
{
    switch (espError)
    {
        case ESP_OK:
            return MqttError::NONE;
        case ESP_FAIL:
            return MqttError::UNKNOWN;
        case ESP_ERR_TIMEOUT:
            return MqttError::TIMEOUT;
        case ESP_ERR_NO_MEM:
            return MqttError::BUFFER_OVERFLOW;
        case ESP_ERR_INVALID_ARG:
            return MqttError::INVALID_TOPIC;
        default:
            return MqttError::UNKNOWN;
    }
}

void EspMqttClient::updateConnectionState(MqttConnectionState newState)
{
    MqttConnectionState oldState = state_.exchange(newState);

    if (oldState != newState)
    {
        LOPCORE_LOGD(TAG, "State transition: %s -> %s", stateToString(oldState), stateToString(newState));
    }
}

} // namespace mqtt
} // namespace lopcore
