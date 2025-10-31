/**
 * @file mqtt_config.hpp
 * @brief MQTT client configuration with builder pattern
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include <esp_err.h>

#include "lopcore/tls/tls_config.hpp" // Use unified TLS configuration

#include "mqtt_types.hpp"

namespace lopcore
{
namespace mqtt
{

// Forward declarations
class BudgetConfigBuilder;
class ReconnectConfigBuilder;

// Type alias for backwards compatibility
using TlsConfig = lopcore::tls::TlsConfig;

/**
 * @brief Message budgeting configuration (anti-flooding)
 */
struct BudgetConfig
{
    bool enabled{true};                   ///< Enable message budgeting
    int32_t defaultBudget{100};           ///< Initial budget
    int32_t maxBudget{1024};              ///< Maximum budget cap
    uint8_t reviveCount{1};               ///< Messages restored per period
    std::chrono::seconds revivePeriod{5}; ///< Budget restoration interval

    /**
     * @brief Validate budget configuration
     * @return ESP_OK if valid, error code otherwise
     */
    esp_err_t validate() const
    {
        if (defaultBudget < 0 || maxBudget < 0)
        {
            return ESP_ERR_INVALID_ARG;
        }

        if (defaultBudget > maxBudget)
        {
            return ESP_ERR_INVALID_ARG;
        }

        if (reviveCount == 0 || revivePeriod.count() == 0)
        {
            return ESP_ERR_INVALID_ARG;
        }

        return ESP_OK;
    }
};

/**
 * @brief Reconnection strategy configuration
 */
struct ReconnectConfig
{
    bool autoReconnect{true};                     ///< Enable auto-reconnect
    std::chrono::milliseconds initialDelay{1000}; ///< Initial reconnect delay
    std::chrono::milliseconds maxDelay{60000};    ///< Maximum reconnect delay
    uint32_t maxAttempts{0};                      ///< Max attempts (0 = infinite)
    bool exponentialBackoff{true};                ///< Use exponential backoff
    float backoffMultiplier{2.0f};                ///< Backoff multiplier
    float jitterFactor{0.25f};                    ///< Random jitter (0.0-1.0)

    /**
     * @brief Validate reconnect configuration
     * @return ESP_OK if valid, error code otherwise
     */
    esp_err_t validate() const
    {
        if (initialDelay.count() <= 0 || maxDelay.count() <= 0)
        {
            return ESP_ERR_INVALID_ARG;
        }

        if (initialDelay > maxDelay)
        {
            return ESP_ERR_INVALID_ARG;
        }

        if (backoffMultiplier < 1.0f)
        {
            return ESP_ERR_INVALID_ARG;
        }

        if (jitterFactor < 0.0f || jitterFactor > 1.0f)
        {
            return ESP_ERR_INVALID_ARG;
        }

        return ESP_OK;
    }
};

/**
 * @brief Last Will and Testament configuration
 */
struct WillConfig
{
    std::string topic;                  ///< Will topic
    std::vector<uint8_t> payload;       ///< Will message payload
    MqttQos qos{MqttQos::AT_MOST_ONCE}; ///< Will message QoS
    bool retain{false};                 ///< Will message retain flag

    /**
     * @brief Check if will is configured
     * @return True if topic is set
     */
    bool isConfigured() const
    {
        return !topic.empty();
    }

    /**
     * @brief Validate will configuration
     * @return ESP_OK if valid, error code otherwise
     */
    esp_err_t validate() const
    {
        if (topic.empty())
        {
            return ESP_OK; // Will is optional
        }

        // Topic validation (basic)
        if (topic.find('#') != std::string::npos || topic.find('+') != std::string::npos)
        {
            return ESP_ERR_INVALID_ARG; // Wildcards not allowed in publish
        }

        return ESP_OK;
    }
};

/**
 * @brief Complete MQTT client configuration
 */
class MqttConfig
{
public:
    std::string broker;                 ///< Broker hostname/IP
    uint16_t port{1883};                ///< Broker port (1883=plain, 8883=TLS)
    std::string clientId;               ///< MQTT client ID (unique)
    std::chrono::seconds keepAlive{60}; ///< Keep-alive interval
    bool cleanSession{true};            ///< Clean session flag
    std::string username;               ///< Username (optional)
    std::string password;               ///< Password (optional)
    uint32_t networkBufferSize{4096};   ///< Network buffer size
    bool autoStartProcessLoop{true};    ///< Auto-start ProcessLoop task on connect (CoreMQTT only)
    uint32_t processLoopTimeoutMs{100}; ///< ProcessLoop timeout per call in milliseconds (CoreMQTT only)
    uint32_t processLoopDelayMs{10}; ///< ProcessLoop task sleep delay between calls in milliseconds (CoreMQTT
                                     ///< only)

    std::optional<TlsConfig> tls; ///< TLS configuration (optional - if not set, transport must be injected)
    BudgetConfig budget;          ///< Budgeting configuration
    ReconnectConfig reconnect;    ///< Reconnection configuration
    WillConfig will;              ///< Last Will and Testament

    /**
     * @brief Validate complete configuration
     * @return ESP_OK if valid, error code otherwise
     */
    esp_err_t validate() const
    {
        if (broker.empty())
        {
            return ESP_ERR_INVALID_ARG;
        }

        if (port == 0)
        {
            return ESP_ERR_INVALID_ARG;
        }

        if (clientId.empty())
        {
            return ESP_ERR_INVALID_ARG;
        }

        if (keepAlive.count() == 0)
        {
            return ESP_ERR_INVALID_ARG;
        }

        if (networkBufferSize < 1024)
        {
            return ESP_ERR_INVALID_ARG;
        }

        if (processLoopTimeoutMs == 0 || processLoopTimeoutMs > 5000)
        {
            return ESP_ERR_INVALID_ARG; // Timeout should be 1-5000ms
        }

        if (processLoopDelayMs == 0 || processLoopDelayMs > 1000)
        {
            return ESP_ERR_INVALID_ARG; // Delay should be 1-1000ms
        }

        // Validate sub-configurations
        // TLS is optional - only validate if present
        if (tls.has_value())
        {
            esp_err_t err = tls->validate();
            if (err != ESP_OK)
                return err;
        }

        esp_err_t err = budget.validate();
        if (err != ESP_OK)
            return err;

        err = reconnect.validate();
        if (err != ESP_OK)
            return err;

        err = will.validate();
        if (err != ESP_OK)
            return err;

        return ESP_OK;
    }

    /**
     * @brief Create builder for configuration
     * @return Configuration builder
     */
    static class MqttConfigBuilder builder();
};

/**
 * @brief Builder for budget configuration
 */
class BudgetConfigBuilder
{
public:
    BudgetConfigBuilder &enabled(bool enable)
    {
        config_.enabled = enable;
        return *this;
    }

    BudgetConfigBuilder &defaultBudget(int32_t budget)
    {
        config_.defaultBudget = budget;
        return *this;
    }

    BudgetConfigBuilder &maxBudget(int32_t budget)
    {
        config_.maxBudget = budget;
        return *this;
    }

    BudgetConfigBuilder &reviveCount(uint8_t count)
    {
        config_.reviveCount = count;
        return *this;
    }

    BudgetConfigBuilder &revivePeriod(std::chrono::seconds period)
    {
        config_.revivePeriod = period;
        return *this;
    }

    BudgetConfig build()
    {
        return config_;
    }

private:
    friend class MqttConfigBuilder;
    BudgetConfig config_;
};

/**
 * @brief Builder for reconnect configuration
 */
class ReconnectConfigBuilder
{
public:
    ReconnectConfigBuilder &autoReconnect(bool enable)
    {
        config_.autoReconnect = enable;
        return *this;
    }

    ReconnectConfigBuilder &initialDelay(std::chrono::milliseconds delay)
    {
        config_.initialDelay = delay;
        return *this;
    }

    ReconnectConfigBuilder &maxDelay(std::chrono::milliseconds delay)
    {
        config_.maxDelay = delay;
        return *this;
    }

    ReconnectConfigBuilder &maxAttempts(uint32_t attempts)
    {
        config_.maxAttempts = attempts;
        return *this;
    }

    ReconnectConfigBuilder &exponentialBackoff(bool enable)
    {
        config_.exponentialBackoff = enable;
        return *this;
    }

    ReconnectConfigBuilder &backoffMultiplier(float multiplier)
    {
        config_.backoffMultiplier = multiplier;
        return *this;
    }

    ReconnectConfigBuilder &jitterFactor(float factor)
    {
        config_.jitterFactor = factor;
        return *this;
    }

    ReconnectConfig build()
    {
        return config_;
    }

private:
    friend class MqttConfigBuilder;
    ReconnectConfig config_;
};

/**
 * @brief Builder for MQTT configuration (main builder)
 */
class MqttConfigBuilder
{
public:
    MqttConfigBuilder &broker(const std::string &brokerAddress)
    {
        config_.broker = brokerAddress;
        return *this;
    }

    MqttConfigBuilder &port(uint16_t brokerPort)
    {
        config_.port = brokerPort;
        return *this;
    }

    MqttConfigBuilder &clientId(const std::string &id)
    {
        config_.clientId = id;
        return *this;
    }

    MqttConfigBuilder &keepAlive(std::chrono::seconds interval)
    {
        config_.keepAlive = interval;
        return *this;
    }

    MqttConfigBuilder &cleanSession(bool clean)
    {
        config_.cleanSession = clean;
        return *this;
    }

    MqttConfigBuilder &username(const std::string &user)
    {
        config_.username = user;
        return *this;
    }

    MqttConfigBuilder &password(const std::string &pass)
    {
        config_.password = pass;
        return *this;
    }

    MqttConfigBuilder &networkBufferSize(uint32_t size)
    {
        config_.networkBufferSize = size;
        return *this;
    }

    /**
     * @brief Enable/disable automatic ProcessLoop task start on connect (CoreMQTT only)
     *
     * @param autoStart If true, ProcessLoop task starts automatically after connect.
     *                  If false, you must manually call startProcessLoopTask().
     * @return Reference to builder for chaining
     *
     * @note This only affects CoreMQTT client. ESP-MQTT handles this internally.
     * @note Default is true (automatic start)
     */
    MqttConfigBuilder &autoStartProcessLoop(bool autoStart)
    {
        config_.autoStartProcessLoop = autoStart;
        return *this;
    }

    /**
     * @brief Set ProcessLoop timeout per call in milliseconds (CoreMQTT only)
     *
     * @param timeoutMs How long to wait for packets in each processLoop() call (1-5000ms)
     * @return Reference to builder for chaining
     *
     * @note This controls how long processLoop() blocks waiting for incoming data
     * @note Lower values = more responsive to stop signals but more CPU usage
     * @note Higher values = less CPU usage but slower responsiveness
     * @note Default is 100ms
     */
    MqttConfigBuilder &processLoopTimeout(uint32_t timeoutMs)
    {
        config_.processLoopTimeoutMs = timeoutMs;
        return *this;
    }

    /**
     * @brief Set ProcessLoop task sleep delay in milliseconds (CoreMQTT only)
     *
     * @param delayMs Delay between ProcessLoop iterations (1-1000ms)
     * @return Reference to builder for chaining
     *
     * @note Lower values = more responsive but higher CPU usage
     * @note Higher values = less CPU usage but slower message processing
     * @note Default is 10ms
     */
    MqttConfigBuilder &processLoopDelay(uint32_t delayMs)
    {
        config_.processLoopDelayMs = delayMs;
        return *this;
    }

    /**
     * @brief Set TLS configuration
     *
     * @note Use lopcore::tls::TlsConfigBuilder to create TlsConfig
     *
     * Example:
     * @code
     * auto tlsConfig = lopcore::tls::TlsConfigBuilder()
     *     .hostname("mqtt.example.com")
     *     .port(8883)
     *     .caCertificate("/spiffs/certs/ca.crt")
     *     .build();
     *
     * auto mqttConfig = MqttConfigBuilder()
     *     .broker("mqtt.example.com")
     *     .tlsConfig(tlsConfig)
     *     .build();
     * @endcode
     */
    MqttConfigBuilder &tlsConfig(const TlsConfig &tlsConf)
    {
        config_.tls = tlsConf;
        return *this;
    }

    BudgetConfigBuilder budgeting()
    {
        BudgetConfigBuilder builder;
        builder.config_ = config_.budget;
        return builder;
    }

    MqttConfigBuilder &budgetConfig(const BudgetConfig &budgetConf)
    {
        config_.budget = budgetConf;
        return *this;
    }

    ReconnectConfigBuilder reconnection()
    {
        ReconnectConfigBuilder builder;
        builder.config_ = config_.reconnect;
        return builder;
    }

    MqttConfigBuilder &reconnectConfig(const ReconnectConfig &reconnectConf)
    {
        config_.reconnect = reconnectConf;
        return *this;
    }

    MqttConfigBuilder &willTopic(const std::string &topic)
    {
        config_.will.topic = topic;
        return *this;
    }

    MqttConfigBuilder &willPayload(const std::vector<uint8_t> &payload)
    {
        config_.will.payload = payload;
        return *this;
    }

    MqttConfigBuilder &willQos(MqttQos qos)
    {
        config_.will.qos = qos;
        return *this;
    }

    MqttConfigBuilder &willRetain(bool retain)
    {
        config_.will.retain = retain;
        return *this;
    }

    MqttConfig build()
    {
        return config_;
    }

private:
    MqttConfig config_;
};

// Implementation of static builder method
inline MqttConfigBuilder MqttConfig::builder()
{
    return MqttConfigBuilder();
}

} // namespace mqtt
} // namespace lopcore
