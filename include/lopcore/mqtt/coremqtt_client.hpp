/**
 * @file coremqtt_client.hpp
 * @brief Standalone AWS coreMQTT library wrapper
 *
 * This class wraps the AWS IoT coreMQTT library to provide:
 * - Standalone MQTT client (no interface inheritance)
 * - C++ RAII resource management
 * - Integration with LopCore logging and configuration
 * - TLS/PKCS#11 transport support
 * - Stateful QoS tracking for AWS IoT reliability
 * - Manual processing capability (processLoop)
 *
 * Use this implementation for AWS IoT Core connectivity where stateful
 * QoS tracking and explicit state machine control are required.
 *
 * @copyright Copyright (c) 2025
 */

#ifndef LOPCORE_MQTT_COREMQTT_CLIENT_HPP
#define LOPCORE_MQTT_COREMQTT_CLIENT_HPP

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include <esp_timer.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lopcore/mqtt/mqtt_budget.hpp"
#include "lopcore/mqtt/mqtt_config.hpp"
#include "lopcore/mqtt/mqtt_types.hpp"

// Forward declarations for TLS transport
namespace lopcore
{
namespace tls
{
class ITlsTransport;
}
} // namespace lopcore

// Forward declare for circular dependency
namespace lopcore
{
namespace mqtt
{
class CoreMqttClient;
}
} // namespace lopcore

// NetworkContext with client back-reference
// CRITICAL: Using generic TLS abstraction from tls/network_context.h
// This keeps MQTT layer agnostic of specific TLS implementations
#include "lopcore/tls/network_context.h"

// coreMQTT headers
extern "C" {
#include "core_mqtt.h"
#include "transport_interface.h"
}

namespace lopcore
{
namespace mqtt
{

/**
 * @brief coreMQTT-based standalone MQTT client
 *
 * This class wraps the AWS IoT coreMQTT library as a standalone component
 * with the following characteristics:
 *
 * Features:
 * - Stateful QoS tracking (critical for AWS Device Shadow)
 * - Explicit state machine control (required for AWS IoT Jobs)
 * - Manual polling model (needed for Fleet Provisioning)
 * - Automatic background processing (optional ProcessLoop task)
 * - Minimal memory footprint (~5 KB RAM)
 * - Transport abstraction (TLS + PKCS#11)
 *
 * Architecture:
 * - Polling-based: Application calls processLoop() to handle network I/O
 * - Supports both synchronous and asynchronous modes
 * - State exposure: Can inspect QoS state arrays
 *
 * Use Cases:
 * - AWS IoT Core (Device Shadow, Jobs, Fleet Provisioning)
 * - Guaranteed delivery requirements
 * - Multi-step MQTT protocols
 * - Memory-constrained devices
 *
 * Thread Safety:
 * - Methods are thread-safe via mutex
 * - processLoop() must be called regularly (manually or via background task)
 */
class CoreMqttClient
{
public:
    /**
     * @brief Construct CoreMQTT client with dependency injection
     *
     * @param config MQTT configuration
     * @param transport TLS transport (must be connected before passing)
     *
     * @note The transport must already be connected via transport->connect()
     *       before being passed to this constructor.
     *
     * @note Uses shared_ptr to allow TLS connection sharing between multiple
     *       clients (e.g., MQTT and HTTP using the same TLS session).
     *
     * Example:
     * @code
     * auto tlsTransport = std::make_shared<MbedtlsTransport>();
     * tlsTransport->connect(tlsConfig);
     *
     * // Both clients share the same TLS connection
     * auto mqttClient = std::make_unique<CoreMqttClient>(mqttConfig, tlsTransport);
     * auto httpClient = std::make_unique<HttpClient>(httpConfig, tlsTransport);
     * @endcode
     */
    CoreMqttClient(const MqttConfig &config, std::shared_ptr<lopcore::tls::ITlsTransport> transport);

    /**
     * @brief Destructor - cleans up resources
     */
    ~CoreMqttClient();

    // Disable copy
    CoreMqttClient(const CoreMqttClient &) = delete;
    CoreMqttClient &operator=(const CoreMqttClient &) = delete;

    // =============================================================================
    // Core MQTT Operations
    // =============================================================================

    esp_err_t connect();
    esp_err_t disconnect();
    bool isConnected() const;

    esp_err_t publish(const std::string &topic,
                      const std::vector<uint8_t> &payload,
                      MqttQos qos = MqttQos::AT_MOST_ONCE,
                      bool retain = false);

    esp_err_t publishString(const std::string &topic,
                            const std::string &payload,
                            MqttQos qos = MqttQos::AT_MOST_ONCE,
                            bool retain = false);

    esp_err_t subscribe(const std::string &topic,
                        MessageCallback callback,
                        MqttQos qos = MqttQos::AT_MOST_ONCE);

    esp_err_t unsubscribe(const std::string &topic);

    esp_err_t setWillMessage(const std::string &topic,
                             const std::vector<uint8_t> &payload,
                             MqttQos qos = MqttQos::AT_MOST_ONCE,
                             bool retain = false);

    void setConnectionCallback(ConnectionCallback callback);
    void setErrorCallback(ErrorCallback callback);

    MqttStatistics getStatistics() const;
    void resetStatistics();

    // =============================================================================
    // CoreMQTT-Specific Methods
    // =============================================================================

    /**
     * @brief Process MQTT network I/O
     *
     * This method MUST be called regularly to:
     * - Send outgoing packets
     * - Receive incoming packets
     * - Process keep-alive pings
     * - Handle reconnection logic
     *
     * Typical usage:
     * @code
     * void mqtt_task(void* param) {
     *     CoreMqttClient* client = static_cast<CoreMqttClient*>(param);
     *     while (running) {
     *         client->processLoop(100);  // 100ms timeout
     *         vTaskDelay(pdMS_TO_TICKS(10));
     *     }
     * }
     * @endcode
     *
     * @param timeoutMs Maximum time to block waiting for network activity
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t processLoop(uint32_t timeoutMs = 100);

    /**
     * @brief Get publish state for a specific packet ID
     *
     * Allows inspection of QoS state for debugging or application logic.
     *
     * @param packetId MQTT packet ID
     * @return Publish state or MQTTStateNull if not found
     */
    MQTTPublishState_t getPublishState(uint16_t packetId) const;

    /**
     * @brief Check if there are any outstanding packets awaiting acknowledgment
     * @return true if packets are in-flight, false otherwise
     */
    bool hasOutstandingPackets() const;

    /**
     * @brief Start the background ProcessLoop task
     *
     * Creates a FreeRTOS task that continuously calls processLoop() to handle
     * incoming/outgoing MQTT packets, keep-alive pings, and ACKs.
     *
     * This task is automatically started by connect() unless auto-start is disabled
     * in the configuration. You can also start it manually for more control.
     *
     * @return ESP_OK on success, ESP_ERR_INVALID_STATE if already running,
     *         ESP_FAIL if task creation fails
     */
    esp_err_t startProcessLoopTask();

    /**
     * @brief Stop the background ProcessLoop task
     *
     * Gracefully stops the ProcessLoop task. Waits up to 500ms for the task
     * to exit cleanly before forcing deletion.
     *
     * This task is automatically stopped by disconnect().
     *
     * @return ESP_OK on success
     */
    esp_err_t stopProcessLoopTask();

    /**
     * @brief Check if ProcessLoop task is running
     * @return true if task is running, false otherwise
     */
    bool isProcessLoopTaskRunning() const;

    /**
     * @brief Enable/disable automatic background processing
     *
     * When enabled, the ProcessLoop task runs automatically in the background.
     * When disabled, you must call processLoop() manually.
     *
     * @param enable true to start background task, false to stop it
     * @return ESP_OK on success
     */
    esp_err_t setAutoProcessing(bool enable);

    /**
     * @brief Check if automatic processing is enabled
     * @return true if ProcessLoop task is running
     */
    bool isAutoProcessingEnabled() const;

    // =============================================================================
    // Accessors (no virtual - plain getters)
    // =============================================================================

    MqttConnectionState getConnectionState() const
    {
        return state_;
    }
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
    // =============================================================================
    // Internal Structures
    // =============================================================================

    /**
     * @brief Subscription record
     */
    struct Subscription
    {
        std::string topic;
        MessageCallback callback;
        MqttQos qos;
    };

    // =============================================================================
    // Transport Layer
    // =============================================================================

    /**
     * @brief Static transport send function (coreMQTT callback)
     */
    static int32_t transportSend(NetworkContext_t *pNetworkContext, const void *pBuffer, size_t bytesToSend);

    /**
     * @brief Static transport receive function (coreMQTT callback)
     */
    static int32_t transportRecv(NetworkContext_t *pNetworkContext, void *pBuffer, size_t bytesToRecv);

    // =============================================================================
    // coreMQTT Callbacks
    // =============================================================================

    /**
     * @brief Static event callback (coreMQTT callback)
     */
    static void eventCallback(MQTTContext_t *pMqttContext,
                              MQTTPacketInfo_t *pPacketInfo,
                              MQTTDeserializedInfo_t *pDeserializedInfo);

    /**
     * @brief Handle incoming events (called from eventCallback)
     */
    void handleEvent(MQTTPacketInfo_t *pPacketInfo, MQTTDeserializedInfo_t *pDeserializedInfo);

    /**
     * @brief Get current time in milliseconds (coreMQTT callback)
     */
    static uint32_t getTimeMs();

    // =============================================================================
    // State Management
    // =============================================================================

    /**
     * @brief Resend unacknowledged publishes after reconnect
     */
    esp_err_t resendPendingPublishes();

    /**
     * @brief Resubscribe to topics after reconnect
     */
    esp_err_t resubscribeTopics();

    /**
     * @brief Find subscription by topic
     */
    Subscription *findSubscription(const std::string &topic);

    /**
     * @brief Background task that processes MQTT loop
     */
    void processLoopTask();

    /**
     * @brief Static wrapper for FreeRTOS task creation
     */
    static void processLoopTaskWrapper(void *pvParameters);

    // =============================================================================
    // Member Variables
    // =============================================================================

    MqttConfig config_;                                         ///< Configuration
    MQTTContext_t mqttContext_;                                 ///< coreMQTT context
    TransportInterface_t transport_;                            ///< Transport interface
    NetworkContext_t networkContext_;                           ///< Network context (TLS)
    std::shared_ptr<lopcore::tls::ITlsTransport> tlsTransport_; ///< TLS transport (shared, can be used by
                                                                ///< multiple clients)
    std::unique_ptr<MqttBudget> budget_;                        ///< Message budgeting
    std::atomic<MqttConnectionState> state_;                    ///< Connection state
    std::vector<uint8_t> networkBuffer_;                        ///< Network buffer
    std::vector<MQTTPubAckInfo_t> outgoingPublishRecords_;      ///< Outgoing QoS records
    std::vector<MQTTPubAckInfo_t> incomingPublishRecords_;      ///< Incoming QoS records
    std::vector<Subscription> subscriptions_;                   ///< Active subscriptions
    ConnectionCallback connectionCallback_;                     ///< Connection callback
    ErrorCallback errorCallback_;                               ///< Error callback
    MqttStatistics statistics_;                                 ///< Statistics
    mutable std::mutex mutex_;                                  ///< Thread safety
    TaskHandle_t processTask_;                                  ///< Process loop task handle
    std::atomic<bool> shouldRun_;                               ///< Process loop control (atomic, no mutex needed)
    SemaphoreHandle_t taskStoppedSemaphore_;                    ///< Signals when task has stopped
};

} // namespace mqtt
} // namespace lopcore

#endif // LOPCORE_MQTT_COREMQTT_CLIENT_HPP
