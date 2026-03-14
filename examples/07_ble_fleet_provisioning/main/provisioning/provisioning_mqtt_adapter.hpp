/**
 * @file provisioning_mqtt_adapter.hpp
 * @brief Duck-typed MQTT adapter for AwsFleetProvisioner using CoreMqttClient.
 *
 * AwsFleetProvisioner<T> requires duck-typed MQTT interface — T must provide:
 *   connect / disconnect / subscribe / unsubscribe / publish / isConnected / processEvents
 *
 * Uses CoreMqttClient + MbedtlsTransport (lopcore recommended path for AWS IoT).
 * Credentials are in-memory PEM strings (claim cert/key not yet in PKCS#11).
 */
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "lopcore/mqtt/coremqtt_client.hpp"
#include "lopcore/mqtt/mqtt_config.hpp"
#include "lopcore/tls/mbedtls_transport.hpp"
#include "lopcore/tls/tls_config.hpp"

class ProvisioningMqttAdapter
{
public:
    ProvisioningMqttAdapter() = default;
    ~ProvisioningMqttAdapter() = default;

    ProvisioningMqttAdapter(const ProvisioningMqttAdapter &) = delete;
    ProvisioningMqttAdapter &operator=(const ProvisioningMqttAdapter &) = delete;

    /**
     * Connect to AWS IoT with in-memory PEM claim credentials.
     * Two-step: connect MbedtlsTransport first, then construct CoreMqttClient.
     */
    bool connect(const char *endpoint,
                 uint16_t port,
                 const char *clientId,
                 const char *certPem,
                 const char *keyPem,
                 const char *rootCaPem);
    bool disconnect();

    bool subscribe(const char *topic, std::function<void(const char *, size_t)> callback);
    bool unsubscribe(const char *topic);
    bool publish(const char *topic, const char *payload, size_t length);
    bool isConnected() const;

    /**
     * Drive CoreMqttClient's synchronous process loop.
     * Must be called regularly — AwsFleetProvisioner calls this between operations.
     */
    void processEvents(uint32_t timeoutMs);

private:
    std::shared_ptr<lopcore::tls::MbedtlsTransport> transport_;
    std::unique_ptr<lopcore::mqtt::CoreMqttClient> client_;
};
