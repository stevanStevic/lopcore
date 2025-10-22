/**
 * @file coremqtt_client.cpp
 * @brief Implementation of coreMQTT client wrapper
 *
 * @copyright Copyright (c) 2025
 */

#include "lopcore/mqtt/coremqtt_client.hpp"

#include <algorithm>
#include <cstring>

#include "lopcore/logging/logger.hpp"
#include "lopcore/tls/mbedtls_transport.hpp"
#include "lopcore/tls/tls_config.hpp"

static const char *TAG = "coremqtt_client";

namespace lopcore
{
namespace mqtt
{

// =============================================================================
// Construction & Destruction
// =============================================================================

CoreMqttClient::CoreMqttClient(const MqttConfig &config,
                               std::shared_ptr<lopcore::tls::ITlsTransport> transport)
    : config_(config), mqttContext_{}, transport_{}, networkContext_{}, tlsTransport_(transport),
      budget_(nullptr), state_(MqttConnectionState::DISCONNECTED), processTask_(nullptr), shouldRun_(false)
{
    // Validate configuration
    esp_err_t err = config_.validate();
    if (err != ESP_OK)
    {
        LOPCORE_LOGE(TAG, "Invalid MQTT configuration: %s", esp_err_to_name(err));
        return;
    }

    // Validate transport is connected
    if (!tlsTransport_ || !tlsTransport_->isConnected())
    {
        LOPCORE_LOGE(TAG, "TLS transport must be connected before creating MQTT client");
        return;
    }

    // Get network context from transport
    NetworkContext_t *netContext = static_cast<NetworkContext_t *>(tlsTransport_->getNetworkContext());
    if (netContext == nullptr)
    {
        LOPCORE_LOGE(TAG, "Failed to get network context from TLS transport");
        return;
    }

    // Copy network context (the transport owns the actual context)
    networkContext_ = *netContext;

    // Set client back-reference for transport callbacks
    networkContext_.client = this;

    // Initialize message budgeting if enabled
    if (config_.budget.enabled)
    {
        budget_ = std::make_unique<MqttBudget>(config_.budget);
        LOPCORE_LOGI(TAG, "Message budgeting enabled");
    }

    // Allocate network buffer
    networkBuffer_.resize(config_.networkBufferSize);

    // Allocate QoS record arrays
    outgoingPublishRecords_.resize(16); // MQTT_STATE_ARRAY_MAX_COUNT
    incomingPublishRecords_.resize(16);

    // Setup transport interface
    transport_.send = transportSend;
    transport_.recv = transportRecv;
    transport_.pNetworkContext = &networkContext_;

    // Setup coreMQTT context
    MQTTFixedBuffer_t fixedBuffer = {.pBuffer = networkBuffer_.data(), .size = networkBuffer_.size()};

    MQTTStatus_t mqttStatus = MQTT_Init(&mqttContext_, &transport_, getTimeMs, eventCallback, &fixedBuffer);

    if (mqttStatus != MQTTSuccess)
    {
        LOPCORE_LOGE(TAG, "MQTT_Init failed: %d", mqttStatus);
        return;
    }

    // Initialize stateful QoS
    mqttStatus = MQTT_InitStatefulQoS(&mqttContext_, outgoingPublishRecords_.data(),
                                      outgoingPublishRecords_.size(), incomingPublishRecords_.data(),
                                      incomingPublishRecords_.size());

    if (mqttStatus != MQTTSuccess)
    {
        LOPCORE_LOGE(TAG, "MQTT_InitStatefulQoS failed: %d", mqttStatus);
        return;
    }

    LOPCORE_LOGI(TAG, "CoreMQTT client created: broker=%s:%d, clientId=%s", config_.broker.c_str(),
                 config_.port, config_.clientId.c_str());
}

CoreMqttClient::~CoreMqttClient()
{
    disconnect();

    // TLS transport cleanup happens automatically via unique_ptr destructor

    LOPCORE_LOGI(TAG, "CoreMQTT client destroyed");
}

// =============================================================================
// Connection Management
// =============================================================================

esp_err_t CoreMqttClient::connect()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ == MqttConnectionState::CONNECTED)
    {
        LOPCORE_LOGW(TAG, "Already connected");
        return ESP_OK;
    }

    // Verify TLS transport is connected
    if (!tlsTransport_ || !tlsTransport_->isConnected())
    {
        LOPCORE_LOGE(TAG, "TLS transport is not connected");
        return ESP_ERR_INVALID_STATE;
    }

    LOPCORE_LOGI(TAG, "Establishing MQTT connection over TLS");

    // Build CONNECT packet
    MQTTConnectInfo_t connectInfo = {};
    connectInfo.cleanSession = config_.cleanSession;
    connectInfo.pClientIdentifier = config_.clientId.c_str();
    connectInfo.clientIdentifierLength = config_.clientId.length();
    connectInfo.keepAliveSeconds = config_.keepAlive.count();

    if (!config_.username.empty())
    {
        connectInfo.pUserName = config_.username.c_str();
        connectInfo.userNameLength = config_.username.length();
    }

    if (!config_.password.empty())
    {
        connectInfo.pPassword = config_.password.c_str();
        connectInfo.passwordLength = config_.password.length();
    }

    // Last Will and Testament
    MQTTPublishInfo_t willInfo = {};
    if (config_.will.isConfigured())
    {
        const auto &will = config_.will;
        willInfo.pTopicName = will.topic.c_str();
        willInfo.topicNameLength = will.topic.length();
        willInfo.pPayload = will.payload.data();
        willInfo.payloadLength = will.payload.size();
        willInfo.qos = static_cast<MQTTQoS_t>(qosToInt(will.qos));
        willInfo.retain = will.retain;
    }

    // Get timeout from TLS config if available, otherwise use default
    uint32_t connectTimeoutMs = 30000; // Default 30 seconds
    if (config_.tls.has_value())
    {
        connectTimeoutMs = config_.tls->timeoutMs;
    }

    // Send CONNECT
    bool sessionPresent = false;
    MQTTStatus_t mqttStatus = MQTT_Connect(&mqttContext_, &connectInfo,
                                           config_.will.isConfigured() ? &willInfo : nullptr,
                                           connectTimeoutMs, &sessionPresent);

    if (mqttStatus != MQTTSuccess)
    {
        LOPCORE_LOGE(TAG, "MQTT_Connect failed: %d", mqttStatus);
        // TLS remains connected for potential retry
        return ESP_FAIL;
    }

    state_ = MqttConnectionState::CONNECTED;
    statistics_.reconnectCount++;
    statistics_.lastConnected = std::chrono::system_clock::now();

    LOPCORE_LOGI(TAG, "Connected to %s:%d (session=%s)", config_.broker.c_str(), config_.port,
                 sessionPresent ? "resumed" : "new");

    // Auto-start ProcessLoop task if enabled in config
    if (config_.autoStartProcessLoop)
    {
        esp_err_t err = startProcessLoopTask();
        if (err != ESP_OK)
        {
            LOPCORE_LOGE(TAG, "Failed to start ProcessLoop task");
            state_ = MqttConnectionState::ERROR;
            return err;
        }
    }
    else
    {
        LOPCORE_LOGI(TAG, "ProcessLoop auto-start disabled - call startProcessLoopTask() manually");
    }

    // Notify application
    if (connectionCallback_)
    {
        connectionCallback_(true);
    }

    // Resubscribe if needed
    if (!sessionPresent)
    {
        resubscribeTopics();
    }
    else
    {
        // Resend unacknowledged publishes
        resendPendingPublishes();
    }

    return ESP_OK;
}

esp_err_t CoreMqttClient::disconnect()
{
    // Stop ProcessLoop task first (if running)
    stopProcessLoopTask();

    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ == MqttConnectionState::DISCONNECTED)
    {
        return ESP_OK;
    }

    // Send DISCONNECT
    MQTTStatus_t mqttStatus = MQTT_Disconnect(&mqttContext_);
    if (mqttStatus != MQTTSuccess)
    {
        LOPCORE_LOGW(TAG, "MQTT_Disconnect failed: %d", mqttStatus);
    }

    state_ = MqttConnectionState::DISCONNECTED;
    statistics_.reconnectCount++;
    statistics_.lastDisconnected = std::chrono::system_clock::now();

    // Disconnect TLS transport
    if (tlsTransport_)
    {
        tlsTransport_->disconnect();
    }

    LOPCORE_LOGI(TAG, "Disconnected");

    // Notify application
    if (connectionCallback_)
    {
        connectionCallback_(false);
    }

    return ESP_OK;
}

bool CoreMqttClient::isConnected() const
{
    return state_ == MqttConnectionState::CONNECTED;
}

// =============================================================================
// Publish/Subscribe
// =============================================================================

esp_err_t CoreMqttClient::publish(const std::string &topic,
                                  const std::vector<uint8_t> &payload,
                                  MqttQos qos,
                                  bool retain)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != MqttConnectionState::CONNECTED)
    {
        LOPCORE_LOGE(TAG, "Cannot publish: not connected");
        return ESP_ERR_INVALID_STATE;
    }

    // Check budget
    if (budget_ && !budget_->consume())
    {
        LOPCORE_LOGW(TAG, "Publish rejected: budget exceeded");
        statistics_.publishErrors++; // Track as publish error
        return ESP_ERR_NO_MEM;
    }

    // Build PUBLISH packet
    MQTTPublishInfo_t publishInfo = {};
    publishInfo.qos = static_cast<MQTTQoS_t>(qosToInt(qos));
    publishInfo.retain = retain;
    publishInfo.pTopicName = topic.c_str();
    publishInfo.topicNameLength = topic.length();
    publishInfo.pPayload = payload.data();
    publishInfo.payloadLength = payload.size();

    // Generate packet ID for QoS > 0
    uint16_t packetId = MQTT_PACKET_ID_INVALID;
    if (qos != MqttQos::AT_MOST_ONCE)
    {
        packetId = MQTT_GetPacketId(&mqttContext_);
    }

    // Send PUBLISH
    MQTTStatus_t mqttStatus = MQTT_Publish(&mqttContext_, &publishInfo, packetId);

    if (mqttStatus != MQTTSuccess)
    {
        LOPCORE_LOGE(TAG, "MQTT_Publish failed: %d", mqttStatus);
        return ESP_FAIL;
    }

    statistics_.messagesPublished++;
    // Note: bytesPublished not in MqttStatistics struct

    LOPCORE_LOGD(TAG, "Published to '%s' (qos=%d, size=%zu, packetId=%u)", topic.c_str(), qosToInt(qos),
                 payload.size(), packetId);

    return ESP_OK;
}

esp_err_t
CoreMqttClient::publishString(const std::string &topic, const std::string &payload, MqttQos qos, bool retain)
{
    // Convert string to byte vector
    std::vector<uint8_t> data(payload.begin(), payload.end());
    return publish(topic, data, qos, retain);
}

esp_err_t CoreMqttClient::subscribe(const std::string &topic, MessageCallback callback, MqttQos qos)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != MqttConnectionState::CONNECTED)
    {
        LOPCORE_LOGE(TAG, "Cannot subscribe: not connected");
        return ESP_ERR_INVALID_STATE;
    }

    // Check if already subscribed
    if (findSubscription(topic) != nullptr)
    {
        LOPCORE_LOGW(TAG, "Already subscribed to '%s'", topic.c_str());
        return ESP_OK;
    }

    // Build SUBSCRIBE packet
    MQTTSubscribeInfo_t subscribeInfo = {};
    subscribeInfo.pTopicFilter = topic.c_str();
    subscribeInfo.topicFilterLength = topic.length();
    subscribeInfo.qos = static_cast<MQTTQoS_t>(qosToInt(qos));

    uint16_t packetId = MQTT_GetPacketId(&mqttContext_);

    MQTTStatus_t mqttStatus = MQTT_Subscribe(&mqttContext_, &subscribeInfo, 1, packetId);

    if (mqttStatus != MQTTSuccess)
    {
        LOPCORE_LOGE(TAG, "MQTT_Subscribe failed: %d", mqttStatus);
        return ESP_FAIL;
    }

    // Add to subscription list
    subscriptions_.push_back({topic, callback, qos});
    statistics_.subscriptionCount = subscriptions_.size();

    LOPCORE_LOGI(TAG, "Subscribed to '%s' (qos=%d)", topic.c_str(), qosToInt(qos));

    return ESP_OK;
}

esp_err_t CoreMqttClient::unsubscribe(const std::string &topic)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != MqttConnectionState::CONNECTED)
    {
        LOPCORE_LOGE(TAG, "Cannot unsubscribe: not connected");
        return ESP_ERR_INVALID_STATE;
    }

    // Build UNSUBSCRIBE packet
    MQTTSubscribeInfo_t unsubscribeInfo = {};
    unsubscribeInfo.pTopicFilter = topic.c_str();
    unsubscribeInfo.topicFilterLength = topic.length();

    uint16_t packetId = MQTT_GetPacketId(&mqttContext_);

    MQTTStatus_t mqttStatus = MQTT_Unsubscribe(&mqttContext_, &unsubscribeInfo, 1, packetId);

    if (mqttStatus != MQTTSuccess)
    {
        LOPCORE_LOGE(TAG, "MQTT_Unsubscribe failed: %d", mqttStatus);
        return ESP_FAIL;
    }

    // Remove from subscription list
    auto it = std::remove_if(subscriptions_.begin(), subscriptions_.end(),
                             [&topic](const Subscription &sub) { return sub.topic == topic; });

    subscriptions_.erase(it, subscriptions_.end());
    statistics_.subscriptionCount = subscriptions_.size();

    LOPCORE_LOGI(TAG, "Unsubscribed from '%s'", topic.c_str());

    return ESP_OK;
}

esp_err_t CoreMqttClient::setWillMessage(const std::string &topic,
                                         const std::vector<uint8_t> &payload,
                                         MqttQos qos,
                                         bool retain)
{
    // Will message must be set before connect() is called
    // This is a configuration issue - will should be in MqttConfig
    LOPCORE_LOGW(TAG, "setWillMessage() called but coreMQTT requires will in CONNECT packet");
    LOPCORE_LOGW(TAG, "Please set will message in MqttConfig before creating client");
    return ESP_ERR_NOT_SUPPORTED;
}

// =============================================================================
// Callbacks
// =============================================================================

void CoreMqttClient::setConnectionCallback(ConnectionCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    connectionCallback_ = callback;
}

void CoreMqttClient::setErrorCallback(ErrorCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    errorCallback_ = callback;
}

// =============================================================================
// Statistics
// =============================================================================

MqttStatistics CoreMqttClient::getStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return statistics_;
}

void CoreMqttClient::resetStatistics()
{
    std::lock_guard<std::mutex> lock(mutex_);
    statistics_ = MqttStatistics{};
}

// =============================================================================
// CoreMQTT-Specific Methods
// =============================================================================

esp_err_t CoreMqttClient::processLoop(uint32_t timeoutMs)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != MqttConnectionState::CONNECTED)
    {
        return ESP_ERR_INVALID_STATE;
    }

    MQTTStatus_t mqttStatus = MQTT_ProcessLoop(&mqttContext_);

    if (mqttStatus != MQTTSuccess && mqttStatus != MQTTNeedMoreBytes)
    {
        LOPCORE_LOGE(TAG, "MQTT_ProcessLoop failed: %d", mqttStatus);

        // Connection lost - trigger disconnect
        state_ = MqttConnectionState::DISCONNECTED;

        // Disconnect TLS transport
        if (tlsTransport_)
        {
            tlsTransport_->disconnect();
        }

        if (connectionCallback_)
        {
            connectionCallback_(false);
        }

        return ESP_FAIL;
    }

    return ESP_OK;
}

MQTTPublishState_t CoreMqttClient::getPublishState(uint16_t packetId) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto &record : outgoingPublishRecords_)
    {
        if (record.packetId == packetId)
        {
            return record.publishState;
        }
    }

    return MQTTStateNull;
}

bool CoreMqttClient::hasOutstandingPackets() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto &record : outgoingPublishRecords_)
    {
        if (record.packetId != MQTT_PACKET_ID_INVALID && record.publishState != MQTTPublishDone &&
            record.publishState != MQTTStateNull)
        {
            return true;
        }
    }

    return false;
}

// =============================================================================
// Transport Layer (TLS with PKCS#11)
// =============================================================================

int32_t CoreMqttClient::transportSend(NetworkContext_t *pNetworkContext,
                                      const void *pBuffer,
                                      size_t bytesToSend)
{
    if (pNetworkContext == nullptr || pBuffer == nullptr)
    {
        LOPCORE_LOGE(TAG, "Invalid transport send parameters");
        return -1;
    }

    // Get client from NetworkContext back-reference
    CoreMqttClient *client = pNetworkContext->client;
    if (client == nullptr || client->tlsTransport_ == nullptr)
    {
        LOPCORE_LOGE(TAG, "Invalid client or transport");
        return -1;
    }

    // Send via TLS transport
    size_t bytesSent = 0;
    esp_err_t err = client->tlsTransport_->send(pBuffer, bytesToSend, &bytesSent);

    if (err == ESP_OK)
    {
        return static_cast<int32_t>(bytesSent);
    }
    else
    {
        LOPCORE_LOGE(TAG, "TLS send failed: %s", esp_err_to_name(err));
        return -1;
    }
}

int32_t CoreMqttClient::transportRecv(NetworkContext_t *pNetworkContext, void *pBuffer, size_t bytesToRecv)
{
    if (pNetworkContext == nullptr || pBuffer == nullptr)
    {
        LOPCORE_LOGE(TAG, "Invalid transport receive parameters");
        return -1;
    }

    // Get client from NetworkContext back-reference
    CoreMqttClient *client = pNetworkContext->client;
    if (client == nullptr || client->tlsTransport_ == nullptr)
    {
        LOPCORE_LOGE(TAG, "Invalid client or transport");
        return -1;
    }

    // Receive via TLS transport
    size_t bytesReceived = 0;
    esp_err_t err = client->tlsTransport_->recv(pBuffer, bytesToRecv, &bytesReceived);

    if (err == ESP_OK)
    {
        return static_cast<int32_t>(bytesReceived);
    }
    else if (err == ESP_ERR_TIMEOUT)
    {
        // Timeout is not an error, return 0 (retry)
        return 0;
    }
    else
    {
        LOPCORE_LOGE(TAG, "TLS recv failed: %s", esp_err_to_name(err));
        return -1;
    }
}

// =============================================================================
// Event Handling
// =============================================================================

void CoreMqttClient::eventCallback(MQTTContext_t *pMqttContext,
                                   MQTTPacketInfo_t *pPacketInfo,
                                   MQTTDeserializedInfo_t *pDeserializedInfo)
{
    if (pMqttContext == nullptr || pMqttContext->transportInterface.pNetworkContext == nullptr)
    {
        LOPCORE_LOGE(TAG, "Invalid MQTT context in event callback");
        return;
    }

    // Get client from NetworkContext back-reference
    NetworkContext_t *pNetworkContext = static_cast<NetworkContext_t *>(
        pMqttContext->transportInterface.pNetworkContext);

    CoreMqttClient *client = pNetworkContext->client;
    if (client)
    {
        client->handleEvent(pPacketInfo, pDeserializedInfo);
    }
}

void CoreMqttClient::handleEvent(MQTTPacketInfo_t *pPacketInfo, MQTTDeserializedInfo_t *pDeserializedInfo)
{
    switch (pPacketInfo->type)
    {
        case MQTT_PACKET_TYPE_PUBLISH: {
            // Incoming message
            MQTTPublishInfo_t *pubInfo = pDeserializedInfo->pPublishInfo;

            std::string topic(pubInfo->pTopicName, pubInfo->topicNameLength);
            std::vector<uint8_t> payload(static_cast<const uint8_t *>(pubInfo->pPayload),
                                         static_cast<const uint8_t *>(pubInfo->pPayload) +
                                             pubInfo->payloadLength);

            statistics_.messagesReceived++;
            // Note: bytesReceived not in MqttStatistics struct

            LOPCORE_LOGD(TAG, "Received message on '%s' (size=%zu)", topic.c_str(), payload.size());

            // Call topic-specific callback
            Subscription *sub = findSubscription(topic);
            if (sub && sub->callback)
            {
                MqttMessage msg{topic, payload, static_cast<MqttQos>(pubInfo->qos), pubInfo->retain};
                sub->callback(msg);
            }
            break;
        }

        case MQTT_PACKET_TYPE_SUBACK:
            LOPCORE_LOGD(TAG, "SUBACK received");
            break;

        case MQTT_PACKET_TYPE_UNSUBACK:
            LOPCORE_LOGD(TAG, "UNSUBACK received");
            break;

        case MQTT_PACKET_TYPE_PUBACK:
            LOPCORE_LOGD(TAG, "PUBACK received (packetId=%u)", pDeserializedInfo->packetIdentifier);
            break;

        case MQTT_PACKET_TYPE_PUBREC:
            LOPCORE_LOGD(TAG, "PUBREC received (packetId=%u)", pDeserializedInfo->packetIdentifier);
            break;

        case MQTT_PACKET_TYPE_PUBREL:
            LOPCORE_LOGD(TAG, "PUBREL received (packetId=%u)", pDeserializedInfo->packetIdentifier);
            break;

        case MQTT_PACKET_TYPE_PUBCOMP:
            LOPCORE_LOGD(TAG, "PUBCOMP received (packetId=%u)", pDeserializedInfo->packetIdentifier);
            break;

        case MQTT_PACKET_TYPE_PINGRESP:
            LOPCORE_LOGD(TAG, "PINGRESP received");
            break;

        default:
            LOPCORE_LOGW(TAG, "Unknown packet type: %d", pPacketInfo->type);
            break;
    }
}

uint32_t CoreMqttClient::getTimeMs()
{
    return esp_timer_get_time() / 1000;
}

// =============================================================================
// State Management
// =============================================================================

esp_err_t CoreMqttClient::resendPendingPublishes()
{
    LOPCORE_LOGI(TAG, "Resending pending publishes");

    // Note: CoreMQTT's stateful QoS management handles retransmission internally.
    // The MQTTPubAckInfo_t structure only tracks acknowledgment state, not the
    // original publish parameters (topic, payload, etc.).
    //
    // For proper DUP flag retransmission, we would need to:
    // 1. Store original publish info (topic, payload, QoS) for each packet
    // 2. Re-create MQTTPublishInfo_t from stored data
    // 3. Call MQTT_Publish() with dup=true
    //
    // However, CoreMQTT's MQTT_ProcessLoop() handles retransmission automatically
    // for QoS > 0 messages, so manual resending is typically not required.

    for (const auto &record : outgoingPublishRecords_)
    {
        if (record.packetId != MQTT_PACKET_ID_INVALID && record.publishState == MQTTPubAckPending)
        {
            LOPCORE_LOGD(TAG, "Pending PUBACK for packetId=%u (handled by CoreMQTT)", record.packetId);
        }
        else if (record.packetId != MQTT_PACKET_ID_INVALID && record.publishState == MQTTPubCompPending)
        {
            LOPCORE_LOGD(TAG, "Pending PUBCOMP for packetId=%u (handled by CoreMQTT)", record.packetId);
        }
    }

    return ESP_OK;
}

esp_err_t CoreMqttClient::resubscribeTopics()
{
    LOPCORE_LOGI(TAG, "Resubscribing to %zu topics", subscriptions_.size());

    for (const auto &sub : subscriptions_)
    {
        MQTTSubscribeInfo_t subscribeInfo = {};
        subscribeInfo.pTopicFilter = sub.topic.c_str();
        subscribeInfo.topicFilterLength = sub.topic.length();
        subscribeInfo.qos = static_cast<MQTTQoS_t>(qosToInt(sub.qos));

        uint16_t packetId = MQTT_GetPacketId(&mqttContext_);

        MQTTStatus_t mqttStatus = MQTT_Subscribe(&mqttContext_, &subscribeInfo, 1, packetId);

        if (mqttStatus != MQTTSuccess)
        {
            LOPCORE_LOGE(TAG, "Failed to resubscribe to '%s': %d", sub.topic.c_str(), mqttStatus);
            return ESP_FAIL;
        }

        LOPCORE_LOGD(TAG, "Resubscribed to '%s'", sub.topic.c_str());
    }

    return ESP_OK;
}

CoreMqttClient::Subscription *CoreMqttClient::findSubscription(const std::string &topic)
{
    auto it = std::find_if(subscriptions_.begin(), subscriptions_.end(),
                           [&topic](const Subscription &sub) { return sub.topic == topic; });

    return (it != subscriptions_.end()) ? &(*it) : nullptr;
}

// =============================================================================
// ProcessLoop Background Task
// =============================================================================

esp_err_t CoreMqttClient::startProcessLoopTask()
{
    // Check if already running
    if (processTask_ != nullptr)
    {
        LOPCORE_LOGW(TAG, "ProcessLoop task already running");
        return ESP_ERR_INVALID_STATE;
    }

    // Must be connected to start ProcessLoop
    if (state_ != MqttConnectionState::CONNECTED)
    {
        LOPCORE_LOGE(TAG, "Cannot start ProcessLoop task - not connected");
        return ESP_ERR_INVALID_STATE;
    }

    shouldRun_ = true;
    BaseType_t xReturned = xTaskCreate(processLoopTaskWrapper,
                                       "coremqtt_loop", // Task name
                                       4096,            // Stack size (4KB)
                                       this,            // Pass 'this' pointer as parameter
                                       5,               // Priority (tskIDLE_PRIORITY + 5)
                                       &processTask_    // Task handle
    );

    if (xReturned != pdPASS)
    {
        LOPCORE_LOGE(TAG, "Failed to create ProcessLoop task");
        processTask_ = nullptr;
        return ESP_FAIL;
    }

    LOPCORE_LOGI(TAG, "ProcessLoop task started");
    return ESP_OK;
}

esp_err_t CoreMqttClient::stopProcessLoopTask()
{
    if (processTask_ == nullptr)
    {
        // Already stopped
        return ESP_OK;
    }

    LOPCORE_LOGI(TAG, "Stopping ProcessLoop task...");

    // Signal task to stop
    shouldRun_ = false;

    // Give task time to exit gracefully (up to 500ms)
    for (int i = 0; i < 50 && processTask_ != nullptr; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Force delete if still running
    if (processTask_ != nullptr)
    {
        LOPCORE_LOGW(TAG, "Force deleting ProcessLoop task");
        vTaskDelete(processTask_);
        processTask_ = nullptr;
    }

    LOPCORE_LOGI(TAG, "ProcessLoop task stopped");
    return ESP_OK;
}

bool CoreMqttClient::isProcessLoopTaskRunning() const
{
    return processTask_ != nullptr;
}

void CoreMqttClient::processLoopTaskWrapper(void *pvParameters)
{
    CoreMqttClient *client = static_cast<CoreMqttClient *>(pvParameters);
    if (client)
    {
        client->processLoopTask();
    }
}

void CoreMqttClient::processLoopTask()
{
    LOPCORE_LOGI(TAG, "ProcessLoop task started (delay: %lu ms)", config_.processLoopDelayMs);

    while (shouldRun_ && state_ == MqttConnectionState::CONNECTED)
    {
        // Call processLoop with timeout matching the delay
        esp_err_t err = processLoop(config_.processLoopDelayMs);

        if (err != ESP_OK)
        {
            // Check if it's a connection error
            if (state_ != MqttConnectionState::CONNECTED)
            {
                LOPCORE_LOGW(TAG, "Connection lost in ProcessLoop");
                break;
            }

            // Log error but continue (might be timeout)
            if (err != ESP_ERR_TIMEOUT)
            {
                LOPCORE_LOGD(TAG, "ProcessLoop returned: %s", esp_err_to_name(err));
            }
        }

        // Small delay to prevent busy-waiting
        vTaskDelay(pdMS_TO_TICKS(config_.processLoopDelayMs));
    }

    LOPCORE_LOGI(TAG, "ProcessLoop task stopped");
    processTask_ = nullptr;
    vTaskDelete(nullptr);
}

} // namespace mqtt
} // namespace lopcore
