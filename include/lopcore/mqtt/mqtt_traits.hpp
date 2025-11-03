/**
 * @file mqtt_traits.hpp
 * @brief Type traits for MQTT client capability detection
 *
 * Provides compile-time capability checking for MQTT client implementations
 * using SFINAE (Substitution Failure Is Not An Error). This enables:
 * - Zero-overhead compile-time polymorphism
 * - Type-safe capability queries
 * - Automatic algorithm adaptation based on client features
 * - Prevention of calling unsupported operations
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include "mqtt_types.hpp"

namespace lopcore
{
namespace mqtt
{
namespace traits
{

// ========================================
// Trait 1: Basic MQTT Client
// ========================================

/**
 * @brief Check if type implements basic MQTT client operations
 *
 * Detects presence of:
 * - connect()
 * - disconnect()
 * - publish(topic, payload, qos)
 * - subscribe(topic, callback)
 * - isConnected()
 *
 * Usage:
 * @code
 * static_assert(is_mqtt_client_v<CoreMqttClient>, "Must be MQTT client");
 * @endcode
 */
template<typename T, typename = void>
struct is_mqtt_client : std::false_type
{
};

template<typename T>
struct is_mqtt_client<T,
                      std::void_t<decltype(std::declval<T>().connect()),
                                  decltype(std::declval<T>().disconnect()),
                                  decltype(std::declval<T>().publish(std::declval<std::string>(),
                                                                     std::declval<std::vector<uint8_t>>(),
                                                                     std::declval<MqttQos>())),
                                  decltype(std::declval<T>().subscribe(std::declval<std::string>(),
                                                                       std::declval<MessageCallback>())),
                                  decltype(std::declval<T>().isConnected())>> : std::true_type
{
};

template<typename T>
inline constexpr bool is_mqtt_client_v = is_mqtt_client<T>::value;

// ========================================
// Trait 2: Manual Processing Support
// ========================================

/**
 * @brief Check if type supports manual MQTT event processing
 *
 * Detects presence of:
 * - processLoop(timeoutMs)
 * - setAutoProcessing(enable)
 * - isAutoProcessingEnabled()
 *
 * Manual processing allows synchronous request-response patterns
 * by giving the caller control over when to process MQTT events.
 * CoreMQTT supports this, ESP-MQTT does not.
 *
 * Usage:
 * @code
 * if constexpr (has_manual_processing_v<MqttClient>) {
 *     client.setAutoProcessing(false);
 *     while (!responseReceived) {
 *         client.processLoop(1000);  // Manual polling
 *     }
 * }
 * @endcode
 */
template<typename T, typename = void>
struct has_manual_processing : std::false_type
{
};

template<typename T>
struct has_manual_processing<T,
                             std::void_t<decltype(std::declval<T>().processLoop(std::declval<uint32_t>())),
                                         decltype(std::declval<T>().setAutoProcessing(std::declval<bool>())),
                                         decltype(std::declval<T>().isAutoProcessingEnabled())>>
    : std::true_type
{
};

template<typename T>
inline constexpr bool has_manual_processing_v = has_manual_processing<T>::value;

// ========================================
// Trait 3: Statistics Support
// ========================================

/**
 * @brief Check if type provides MQTT statistics
 *
 * Detects presence of:
 * - getStatistics()
 * - resetStatistics()
 *
 * Statistics include message counts, connection events, retries, etc.
 *
 * Usage:
 * @code
 * if constexpr (has_statistics_v<MqttClient>) {
 *     auto stats = client.getStatistics();
 *     ESP_LOGI(TAG, "Messages sent: %u", stats.messagesSent);
 * }
 * @endcode
 */
template<typename T, typename = void>
struct has_statistics : std::false_type
{
};

template<typename T>
struct has_statistics<
    T,
    std::void_t<decltype(std::declval<T>().getStatistics()), decltype(std::declval<T>().resetStatistics())>>
    : std::true_type
{
};

template<typename T>
inline constexpr bool has_statistics_v = has_statistics<T>::value;

// ========================================
// Trait 4: QoS 2 Support
// ========================================

/**
 * @brief Check if type supports QoS 2 (Exactly Once) delivery
 *
 * QoS 2 requires additional state management and is not supported
 * by all MQTT implementations. CoreMQTT supports it, some lightweight
 * clients may not.
 *
 * Usage:
 * @code
 * if constexpr (supports_qos2_v<MqttClient>) {
 *     client.publish(topic, payload, MqttQos::EXACTLY_ONCE);
 * } else {
 *     // Fallback to QoS 1
 *     client.publish(topic, payload, MqttQos::AT_LEAST_ONCE);
 * }
 * @endcode
 */
template<typename T, typename = void>
struct supports_qos2 : std::false_type
{
};

// Check if publish() accepts EXACTLY_ONCE QoS
template<typename T>
struct supports_qos2<T,
                     std::void_t<decltype(std::declval<T>().publish(std::declval<std::string>(),
                                                                    std::declval<std::vector<uint8_t>>(),
                                                                    MqttQos::EXACTLY_ONCE))>> : std::true_type
{
};

template<typename T>
inline constexpr bool supports_qos2_v = supports_qos2<T>::value;

// ========================================
// Trait 5: Reconnection Management
// ========================================

/**
 * @brief Check if type provides explicit reconnection control
 *
 * Detects presence of:
 * - reconnect()
 * - setReconnectEnabled(bool)
 *
 * Some clients handle reconnection automatically in the background,
 * others expose explicit control.
 *
 * Usage:
 * @code
 * if constexpr (has_reconnection_control_v<MqttClient>) {
 *     client.setReconnectEnabled(false);  // Disable auto-reconnect
 *     // ... do maintenance ...
 *     client.reconnect();  // Explicitly reconnect
 * }
 * @endcode
 */
template<typename T, typename = void>
struct has_reconnection_control : std::false_type
{
};

template<typename T>
struct has_reconnection_control<
    T,
    std::void_t<decltype(std::declval<T>().reconnect()),
                decltype(std::declval<T>().setReconnectEnabled(std::declval<bool>()))>> : std::true_type
{
};

template<typename T>
inline constexpr bool has_reconnection_control_v = has_reconnection_control<T>::value;

// ========================================
// Trait 6: Budget Management
// ========================================

/**
 * @brief Check if type supports message budgeting
 *
 * Detects presence of budget-related methods for flood prevention.
 *
 * Usage:
 * @code
 * if constexpr (has_budget_management_v<MqttClient>) {
 *     client.setBudgetEnabled(true);
 * }
 * @endcode
 */
template<typename T, typename = void>
struct has_budget_management : std::false_type
{
};

template<typename T>
struct has_budget_management<T,
                             std::void_t<decltype(std::declval<T>().setBudgetEnabled(std::declval<bool>()))>>
    : std::true_type
{
};

template<typename T>
inline constexpr bool has_budget_management_v = has_budget_management<T>::value;

// ========================================
// Composite Traits
// ========================================

/**
 * @brief Check if client is suitable for synchronous request-response patterns
 *
 * Requires:
 * - Basic MQTT operations
 * - Manual processing capability
 *
 * Suitable for: AWS Fleet Provisioning, HTTP-over-MQTT, synchronous RPCs
 */
template<typename T>
inline constexpr bool is_synchronous_capable_v = is_mqtt_client_v<T> && has_manual_processing_v<T>;

/**
 * @brief Check if client is suitable for asynchronous event-driven patterns
 *
 * Requires:
 * - Basic MQTT operations
 *
 * All MQTT clients support this (callback-based operation)
 */
template<typename T>
inline constexpr bool is_asynchronous_capable_v = is_mqtt_client_v<T>;

/**
 * @brief Check if client supports production-grade features
 *
 * Requires:
 * - Basic MQTT operations
 * - Statistics
 * - Budget management or reconnection control
 */
template<typename T>
inline constexpr bool is_production_ready_v = is_mqtt_client_v<T> && has_statistics_v<T> &&
                                              (has_budget_management_v<T> || has_reconnection_control_v<T>);

// ========================================
// Utility: Compile-Time Capability Report
// ========================================

/**
 * @brief Print capabilities of an MQTT client type at compile-time
 *
 * Usage:
 * @code
 * static_assert(report_capabilities<CoreMqttClient>());
 * // Compilation will show all detected capabilities in error message
 * @endcode
 */
template<typename T>
constexpr bool report_capabilities()
{
    static_assert(is_mqtt_client_v<T>, "Type is not a valid MQTT client");

    // The compiler will show which static_asserts pass/fail
    static_assert(!has_manual_processing_v<T> || has_manual_processing_v<T>,
                  "Manual processing: available/unavailable");
    static_assert(!has_statistics_v<T> || has_statistics_v<T>, "Statistics: available/unavailable");
    static_assert(!supports_qos2_v<T> || supports_qos2_v<T>, "QoS 2: supported/unsupported");
    static_assert(!has_reconnection_control_v<T> || has_reconnection_control_v<T>,
                  "Reconnection control: available/unavailable");
    static_assert(!has_budget_management_v<T> || has_budget_management_v<T>,
                  "Budget management: available/unavailable");

    return true;
}

} // namespace traits
} // namespace mqtt
} // namespace lopcore
