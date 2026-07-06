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
 * - publish(topic, payload, qos, retain)
 * - subscribe(topic, callback, qos)
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
struct is_mqtt_client<
    T,
    std::void_t<decltype(std::declval<T>().connect()),
                decltype(std::declval<T>().disconnect()),
                decltype(std::declval<T>().publish(std::declval<const std::string &>(),
                                                   std::declval<const std::vector<uint8_t> &>(),
                                                   std::declval<MqttQos>(),
                                                   std::declval<bool>())),
                decltype(std::declval<T>().subscribe(std::declval<const std::string &>(),
                                                     std::declval<MessageCallback>(),
                                                     std::declval<MqttQos>())),
                decltype(std::declval<const T>().isConnected())>> : std::true_type
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
                                         decltype(std::declval<const T>().isAutoProcessingEnabled())>>
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
struct has_statistics<T,
                      std::void_t<decltype(std::declval<const T>().getStatistics()),
                                  decltype(std::declval<T>().resetStatistics())>> : std::true_type
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
 * Detects whether publish() accepts MqttQos::EXACTLY_ONCE. Both
 * CoreMqttClient and EspMqttClient take the full MqttQos enum, so both
 * satisfy this trait; a client restricted to a QoS 0/1-only enum would not.
 *
 * Usage:
 * @code
 * if constexpr (supports_qos2_v<MqttClient>) {
 *     client.publish(topic, payload, MqttQos::EXACTLY_ONCE, false);
 * } else {
 *     // Fallback to QoS 1
 *     client.publish(topic, payload, MqttQos::AT_LEAST_ONCE, false);
 * }
 * @endcode
 */
template<typename T, typename = void>
struct supports_qos2 : std::false_type
{
};

// Check if publish() accepts EXACTLY_ONCE QoS
template<typename T>
struct supports_qos2<
    T,
    std::void_t<decltype(std::declval<T>().publish(std::declval<const std::string &>(),
                                                   std::declval<const std::vector<uint8_t> &>(),
                                                   MqttQos::EXACTLY_ONCE,
                                                   std::declval<bool>()))>> : std::true_type
{
};

template<typename T>
inline constexpr bool supports_qos2_v = supports_qos2<T>::value;

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

} // namespace traits
} // namespace mqtt
} // namespace lopcore
