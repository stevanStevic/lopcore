/**
 * @file provisioning_mqtt_adapter.cpp
 */
#include "provisioning/provisioning_mqtt_adapter.hpp"

#include "lopcore/logging/logger.hpp"

using lopcore::mqtt::MqttConfig;
using lopcore::mqtt::MqttQos;
using lopcore::tls::TlsConfig;

static const char *TAG = "prov_mqtt";

bool ProvisioningMqttAdapter::connect(const char *endpoint,
                                       uint16_t port,
                                       const char *clientId,
                                       const char *certPem,
                                       const char *keyPem,
                                       const char *rootCaPem)
{
    LOPCORE_LOGI(TAG, "  [MQTT] Connecting to %s:%u as %s", endpoint, port, clientId);

    // Step 1: Build TLS config with in-memory PEM credentials.
    // NOTE: caCertPath accepts inline PEM strings (not just file paths).
    TlsConfig tlsCfg;
    tlsCfg.hostname     = endpoint;
    tlsCfg.port         = port;
    tlsCfg.caCertPath   = rootCaPem;
    tlsCfg.clientCertPem = certPem;
    tlsCfg.clientKeyPem  = keyPem;

    // Step 2: Create and connect transport BEFORE constructing CoreMqttClient.
    // CoreMqttClient constructor requires an already-connected ITlsTransport.
    transport_ = std::make_shared<lopcore::tls::MbedtlsTransport>();
    if (transport_->connect(tlsCfg) != ESP_OK)
    {
        LOPCORE_LOGE(TAG, "  [MQTT] TLS connect failed");
        transport_.reset();
        return false;
    }

    // Step 3: Construct CoreMqttClient with the connected transport.
    MqttConfig mqttCfg;
    mqttCfg.broker               = endpoint;
    mqttCfg.port                 = port;
    mqttCfg.clientId             = clientId;
    mqttCfg.keepAlive            = std::chrono::seconds(60);
    mqttCfg.reconnect.autoReconnect = false;
    mqttCfg.budget.enabled       = false;

    client_ = std::make_unique<lopcore::mqtt::CoreMqttClient>(mqttCfg, transport_);
    if (client_->connect() != ESP_OK)
    {
        LOPCORE_LOGE(TAG, "  [MQTT] MQTT connect failed");
        client_.reset();
        transport_->disconnect();
        transport_.reset();
        return false;
    }

    LOPCORE_LOGI(TAG, "  [MQTT] Connected");
    return true;
}

bool ProvisioningMqttAdapter::disconnect()
{
    if (client_)
    {
        client_->disconnect();
        client_.reset();
    }
    if (transport_)
    {
        transport_->disconnect();
        transport_.reset();
    }
    return true;
}

bool ProvisioningMqttAdapter::subscribe(const char *topic,
                                         std::function<void(const char *, size_t)> callback)
{
    if (!client_)
        return false;

    auto bridged = [cb = std::move(callback)](const lopcore::mqtt::MqttMessage &msg) {
        cb(reinterpret_cast<const char *>(msg.payload.data()), msg.payload.size());
    };

    return client_->subscribe(topic, std::move(bridged), MqttQos::AT_LEAST_ONCE) == ESP_OK;
}

bool ProvisioningMqttAdapter::unsubscribe(const char *topic)
{
    if (!client_)
        return true;
    return client_->unsubscribe(topic) == ESP_OK;
}

bool ProvisioningMqttAdapter::publish(const char *topic, const char *payload, size_t length)
{
    if (!client_)
        return false;
    return client_->publishString(topic, std::string(payload, length),
                                  MqttQos::AT_LEAST_ONCE, false) == ESP_OK;
}

bool ProvisioningMqttAdapter::isConnected() const
{
    return client_ && client_->isConnected();
}

void ProvisioningMqttAdapter::processEvents(uint32_t timeoutMs)
{
    if (client_)
        client_->processLoop(timeoutMs > 0 ? timeoutMs : 10);
}
