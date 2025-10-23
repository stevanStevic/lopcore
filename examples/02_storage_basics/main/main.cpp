/**
 * @file main.cpp
 * @brief Storage basics example for LopCore
 *
 * Demonstrates:
 * - NVS storage for configuration
 * - SPIFFS storage for files
 * - Storage factory pattern
 * - Read/write/exists operations
 */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lopcore/logging/console_sink.hpp"
#include "lopcore/logging/logger.hpp"
#include "lopcore/storage/storage_factory.hpp"

#include "esp_spiffs.h"
#include "esp_system.h"
#include "nvs_flash.h"

static const char *TAG = "STORAGE_EXAMPLE";

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

    // Initialize SPIFFS
    esp_vfs_spiffs_conf_t spiffs_conf = {
        .base_path = "/spiffs", .partition_label = NULL, .max_files = 5, .format_if_mount_failed = true};
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&spiffs_conf));

    // Initialize logger
    auto &logger = lopcore::Logger::getInstance();
    logger.addSink(std::make_unique<lopcore::ConsoleSink>());
    logger.setGlobalLevel(lopcore::LogLevel::INFO);

    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "LopCore Storage Basics Example");
    LOPCORE_LOGI(TAG, "===========================================");

    // =========================================================================
    // NVS Storage Example (for configuration)
    // =========================================================================

    LOPCORE_LOGI(TAG, "\n--- NVS Storage Example ---");

    auto nvsStorage = lopcore::StorageFactory::createNvs("app_config");

    // Write configuration
    LOPCORE_LOGI(TAG, "Writing configuration to NVS...");
    nvsStorage->write("wifi_ssid", "MyNetwork");
    nvsStorage->write("wifi_pass", "SecurePassword123");
    nvsStorage->write("mqtt_broker", "mqtt.example.com");
    nvsStorage->write("device_name", "ESP32-Device-001");

    // Read configuration
    LOPCORE_LOGI(TAG, "Reading configuration from NVS...");
    auto ssid = nvsStorage->read("wifi_ssid");
    auto broker = nvsStorage->read("mqtt_broker");
    auto name = nvsStorage->read("device_name");

    if (ssid && broker && name)
    {
        LOPCORE_LOGI(TAG, "WiFi SSID: %s", ssid->c_str());
        LOPCORE_LOGI(TAG, "MQTT Broker: %s", broker->c_str());
        LOPCORE_LOGI(TAG, "Device Name: %s", name->c_str());
    }

    // Check if key exists
    if (nvsStorage->exists("wifi_ssid"))
    {
        LOPCORE_LOGI(TAG, "WiFi credentials found in NVS");
    }

    // =========================================================================
    // SPIFFS Storage Example (for files)
    // =========================================================================

    LOPCORE_LOGI(TAG, "\n--- SPIFFS Storage Example ---");

    auto spiffsStorage = lopcore::StorageFactory::createSpiffs("/spiffs");

    // Write a configuration file
    LOPCORE_LOGI(TAG, "Writing JSON config file...");
    std::string jsonConfig = R"({
    "version": "1.0.0",
    "mode": "auto",
    "interval": 60,
    "enabled": true
})";

    if (spiffsStorage->write("config.json", jsonConfig))
    {
        LOPCORE_LOGI(TAG, "Config file written successfully");
    }

    // Read the file back
    auto configData = spiffsStorage->read("config.json");
    if (configData)
    {
        LOPCORE_LOGI(TAG, "Read config file:");
        LOPCORE_LOGI(TAG, "%s", configData->c_str());
    }

    // Write binary data (e.g., certificate)
    LOPCORE_LOGI(TAG, "\nWriting binary certificate...");
    std::vector<uint8_t> certData = {'C', 'E', 'R', 'T', 0x00, 0x01, 0x02, 0x03};
    if (spiffsStorage->write("cert.der", certData))
    {
        LOPCORE_LOGI(TAG, "Certificate written successfully");
    }

    // Read binary data back
    auto readCert = spiffsStorage->read("cert.der");
    if (readCert)
    {
        LOPCORE_LOGI(TAG, "Read certificate: %zu bytes", readCert->size());
    }

    // List all files
    LOPCORE_LOGI(TAG, "\nListing files in SPIFFS:");
    auto files = spiffsStorage->listKeys();
    for (const auto &file : files)
    {
        LOPCORE_LOGI(TAG, "  - %s", file.c_str());
    }

    // Check storage usage
    size_t totalBytes = spiffsStorage->getTotalSize();
    size_t usedBytes = spiffsStorage->getUsedSize();
    size_t freeBytes = totalBytes - usedBytes;

    LOPCORE_LOGI(TAG, "\nStorage usage:");
    LOPCORE_LOGI(TAG, "  Total: %zu bytes", totalBytes);
    LOPCORE_LOGI(TAG, "  Used:  %zu bytes (%d%%)", usedBytes, (int) ((usedBytes * 100) / totalBytes));
    LOPCORE_LOGI(TAG, "  Free:  %zu bytes", freeBytes);

    // Delete a file
    LOPCORE_LOGI(TAG, "\nDeleting test file...");
    if (spiffsStorage->remove("config.json") == ESP_OK)
    {
        LOPCORE_LOGI(TAG, "File deleted successfully");
    }

    LOPCORE_LOGI(TAG, "\n===========================================");
    LOPCORE_LOGI(TAG, "Storage example completed!");
    LOPCORE_LOGI(TAG, "===========================================");

    // Clean up
    esp_vfs_spiffs_unregister(NULL);
}
