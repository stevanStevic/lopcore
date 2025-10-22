/**
 * @file mqtt_client_factory.cpp
 * @brief Implementation of MQTT client factory
 *
 * @copyright Copyright (c) 2025
 */

#include "lopcore/mqtt/mqtt_client_factory.hpp"

#include <algorithm>

#include "lopcore/logging/logger.hpp"
#include "lopcore/tls/mbedtls_transport.hpp"
#include "lopcore/tls/tls_config.hpp"
#include "lopcore/tls/tls_transport.hpp"

static const char *TAG = "mqtt_factory";

namespace lopcore
{
namespace mqtt
{

std::unique_ptr<IMqttClient> MqttClientFactory::create(MqttClientType type, const MqttConfig &config)
{
    // Delegate to overload with null transport (will create default)
    return create(type, config, nullptr);
}

std::unique_ptr<IMqttClient> MqttClientFactory::create(MqttClientType type,
                                                       const MqttConfig &config,
                                                       std::shared_ptr<lopcore::tls::ITlsTransport> transport)
{
    // Validate configuration
    esp_err_t err = config.validate();
    if (err != ESP_OK)
    {
        LOPCORE_LOGE(TAG, "Invalid MQTT configuration: %s", esp_err_to_name(err));
        return nullptr;
    }

    // Auto-select if requested
    MqttClientType selectedType = type;
    if (type == MqttClientType::AUTO)
    {
        selectedType = selectType(config);
        LOPCORE_LOGI(TAG, "Auto-selected %s implementation for broker: %s", getTypeName(selectedType),
                     config.broker.c_str());
    }

    // Create appropriate implementation
    switch (selectedType)
    {
        case MqttClientType::ESP_MQTT:
            LOPCORE_LOGI(TAG, "Creating ESP-MQTT client");
            return std::make_unique<EspMqttClient>(config);

        case MqttClientType::AWS_IOT: {
            LOPCORE_LOGI(TAG, "Creating coreMQTT client");

            // Use provided transport or create from config
            std::shared_ptr<lopcore::tls::ITlsTransport> tlsTransport = transport;

            if (!tlsTransport)
            {
                // No transport provided - we need TLS config to create one
                if (!config.tls.has_value())
                {
                    LOPCORE_LOGE(TAG, "No TLS transport provided and no TLS config specified");
                    return nullptr;
                }

                LOPCORE_LOGI(TAG, "No TLS transport provided, creating MbedtlsTransport from config");

                // Validate TLS configuration
                err = config.tls->validate();
                if (err != ESP_OK)
                {
                    LOPCORE_LOGE(TAG, "Invalid TLS configuration: %s", esp_err_to_name(err));
                    return nullptr;
                }

                // Create and connect TLS transport
                tlsTransport = std::make_shared<lopcore::tls::MbedtlsTransport>();
                err = tlsTransport->connect(config.tls.value());
                if (err != ESP_OK)
                {
                    LOPCORE_LOGE(TAG, "TLS connection failed: %s", esp_err_to_name(err));
                    return nullptr;
                }
            }
            else
            {
                LOPCORE_LOGI(TAG, "Using pre-connected TLS transport (dependency injection)");
            }

            // Create MQTT client with transport (can be shared with HTTP, etc.)
            return std::make_unique<CoreMqttClient>(config, tlsTransport);
        }

        case MqttClientType::MOCK:
            LOPCORE_LOGE(TAG, "Mock client not yet implemented");
            return nullptr;

        default:
            LOPCORE_LOGE(TAG, "Unknown client type: %d", static_cast<int>(selectedType));
            return nullptr;
    }
}

MqttClientType MqttClientFactory::selectType(const MqttConfig &config)
{
    // Check if this is an AWS IoT endpoint
    if (isAwsIotEndpoint(config.broker))
    {
        return MqttClientType::AWS_IOT;
    }

    // Default to ESP-MQTT for general MQTT brokers
    return MqttClientType::ESP_MQTT;
}

const char *MqttClientFactory::getTypeName(MqttClientType type)
{
    switch (type)
    {
        case MqttClientType::AUTO:
            return "AUTO";
        case MqttClientType::ESP_MQTT:
            return "ESP-MQTT";
        case MqttClientType::AWS_IOT:
            return "AWS_IOT (coreMQTT)";
        case MqttClientType::MOCK:
            return "MOCK";
        default:
            return "UNKNOWN";
    }
}

bool MqttClientFactory::isAwsIotEndpoint(const std::string &broker)
{
    // Check for AWS IoT Core endpoints
    // Format: xxxxx.iot.region.amazonaws.com
    // Examples:
    // - a1b2c3d4e5f6g7.iot.us-east-1.amazonaws.com
    // - xxxxx-ats.iot.us-west-2.amazonaws.com

    // Simple check: contains both "iot." and "amazonaws.com"
    bool hasIot = (broker.find("iot.") != std::string::npos);
    bool hasAws = (broker.find("amazonaws.com") != std::string::npos);

    return hasIot && hasAws;
}

} // namespace mqtt
} // namespace lopcore
