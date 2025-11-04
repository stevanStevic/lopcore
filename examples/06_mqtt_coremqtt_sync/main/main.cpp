/**
 * @file main.cpp
 * @brief CoreMQTT Sync Client Example
 *
 * Demonstrates basic MQTT request-response pattern using manual processLoop:
 * - Manual control of message processing
 * - Request-response transactions
 * - Topic-based command handling
 * - Synchronous publish-subscribe workflow
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

static const char *TAG = "mqtt_coremqtt_sync";

// ============================================================================
// Configuration
// ============================================================================

const char *brokerHost = "test.mosquitto.org";
const uint16_t brokerPort = 8883;
const char *clientId = "lopcore_coremqtt_sync";

// Request-Response Topics
const char *requestTopic = "lopcore/rpc/request";
const char *responseTopic = "lopcore/rpc/response";

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
// Response Tracking
// ============================================================================

struct ResponseState
{
    bool received = false;
    std::string payload;
    TickType_t timestamp = 0;

    void reset()
    {
        received = false;
        payload.clear();
        timestamp = 0;
    }
};

static ResponseState g_response;

// ============================================================================
// Message Callbacks
// ============================================================================
// Message Callbacks
// ============================================================================

void onResponseReceived(const lopcore::mqtt::MqttMessage &message)
{
    g_response.payload = std::string(message.payload.begin(), message.payload.end());
    g_response.received = true;
    g_response.timestamp = xTaskGetTickCount();

    LOPCORE_LOGI(TAG, "Response received: %s", g_response.payload.c_str());
}

void onRequestReceived(const lopcore::mqtt::MqttMessage &message)
{
    std::string request(message.payload.begin(), message.payload.end());
    LOPCORE_LOGI(TAG, "Request received: %s", request.c_str());

    // Example: Handle different request types
    if (request == "ping")
    {
        LOPCORE_LOGI(TAG, "  → Responding with 'pong'");
    }
    else if (request.find("echo:") == 0)
    {
        LOPCORE_LOGI(TAG, "  → Echo request");
    }
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
// Helper Functions
// ============================================================================

/**
 * Subscribe and wait for acknowledgment using processLoop
 */
esp_err_t subscribeWithProcessing(lopcore::mqtt::CoreMqttClient *client,
                                  const char *topic,
                                  lopcore::mqtt::MessageCallback callback,
                                  uint32_t timeoutMs = 5000)
{
    LOPCORE_LOGI(TAG, "Subscribing to: %s", topic);

    if (client->subscribe(topic, callback, lopcore::mqtt::MqttQos::AT_LEAST_ONCE) != ESP_OK)
    {
        return ESP_FAIL;
    }

    // Process SUBACK with manual loop
    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeoutMs))
    {
        if (client->processLoop(200) != ESP_OK)
        {
            LOPCORE_LOGW(TAG, "processLoop failed during subscribe");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    LOPCORE_LOGI(TAG, "Subscribed to: %s", topic);
    return ESP_OK;
}

/**
 * Publish with manual processing
 */
esp_err_t publishWithProcessing(lopcore::mqtt::CoreMqttClient *client,
                                const char *topic,
                                const char *message,
                                uint32_t timeoutMs = 3000)
{
    LOPCORE_LOGI(TAG, "Publishing to %s: %s", topic, message);

    if (client->publishString(topic, message, lopcore::mqtt::MqttQos::AT_LEAST_ONCE, false) != ESP_OK)
    {
        return ESP_FAIL;
    }

    // Process PUBACK
    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeoutMs))
    {
        if (client->processLoop(200) != ESP_OK)
        {
            LOPCORE_LOGW(TAG, "processLoop failed during publish");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    return ESP_OK;
}

/**
 * Wait for response using processLoop
 */
bool waitForResponse(lopcore::mqtt::CoreMqttClient *client, uint32_t timeoutMs = 10000)
{
    LOPCORE_LOGI(TAG, "Waiting for response (timeout: %lu ms)...", timeoutMs);

    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeoutMs))
    {
        if (g_response.received)
        {
            return true;
        }

        if (client->processLoop(200) != ESP_OK)
        {
            LOPCORE_LOGW(TAG, "processLoop failed while waiting for response");
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    LOPCORE_LOGW(TAG, "⏱ Response timeout");
    return false;
}

// ============================================================================
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
    LOPCORE_LOGI(TAG, "CoreMQTT Sync Client Example");
    LOPCORE_LOGI(TAG, "Request-Response Pattern with Manual Loop");
    LOPCORE_LOGI(TAG, "===========================================");

    // Note: This example assumes WiFi is already connected

    // ========================================================================
    // 1. Configure TLS with Certificate from SPIFFS
    // ========================================================================

    LOPCORE_LOGI(TAG, "Configuring TLS...");

    // Certificate is stored in SPIFFS at /spiffs/root_ca.crt
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
    // 3. Create MQTT Client
    // ========================================================================

    auto mqttConfig = lopcore::mqtt::MqttConfig::builder()
                          .clientId(clientId)
                          .keepAlive(std::chrono::seconds(60))
                          .cleanSession(true)
                          .build();

    auto mqttClient = std::make_unique<lopcore::mqtt::CoreMqttClient>(mqttConfig, tlsTransport);

    mqttClient->setConnectionCallback([](bool connected) {
        if (connected)
        {
            LOPCORE_LOGI(TAG, "MQTT connected");
        }
        else
        {
            LOPCORE_LOGW(TAG, "MQTT disconnected");
        }
    });

    // ========================================================================
    // 4. Connect with Manual Processing
    // ========================================================================

    LOPCORE_LOGI(TAG, "Connecting to broker...");

    if (mqttClient->connect() != ESP_OK)
    {
        LOPCORE_LOGE(TAG, "Failed to connect");
        return;
    }

    // Process CONNACK manually
    for (int i = 0; i < 10; i++)
    {
        if (mqttClient->isConnected())
        {
            break;
        }
        mqttClient->processLoop(200);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (!mqttClient->isConnected())
    {
        LOPCORE_LOGE(TAG, "Connection timeout");
        return;
    }

    // ========================================================================
    // 5. Subscribe to Topics
    // ========================================================================

    subscribeWithProcessing(mqttClient.get(), responseTopic, onResponseReceived);
    subscribeWithProcessing(mqttClient.get(), requestTopic, onRequestReceived);

    vTaskDelay(pdMS_TO_TICKS(1000));

    // ========================================================================
    // 6. Request-Response Transaction Examples
    // ========================================================================

    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "Starting Request-Response Examples");
    LOPCORE_LOGI(TAG, "===========================================");

    // Example 1: Simple ping request
    LOPCORE_LOGI(TAG, "\n--- Example 1: Ping Request ---");
    g_response.reset();
    publishWithProcessing(mqttClient.get(), requestTopic, "ping");

    if (waitForResponse(mqttClient.get(), 10000))
    {
        LOPCORE_LOGI(TAG, "Transaction complete: %s", g_response.payload.c_str());
    }

    vTaskDelay(pdMS_TO_TICKS(2000));

    // Example 2: Echo request
    LOPCORE_LOGI(TAG, "\n--- Example 2: Echo Request ---");
    g_response.reset();
    publishWithProcessing(mqttClient.get(), requestTopic, "echo:Hello from CoreMQTT!");

    if (waitForResponse(mqttClient.get(), 10000))
    {
        LOPCORE_LOGI(TAG, "Transaction complete: %s", g_response.payload.c_str());
    }

    vTaskDelay(pdMS_TO_TICKS(2000));

    // Example 3: Info request
    LOPCORE_LOGI(TAG, "\n--- Example 3: Device Info Request ---");
    g_response.reset();
    publishWithProcessing(mqttClient.get(), requestTopic, "info");

    if (waitForResponse(mqttClient.get(), 10000))
    {
        LOPCORE_LOGI(TAG, "Transaction complete: %s", g_response.payload.c_str());
    }

    // ========================================================================
    // 7. Periodic Status Publishing
    // ========================================================================

    LOPCORE_LOGI(TAG, "\n===========================================");
    LOPCORE_LOGI(TAG, "Periodic Status Loop");
    LOPCORE_LOGI(TAG, "===========================================");

    int counter = 0;
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(15000));

        if (!mqttClient->isConnected())
        {
            LOPCORE_LOGW(TAG, "Not connected, waiting...");
            continue;
        }

        // Build status message
        char status[128];
        snprintf(status, sizeof(status), "{\"counter\":%d,\"uptime\":%llu,\"heap\":%lu}", counter++,
                 esp_timer_get_time() / 1000000, esp_get_free_heap_size());

        // Publish status
        publishWithProcessing(mqttClient.get(), responseTopic, status);

        // Keep processing messages
        mqttClient->processLoop(200);

        // Show statistics periodically
        if (counter % 5 == 0)
        {
            auto stats = mqttClient->getStatistics();
            LOPCORE_LOGI(TAG, "Stats: %lu sent, %lu received", stats.messagesPublished,
                         stats.messagesReceived);
        }
    }

    // ========================================================================
    // 8. Cleanup (unreachable)
    // ========================================================================

    mqttClient->disconnect();
    tlsTransport->disconnect();
}
