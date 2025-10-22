/**
 * @file storage_factory.hpp
 * @brief Factory for creating storage instances
 *
 * Provides a centralized way to create storage backends using the Factory pattern.
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <memory>

#include "istorage.hpp"

namespace lopcore
{

/**
 * @brief Factory class for creating storage instances
 *
 * Uses the Factory pattern to create appropriate storage backends.
 * Simplifies storage creation and allows for easy mocking in tests.
 *
 * Example:
 * @code
 * auto spiffs = StorageFactory::createSpiffs("/spiffs");
 * auto nvs = StorageFactory::createNvs("app_config");
 * auto storage = StorageFactory::create(StorageType::SPIFFS);
 * @endcode
 */
class StorageFactory
{
public:
    /**
     * @brief Create storage instance by type
     *
     * @param type Storage backend type
     * @return Unique pointer to IStorage implementation
     *
     * @note Uses default parameters for each backend
     */
    static std::unique_ptr<IStorage> create(StorageType type);

    /**
     * @brief Create SPIFFS storage with custom base path
     *
     * @param basePath Mount point for SPIFFS (default: "/spiffs")
     * @return Unique pointer to SpiffsStorage
     */
    static std::unique_ptr<IStorage> createSpiffs(const std::string &basePath = "/spiffs");

    /**
     * @brief Create NVS storage with custom namespace
     *
     * @param namespaceName NVS namespace (default: "lopcore")
     * @return Unique pointer to NvsStorage
     */
    static std::unique_ptr<IStorage> createNvs(const std::string &namespaceName = "lopcore");

    /**
     * @brief Create SD card storage (future implementation)
     *
     * @param basePath Mount point for SD card (default: "/sdcard")
     * @return Unique pointer to SdCardStorage
     *
     * @note Not yet implemented - returns nullptr
     */
    static std::unique_ptr<IStorage> createSdCard(const std::string &basePath = "/sdcard");

private:
    // Private constructor - factory class should not be instantiated
    StorageFactory() = delete;
};

} // namespace lopcore
