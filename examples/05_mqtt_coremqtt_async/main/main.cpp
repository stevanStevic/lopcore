/**
 * @file main.cpp
 * @brief CoreMQTT Async Client Example
 *
 * Demonstrates basic MQTT communication using CoreMQTT client with TLS:
 * - Secure TLS connection
 * - Automatic message processing with callbacks
 * - Request-response pattern
 * - Async pub/sub operations
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#include <stdio.h>

#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lopcore/logging/console_sink.hpp"
#include "lopcore/logging/logger.hpp"
#include "lopcore/mqtt/coremqtt_client.hpp"
#include "lopcore/tls/mbedtls_transport.hpp"

#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"

static const char *TAG = "mqtt_coremqtt_async";

// ============================================================================
// MQTT Broker Configuration
// ============================================================================

// For TLS testing, you can use:
// - test.mosquitto.org:8883 (requires server cert validation)
// - broker.emqx.io:8883
const char *brokerHost = "test.mosquitto.org";
const uint16_t brokerPort = 8883;
const char *clientId = "lopcore_coremqtt_async";

// Topics for demonstration
const char *sensorTopic = "lopcore/sensors/temperature";
const char *commandTopic = "lopcore/commands";
const char *responseTopic = "lopcore/responses";

// ============================================================================
// Test Certificates
// ============================================================================

// ISRG Root X1 (Let's Encrypt root CA)
// Used by test.mosquitto.org
const char *testRootCA = R"(-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----)";

// ============================================================================
// Message Callbacks
// ============================================================================

void onCommandReceived(const lopcore::mqtt::MqttMessage &message)
{
    std::string command(message.payload.begin(), message.payload.end());
    LOPCORE_LOGI(TAG, "Command received: %s", command.c_str());

    // Example: Echo command back as response
    if (command == "status")
    {
        LOPCORE_LOGI(TAG, "Handling 'status' command");
        // Response will be sent in main loop
    }
}

void onResponseReceived(const lopcore::mqtt::MqttMessage &message)
{
    LOPCORE_LOGI(TAG, "Response: %.*s", (int) message.payload.size(), message.payload.data());
}

// ============================================================================
// Clock compatibility layer (stub for linking)
// ============================================================================

extern "C" void Clock_SleepMs(uint32_t sleepTimeMs)
{
    vTaskDelay(pdMS_TO_TICKS(sleepTimeMs));
}

// ============================================================================
// SPIFFS Initialization and Certificate Setup
// ============================================================================

esp_err_t initializeSpiffsAndCertificate()
{
    LOPCORE_LOGI(TAG, "Initializing SPIFFS...");

    // Configure SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs", .partition_label = NULL, .max_files = 5, .format_if_mount_failed = true};

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        LOPCORE_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }

    // Check SPIFFS info
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret == ESP_OK)
    {
        LOPCORE_LOGI(TAG, "SPIFFS: %d KB total, %d KB used", total / 1024, used / 1024);
    }

    // Write root CA certificate to SPIFFS
    const char *certPath = "/spiffs/root_ca.crt";
    LOPCORE_LOGI(TAG, "Writing certificate to %s...", certPath);

    FILE *f = fopen(certPath, "w");
    if (f == NULL)
    {
        LOPCORE_LOGE(TAG, "Failed to open certificate file for writing");
        return ESP_FAIL;
    }

    size_t written = fwrite(testRootCA, 1, strlen(testRootCA), f);
    fclose(f);

    if (written != strlen(testRootCA))
    {
        LOPCORE_LOGE(TAG, "Failed to write complete certificate");
        return ESP_FAIL;
    }

    LOPCORE_LOGI(TAG, "Certificate written successfully (%d bytes)", written);
    return ESP_OK;
}

// ============================================================================
// Main Application
// ============================================================================

extern "C" void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize SPIFFS and write certificate
    ESP_ERROR_CHECK(initializeSpiffsAndCertificate());

    // Initialize logger
    auto &logger = lopcore::Logger::getInstance();
    logger.addSink(std::make_unique<lopcore::ConsoleSink>());
    logger.setGlobalLevel(lopcore::LogLevel::INFO);

    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "CoreMQTT Async Client Example (TLS)");
    LOPCORE_LOGI(TAG, "===========================================");

    // Note: This example assumes WiFi is already connected
    // In a real application, initialize and connect WiFi first

    // ========================================================================
    // 1. Configure TLS with Certificate from SPIFFS
    // ========================================================================

    LOPCORE_LOGI(TAG, "Configuring TLS...");

    // Certificate is stored in SPIFFS at /spiffs/root_ca.crt
    // For mutual TLS authentication, add client certificate and key via PKCS#11
    auto tlsConfig = lopcore::tls::TlsConfigBuilder()
                         .hostname(brokerHost)
                         .port(brokerPort)
                         .caCertificate("/spiffs/root_ca.crt") // Certificate from SPIFFS
                         .verifyPeer(true)                     // Enable peer verification
                         .build();

    // ========================================================================
    // 2. Create TLS Transport
    // ========================================================================

    LOPCORE_LOGI(TAG, "Setting up TLS transport to %s:%d...", brokerHost, brokerPort);

    auto tlsTransport = std::make_shared<lopcore::tls::MbedtlsTransport>();

    if (tlsTransport->connect(tlsConfig) != ESP_OK)
    {
        LOPCORE_LOGE(TAG, "Failed to establish TLS connection");
        return;
    }

    LOPCORE_LOGI(TAG, "TLS connection established");

    // ========================================================================
    // 3. Configure MQTT Client
    // ========================================================================

    auto mqttConfig = lopcore::mqtt::MqttConfig::builder()
                          .clientId(clientId)
                          .keepAlive(std::chrono::seconds(60))
                          .cleanSession(true)
                          .build();

    // ========================================================================
    // 4. Create CoreMQTT Client (Async Mode)
    // ========================================================================

    LOPCORE_LOGI(TAG, "Creating CoreMQTT client...");

    auto mqttClient = std::make_unique<lopcore::mqtt::CoreMqttClient>(mqttConfig, tlsTransport);

    // Set connection callback
    mqttClient->setConnectionCallback([](bool connected) {
        if (connected)
        {
            LOPCORE_LOGI(TAG, "Connected to broker");
        }
        else
        {
            LOPCORE_LOGW(TAG, "Disconnected from broker");
        }
    });

    // Set error callback
    mqttClient->setErrorCallback([](lopcore::mqtt::MqttError error, const std::string &message) {
        LOPCORE_LOGE(TAG, "Error: %s", message.c_str());
    });

    // ========================================================================
    // 6. Connect to Broker
    // ========================================================================

    LOPCORE_LOGI(TAG, "Connecting to broker...");

    if (mqttClient->connect() != ESP_OK)
    {
        LOPCORE_LOGE(TAG, "Failed to connect");
        return;
    }

    // Wait for connection
    vTaskDelay(pdMS_TO_TICKS(3000));

    if (!mqttClient->isConnected())
    {
        LOPCORE_LOGE(TAG, "Connection timeout - check WiFi and TLS configuration");
        return;
    }

    // ========================================================================
    // 7. Subscribe to Topics
    // ========================================================================

    LOPCORE_LOGI(TAG, "Subscribing to topics...");

    mqttClient->subscribe(commandTopic, onCommandReceived, lopcore::mqtt::MqttQos::AT_LEAST_ONCE);
    mqttClient->subscribe(responseTopic, onResponseReceived, lopcore::mqtt::MqttQos::AT_LEAST_ONCE);

    LOPCORE_LOGI(TAG, "Subscribed to command and response topics");

    vTaskDelay(pdMS_TO_TICKS(1000));

    // ========================================================================
    // 8. Publish Initial Message
    // ========================================================================

    const char *initMsg = "{\"status\":\"online\",\"client\":\"coremqtt_async\"}";
    if (mqttClient->publishString(responseTopic, initMsg, lopcore::mqtt::MqttQos::AT_LEAST_ONCE, false) ==
        ESP_OK)
    {
        LOPCORE_LOGI(TAG, "Published initial status");
    }

    // ========================================================================
    // 9. Periodic Sensor Data Publishing
    // ========================================================================

    LOPCORE_LOGI(TAG, "Starting periodic publishing (every 20 seconds)...");
    LOPCORE_LOGI(TAG, "Try sending commands:");
    LOPCORE_LOGI(TAG, "  mosquitto_pub -h %s -p 8883 --cafile ca.crt -t %s -m \"status\"", brokerHost,
                 commandTopic);

    int counter = 0;
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(20000));

        if (!mqttClient->isConnected())
        {
            LOPCORE_LOGW(TAG, "Not connected, waiting for reconnection...");
            continue;
        }

        // Simulate sensor data
        char sensorData[128];
        int currentCounter = counter++;
        snprintf(sensorData, sizeof(sensorData), "{\"temperature\":%.1f,\"humidity\":%.1f,\"counter\":%d}",
                 22.5 + (currentCounter % 10) * 0.5, // Simulated temperature
                 45.0 + (currentCounter % 20) * 1.0, // Simulated humidity
                 currentCounter);

        // Publish sensor data
        if (mqttClient->publishString(sensorTopic, sensorData, lopcore::mqtt::MqttQos::AT_LEAST_ONCE,
                                      false) == ESP_OK)
        {
            LOPCORE_LOGI(TAG, "Published sensor data: %s", sensorData);
        }

        // Show statistics periodically
        if (counter % 3 == 0)
        {
            auto stats = mqttClient->getStatistics();
            LOPCORE_LOGI(TAG, "Stats: %lu sent, %lu received", stats.messagesPublished,
                         stats.messagesReceived);
        }
    }

    // ========================================================================
    // 10. Cleanup (unreachable)
    // ========================================================================

    mqttClient->disconnect();
    tlsTransport->disconnect();
}
