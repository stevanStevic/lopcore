/**
 * @file storage_factory.cpp
 * @brief Storage factory implementation
 */

#include "lopcore/storage/storage_factory.hpp"

#include "lopcore/storage/nvs_storage.hpp"
#include "lopcore/storage/spiffs_storage.hpp"

#ifdef ESP_PLATFORM
#include <esp_log.h>
#else
#include <iostream>
#define ESP_LOGE(tag, format, ...) std::cerr << "[ERROR] " << tag << ": " << format << std::endl
#endif

static const char *TAG = "StorageFactory";

namespace lopcore
{

std::unique_ptr<IStorage> StorageFactory::create(StorageType type)
{
    switch (type)
    {
        case StorageType::SPIFFS:
            return createSpiffs();

        case StorageType::NVS:
            return createNvs();

        case StorageType::SD_CARD:
            return createSdCard();

        default:
            ESP_LOGE(TAG, "Unknown storage type: %d", static_cast<int>(type));
            return nullptr;
    }
}

std::unique_ptr<IStorage> StorageFactory::createSpiffs(const std::string &basePath)
{
    return std::make_unique<SpiffsStorage>(basePath);
}

std::unique_ptr<IStorage> StorageFactory::createNvs(const std::string &namespaceName)
{
    return std::make_unique<NvsStorage>(namespaceName);
}

std::unique_ptr<IStorage> StorageFactory::createSdCard(const std::string &basePath)
{
    // SD card storage not yet implemented
    (void) basePath; // Suppress unused warning
    ESP_LOGE(TAG, "SD card storage not yet implemented");
    return nullptr;
}

} // namespace lopcore
