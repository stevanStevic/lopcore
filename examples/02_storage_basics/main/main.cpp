/**
 * @file main.cpp
 * @brief Storage basics example for LopCore
 *
 * Demonstrates:
 * - Direct storage construction with complete configuration
 * - Utilizing ALL config settings (no waste)
 * - NVS storage: namespace and readOnly settings
 * - SPIFFS storage: basePath, partitionLabel, maxFiles, formatIfFailed
 * - Read/write/exists operations
 * - Type-safe configuration with builder pattern
 * - Proper configuration storage as member variables
 */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lopcore/logging/console_sink.hpp"
#include "lopcore/logging/logger.hpp"
#include "lopcore/storage/nvs_storage.hpp"
#include "lopcore/storage/spiffs_storage.hpp"
#include "lopcore/storage/storage_config.hpp"

static const char *TAG = "STORAGE_EXAMPLE";

extern "C" void app_main(void)
{
    // Initialize logger
    auto &logger = lopcore::Logger::getInstance();
    logger.addSink(std::make_unique<lopcore::ConsoleSink>());
    logger.setGlobalLevel(lopcore::LogLevel::INFO);

    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "LopCore Storage Basics Example");
    LOPCORE_LOGI(TAG, "===========================================");
    LOPCORE_LOGI(TAG, "Note: NVS and SPIFFS are automatically initialized by storage classes");
    LOPCORE_LOGI(TAG, "      using configuration provided in constructors");

    // =========================================================================
    // NVS Storage Example (for configuration)
    // =========================================================================

    LOPCORE_LOGI(TAG, "\n--- NVS Storage Example ---");

    // Create NVS storage with explicit configuration using builder pattern
    // Demonstrates utilizing all config settings:
    // - namespace: logical grouping of keys
    // - readOnly: false to allow writes
    auto nvsConfig = lopcore::storage::NvsConfig().setNamespace("app_config").setReadOnly(false);

    LOPCORE_LOGI(TAG, "Creating NVS storage:");
    LOPCORE_LOGI(TAG, "  Namespace: %s", nvsConfig.namespaceName.c_str());
    LOPCORE_LOGI(TAG, "  Read-only: %s", nvsConfig.readOnly ? "true" : "false");

    lopcore::NvsStorage nvsStorage(nvsConfig);

    // Write configuration
    LOPCORE_LOGI(TAG, "Writing configuration to NVS...");
    nvsStorage.write("wifi_ssid", "MyNetwork");
    nvsStorage.write("wifi_pass", "SecurePassword123");
    nvsStorage.write("mqtt_broker", "mqtt.example.com");
    nvsStorage.write("device_name", "ESP32-Device-001");

    // Read configuration
    LOPCORE_LOGI(TAG, "Reading configuration from NVS...");
    auto ssid = nvsStorage.read("wifi_ssid");
    auto broker = nvsStorage.read("mqtt_broker");
    auto name = nvsStorage.read("device_name");

    if (ssid && broker && name)
    {
        LOPCORE_LOGI(TAG, "WiFi SSID: %s", ssid->c_str());
        LOPCORE_LOGI(TAG, "MQTT Broker: %s", broker->c_str());
        LOPCORE_LOGI(TAG, "Device Name: %s", name->c_str());
    }

    // Check if key exists
    if (nvsStorage.exists("wifi_ssid"))
    {
        LOPCORE_LOGI(TAG, "WiFi credentials found in NVS");
    }

    // =========================================================================
    // SPIFFS Storage Example (for files)
    // =========================================================================

    LOPCORE_LOGI(TAG, "\n--- SPIFFS Storage Example ---");

    // Create SPIFFS storage with explicit configuration using builder pattern
    // Demonstrates utilizing ALL config settings:
    // - basePath: mount point for the filesystem
    // - partitionLabel: specific partition to use (NULL for default)
    // - maxFiles: maximum number of simultaneously open files
    // - formatIfFailed: auto-format if mount fails (useful for first boot)
    auto spiffsConfig = lopcore::storage::SpiffsConfig()
                            .setBasePath("/spiffs")
                            .setPartitionLabel("storage")  // Using specific partition
                            .setMaxFiles(5)
                            .setFormatIfFailed(true);

    LOPCORE_LOGI(TAG, "Creating SPIFFS storage with full config:");
    LOPCORE_LOGI(TAG, "  Base path: %s", spiffsConfig.basePath.c_str());
    LOPCORE_LOGI(TAG, "  Partition label: %s", 
                 spiffsConfig.partitionLabel.empty() ? "(default)" : spiffsConfig.partitionLabel.c_str());
    LOPCORE_LOGI(TAG, "  Max files: %d", spiffsConfig.maxFiles);
    LOPCORE_LOGI(TAG, "  Format if failed: %s", spiffsConfig.formatIfFailed ? "true" : "false");

    lopcore::SpiffsStorage spiffsStorage(spiffsConfig);

    // Write a configuration file
    LOPCORE_LOGI(TAG, "Writing JSON config file...");
    std::string jsonConfig = R"({
    "version": "1.0.0",
    "mode": "auto",
    "interval": 60,
    "enabled": true
})";

    if (spiffsStorage.write("config.json", jsonConfig))
    {
        LOPCORE_LOGI(TAG, "Config file written successfully");
    }

    // Read the file back
    auto configData = spiffsStorage.read("config.json");
    if (configData)
    {
        LOPCORE_LOGI(TAG, "Read config file:");
        LOPCORE_LOGI(TAG, "%s", configData->c_str());
    }

    // Write binary data (e.g., certificate)
    LOPCORE_LOGI(TAG, "\nWriting binary certificate...");
    std::vector<uint8_t> certData = {'C', 'E', 'R', 'T', 0x00, 0x01, 0x02, 0x03};
    if (spiffsStorage.write("cert.der", certData))
    {
        LOPCORE_LOGI(TAG, "Certificate written successfully");
    }

    // Read binary data back
    auto readCert = spiffsStorage.read("cert.der");
    if (readCert)
    {
        LOPCORE_LOGI(TAG, "Read certificate: %zu bytes", readCert->size());
    }

    // List all files
    LOPCORE_LOGI(TAG, "\nListing files in SPIFFS:");
    auto files = spiffsStorage.listKeys();
    for (const auto &file : files)
    {
        LOPCORE_LOGI(TAG, "  - %s", file.c_str());
    }

    // Check storage usage
    size_t totalBytes = spiffsStorage.getTotalSize();
    size_t usedBytes = spiffsStorage.getUsedSize();
    size_t freeBytes = totalBytes - usedBytes;

    LOPCORE_LOGI(TAG, "\nStorage usage:");
    LOPCORE_LOGI(TAG, "  Total: %zu bytes", totalBytes);
    LOPCORE_LOGI(TAG, "  Used:  %zu bytes (%d%%)", usedBytes, (int) ((usedBytes * 100) / totalBytes));
    LOPCORE_LOGI(TAG, "  Free:  %zu bytes", freeBytes);

    // Delete a file
    LOPCORE_LOGI(TAG, "\nDeleting test file...");
    if (spiffsStorage.remove("config.json"))
    {
        LOPCORE_LOGI(TAG, "File deleted successfully");
    }

    LOPCORE_LOGI(TAG, "\n===========================================");
    LOPCORE_LOGI(TAG, "Storage example completed!");
    LOPCORE_LOGI(TAG, "===========================================");
}
