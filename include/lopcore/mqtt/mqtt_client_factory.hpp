/**
 * @file mqtt_client_factory.hpp
 * @brief Factory for creating MQTT client implementations
 *
 * Provides factory pattern for creating appropriate MQTT client implementations
 * based on configuration or explicit selection.
 *
 * @copyright Copyright (c) 2025
 */

#ifndef LOPCORE_MQTT_CLIENT_FACTORY_HPP
#define LOPCORE_MQTT_CLIENT_FACTORY_HPP

#include <memory>

#include "lopcore/mqtt/coremqtt_client.hpp"
#include "lopcore/mqtt/esp_mqtt_client.hpp"
#include "lopcore/mqtt/imqtt_client.hpp"
#include "lopcore/mqtt/mqtt_types.hpp"

// Forward declaration for TLS transport
namespace lopcore
{
namespace tls
{
class ITlsTransport;
}
} // namespace lopcore

namespace lopcore
{
namespace mqtt
{

/**
 * @brief Factory for creating MQTT clients
 *
 * Usage:
 * @code
 * // Auto-select based on broker
 * auto config = MqttConfig::builder()
 *     .broker("xxxxx.iot.us-east-1.amazonaws.com")
 *     .build();
 * auto client = MqttClientFactory::create(MqttClientType::AUTO, config);
 *
 * // Explicit selection with default transport
 * auto espClient = MqttClientFactory::create(MqttClientType::ESP_MQTT, config);
 *
 * // Provide custom transport (for AWS_IOT)
 * auto tlsTransport = std::make_shared<MbedtlsTransport>();
 * tlsTransport->connect(tlsConfig);
 * auto coreClient = MqttClientFactory::create(MqttClientType::AWS_IOT, config, tlsTransport);
 *
 * // Reuse transport between MQTT and HTTP
 * auto sharedTransport = std::make_shared<MbedtlsTransport>();
 * sharedTransport->connect(tlsConfig);
 * auto mqttClient = MqttClientFactory::create(MqttClientType::AWS_IOT, config, sharedTransport);
 * auto httpClient = std::make_unique<HttpClient>(httpConfig, sharedTransport);
 * @endcode
 */
class MqttClientFactory
{
public:
    /**
     * @brief Create MQTT client with default transport
     *
     * For AWS_IOT type, this will create a default MbedtlsTransport based on
     * the TLS configuration in MqttConfig. For full control over the transport,
     * use the overload that accepts a transport parameter.
     *
     * @param type Client implementation type
     * @param config MQTT configuration
     * @return Unique pointer to IMqttClient implementation
     * @throws std::invalid_argument if type is invalid or config is invalid
     * @throws std::runtime_error if client creation fails
     */
    static std::unique_ptr<IMqttClient> create(MqttClientType type, const MqttConfig &config);

    /**
     * @brief Create MQTT client with custom transport (dependency injection)
     *
     * This overload allows you to provide your own TLS transport implementation,
     * enabling:
     * - Custom TLS implementations (e.g., OpenSSL, BearSSL)
     * - Transport reuse between MQTT and HTTP clients
     * - Mock transports for testing
     * - Pre-configured or pre-connected transports
     *
     * @param type Client implementation type (must be AWS_IOT for transport to be used)
     * @param config MQTT configuration
     * @param transport TLS transport (must be connected before passing)
     * @return Unique pointer to IMqttClient implementation
     *
     * @note The transport must already be connected via transport->connect()
     *       before being passed to this method.
     * @note For ESP_MQTT type, the transport parameter is ignored (ESP-MQTT uses
     *       its own internal TLS implementation).
     *
     * @throws std::invalid_argument if type is invalid or config is invalid
     * @throws std::runtime_error if client creation fails
     */
    static std::unique_ptr<IMqttClient> create(MqttClientType type,
                                               const MqttConfig &config,
                                               std::shared_ptr<lopcore::tls::ITlsTransport> transport);

    /**
     * @brief Auto-select client type based on configuration
     *
     * Selection logic:
     * - If broker contains "amazonaws.com" -> CORE_MQTT
     * - If broker contains "iot." and ".amazonaws.com" -> CORE_MQTT
     * - Otherwise -> ESP_MQTT
     *
     * @param config MQTT configuration
     * @return Recommended client type
     */
    static MqttClientType selectType(const MqttConfig &config);

    /**
     * @brief Get human-readable name for client type
     * @param type Client type
     * @return Type name string
     */
    static const char *getTypeName(MqttClientType type);

private:
    /**
     * @brief Check if broker is AWS IoT endpoint
     */
    static bool isAwsIotEndpoint(const std::string &broker);
};

} // namespace mqtt
} // namespace lopcore

#endif // LOPCORE_MQTT_CLIENT_FACTORY_HPP
