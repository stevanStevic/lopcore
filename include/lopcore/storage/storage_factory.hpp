/**
 * @file storage_factory.hpp
 * @brief Factory for creating storage instances (DEPRECATED)
 *
 * @deprecated Use direct construction with configuration structs instead.
 * This factory pattern adds unnecessary indirection and virtual function overhead.
 *
 * Old way (deprecated):
 * @code
 * auto storage = StorageFactory::createSpiffs("/spiffs");  // Don't use
 * @endcode
 *
 * New way (recommended):
 * @code
 * SpiffsConfig config;
 * config.setBasePath("/spiffs").setFormatIfFailed(true);
 * SpiffsStorage storage(config);  // Direct construction
 * @endcode
 *
 * @see storage_config.hpp for configuration structures
 * @see storage_traits.hpp for compile-time capability detection
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#pragma once

#include <memory>

#include "istorage.hpp"

namespace lopcore
{

/**
 * @brief Factory class for creating storage instances (DEPRECATED)
 *
 * @deprecated Use direct construction with SpiffsConfig/NvsConfig instead.
 * The factory pattern is being phased out in favor of:
 * - Direct construction with explicit configuration
 * - Compile-time trait-based generic algorithms
 * - Zero virtual function overhead
 *
 * Migration guide:
 * @code
 * // OLD (deprecated):
 * auto spiffs = StorageFactory::createSpiffs("/spiffs");
 * auto nvs = StorageFactory::createNvs("app_config");
 *
 * // NEW (recommended):
 * SpiffsConfig spiffsConf;
 * spiffsConf.setBasePath("/spiffs");
 * SpiffsStorage spiffs(spiffsConf);
 *
 * NvsConfig nvsConf;
 * nvsConf.setNamespace("app_config");
 * NvsStorage nvs(nvsConf);
 * @endcode
 *
 * This class will be removed in a future release.
 */
class [[deprecated("Use direct construction with SpiffsConfig/NvsConfig instead")]] StorageFactory
{
public:
    /**
     * @brief Create storage instance by type
     *
     * @deprecated Use direct construction with config structs instead
     * @param type Storage backend type
     * @return Unique pointer to IStorage implementation
     *
     * @note Uses default parameters for each backend
     */
    [[deprecated("Use direct construction with SpiffsConfig/NvsConfig")]] static std::unique_ptr<IStorage> create(
        StorageType type);

    /**
     * @brief Create SPIFFS storage with custom base path
     *
     * @deprecated Use SpiffsStorage(SpiffsConfig) constructor instead
     * @param basePath Mount point for SPIFFS (default: "/spiffs")
     * @return Unique pointer to SpiffsStorage
     */
    [[deprecated("Use SpiffsStorage(SpiffsConfig) constructor")]] static std::unique_ptr<IStorage> createSpiffs(
        const std::string &basePath = "/spiffs");

    /**
     * @brief Create NVS storage with custom namespace
     *
     * @deprecated Use NvsStorage(NvsConfig) constructor instead
     * @param namespaceName NVS namespace (default: "lopcore")
     * @return Unique pointer to NvsStorage
     */
    [[deprecated("Use NvsStorage(NvsConfig) constructor")]] static std::unique_ptr<IStorage> createNvs(
        const std::string &namespaceName = "lopcore");

    /**
     * @brief Create SD card storage (future implementation)
     *
     * @deprecated Not implemented, will be removed
     * @param basePath Mount point for SD card (default: "/sdcard")
     * @return Unique pointer to SdCardStorage
     *
     * @note Not yet implemented - returns nullptr
     */
    [[deprecated("Not implemented")]] static std::unique_ptr<IStorage> createSdCard(
        const std::string &basePath = "/sdcard");

private:
    // Private constructor - factory class should not be instantiated
    StorageFactory() = delete;
};

} // namespace lopcore
