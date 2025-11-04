/**
 * @file storage_config.hpp
 * @brief Configuration structures for storage backends
 *
 * Provides type-safe configuration with builder pattern for explicit
 * storage construction without factory pattern.
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#pragma once

#include <cstddef>
#include <string>

namespace lopcore
{
namespace storage
{

/**
 * @brief SPIFFS filesystem configuration
 *
 * Configuration for SPIFFS storage backend. Use builder pattern
 * for convenient setup:
 *
 * @code
 * SpiffsConfig config;
 * config.setBasePath("/spiffs")
 *       .setMaxFiles(10)
 *       .setFormatIfFailed(true);
 *
 * SpiffsStorage storage(config);
 * @endcode
 */
struct SpiffsConfig
{
    std::string basePath = "/spiffs";
    std::string partitionLabel = "storage";
    size_t maxFiles = 5;
    bool formatIfFailed = false;

    /**
     * @brief Set SPIFFS mount point path
     *
     * @param path Mount point (e.g., "/spiffs")
     * @return Reference to this config for chaining
     */
    SpiffsConfig &setBasePath(const std::string &path)
    {
        basePath = path;
        return *this;
    }

    /**
     * @brief Set partition label
     *
     * @param label Partition label from partition table
     * @return Reference to this config for chaining
     */
    SpiffsConfig &setPartitionLabel(const std::string &label)
    {
        partitionLabel = label;
        return *this;
    }

    /**
     * @brief Set maximum number of open files
     *
     * @param max Maximum simultaneous open files
     * @return Reference to this config for chaining
     */
    SpiffsConfig &setMaxFiles(size_t max)
    {
        maxFiles = max;
        return *this;
    }

    /**
     * @brief Enable automatic format on mount failure
     *
     * @param format True to format if mount fails
     * @return Reference to this config for chaining
     */
    SpiffsConfig &setFormatIfFailed(bool format)
    {
        formatIfFailed = format;
        return *this;
    }
};

/**
 * @brief NVS (Non-Volatile Storage) configuration
 *
 * Configuration for NVS storage backend. Use builder pattern
 * for convenient setup:
 *
 * @code
 * NvsConfig config;
 * config.setNamespace("my_app")
 *       .setReadOnly(false);
 *
 * NvsStorage storage(config);
 * @endcode
 */
struct NvsConfig
{
    std::string namespaceName = "lopcore";
    bool readOnly = false;

    /**
     * @brief Set NVS namespace
     *
     * @param ns Namespace name (max 15 characters)
     * @return Reference to this config for chaining
     */
    NvsConfig &setNamespace(const std::string &ns)
    {
        namespaceName = ns;
        return *this;
    }

    /**
     * @brief Set read-only mode
     *
     * @param ro True for read-only access
     * @return Reference to this config for chaining
     */
    NvsConfig &setReadOnly(bool ro)
    {
        readOnly = ro;
        return *this;
    }
};

} // namespace storage
} // namespace lopcore
