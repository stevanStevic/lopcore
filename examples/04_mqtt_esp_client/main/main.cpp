/**
 * @file main.cpp
 * @brief ESP-MQTT Client Example
 *
 * Demonstrates basic MQTT communication using ESP-MQTT client:
 * - Connecting to a public MQTT broker
 * - Subscribing to topics
 * - Publishing messages
 * - Handling incoming messages
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lopcore/logging/console_sink.hpp"
#include "lopcore/logging/logger.hpp"
#include "lopcore/mqtt/esp_mqtt_client.hpp"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "mqtt_esp_example";

// ============================================================================
// Configuration
// ============================================================================

// MQTT Broker Configuration
// Using public test broker - replace with your own for production
const char *brokerUri = "mqtt://test.mosquitto.org:1883";
const char *clientId = "esp32_lopcore_demo";

// Topics for demonstration
const char *pubTopic = "lopcore/demo/status";
const char *subTopic = "lopcore/demo/commands";

// ============================================================================
// ============================================================================
// Message Callback
// ============================================================================

void onMessageReceived(const lopcore::mqtt::MqttMessage &message)
{
    LOPCORE_LOGI(TAG, "  Message received:");
    LOPCORE_LOGI(TAG, "  Topic: %s", message.topic.c_str());
    LOPCORE_LOGI(TAG, "  QoS: %d", static_cast<int>(message.qos));
    LOPCORE_LOGI(TAG, "  Payload: %.*s", (int) message.payload.size(), message.payload.data());

    // Example: Handle simple commands
    std::string msg(message.payload.begin(), message.payload.end());
    if (msg == "ping")
    {
        LOPCORE_LOGI(TAG, "Received ping command!");
    }
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

    // Initialize logger
    auto &logger = lopcore::Logger::getInstance();
    logger.addSink(std::make_unique<lopcore::ConsoleSink>());
    logger.setGlobalLevel(lopcore::LogLevel::INFO);

    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "ESP-MQTT Client Example");
    LOPCORE_LOGI(TAG, "===========================================");

    // Note: This example assumes WiFi is already connected
    // In a real application, initialize and connect WiFi first

    // ========================================================================
    // 1. Create MQTT Configuration
    // ========================================================================

    auto mqttConfig = lopcore::mqtt::MqttConfig::builder()
                          .broker(brokerUri)
                          .clientId(clientId)
                          .keepAlive(std::chrono::seconds(60))
                          .cleanSession(true)
                          .build();

    LOPCORE_LOGI(TAG, "Configuration:");
    LOPCORE_LOGI(TAG, "  Broker: %s", brokerUri);
    LOPCORE_LOGI(TAG, "  Client ID: %s", clientId);

    // ========================================================================
    // 2. Create ESP-MQTT Client
    // ========================================================================

    auto mqttClient = std::make_unique<lopcore::mqtt::EspMqttClient>(mqttConfig);

    // Set connection callback
    mqttClient->setConnectionCallback([](bool connected) {
        if (connected)
        {
            LOPCORE_LOGI(TAG, "✓ Connected to broker");
        }
        else
        {
            LOPCORE_LOGW(TAG, "✗ Disconnected from broker");
        }
    });

    // Set error callback
    mqttClient->setErrorCallback([](lopcore::mqtt::MqttError error, const std::string &message) {
        LOPCORE_LOGE(TAG, "Error: %s", message.c_str());
    });

    // ========================================================================
    // 3. Connect to Broker
    // ========================================================================

    LOPCORE_LOGI(TAG, "Connecting to broker...");
    if (mqttClient->connect() != ESP_OK)
    {
        LOPCORE_LOGE(TAG, "Failed to initiate connection");
        return;
    }

    // Wait for connection
    vTaskDelay(pdMS_TO_TICKS(3000));

    if (!mqttClient->isConnected())
    {
        LOPCORE_LOGE(TAG, "Connection timeout - check WiFi and broker");
        return;
    }

    // ========================================================================
    // 4. Subscribe to Topic
    // ========================================================================

    LOPCORE_LOGI(TAG, "Subscribing to: %s", subTopic);

    if (mqttClient->subscribe(subTopic, onMessageReceived, lopcore::mqtt::MqttQos::AT_LEAST_ONCE) == ESP_OK)
    {
        LOPCORE_LOGI(TAG, "Subscribed successfully");
        LOPCORE_LOGI(TAG, "Try publishing to this topic from another device:");
        LOPCORE_LOGI(TAG, "  mosquitto_pub -h test.mosquitto.org -t %s -m \"ping\"", subTopic);
    }

    // ========================================================================
    // 5. Publish Initial Status
    // ========================================================================

    vTaskDelay(pdMS_TO_TICKS(1000));

    const char *statusMsg = "{\"status\":\"online\",\"device\":\"esp32\"}";
    if (mqttClient->publishString(pubTopic, statusMsg, lopcore::mqtt::MqttQos::AT_LEAST_ONCE, false) ==
        ESP_OK)
    {
        LOPCORE_LOGI(TAG, "Published status to: %s", pubTopic);
    }

    // ========================================================================
    // 6. Periodic Publishing Loop
    // ========================================================================

    LOPCORE_LOGI(TAG, "Starting periodic publishing (every 15 seconds)...");
    LOPCORE_LOGI(TAG, "Monitor with: mosquitto_sub -h test.mosquitto.org -t lopcore/demo/#");

    int counter = 0;
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(15000));

        if (!mqttClient->isConnected())
        {
            LOPCORE_LOGW(TAG, "Not connected, waiting for reconnection...");
            continue;
        }

        // Build telemetry message
        char telemetry[128];
        snprintf(telemetry, sizeof(telemetry), "{\"counter\":%d,\"uptime\":%llu,\"heap\":%lu}", counter++,
                 esp_timer_get_time() / 1000000, esp_get_free_heap_size());

        // Publish telemetry
        if (mqttClient->publishString(pubTopic, telemetry, lopcore::mqtt::MqttQos::AT_LEAST_ONCE, false) ==
            ESP_OK)
        {
            LOPCORE_LOGI(TAG, "Published: %s", telemetry);
        }

        // Show statistics periodically
        if (counter % 5 == 0)
        {
            auto stats = mqttClient->getStatistics();
            LOPCORE_LOGI(TAG, "Stats: %lu sent, %lu received", (unsigned long) stats.messagesPublished,
                         (unsigned long) stats.messagesReceived);
        }
    }
}
