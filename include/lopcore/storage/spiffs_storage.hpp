/**
 * @file spiffs_storage.hpp
 * @brief SPIFFS storage implementation for LopCore
 *
 * Wraps ESP-IDF SPIFFS APIs to provide IStorage interface.
 * SPIFFS is best suited for:
 * - Log files
 * - Configuration files
 * - Certificates and keys
 * - Small to medium sized files (<100KB typically)
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#pragma once

#include <mutex>
#include <string>

#include "istorage.hpp"
#include "storage_config.hpp"

namespace lopcore
{

/**
 * @brief SPIFFS implementation of IStorage interface
 *
 * Provides file-based storage using ESP-IDF's SPIFFS (SPI Flash File System).
 * Thread-safe: All operations are protected by mutex.
 *
 * SPIFFS Characteristics:
 * - Wear leveling built-in
 * - No directories (flat namespace)
 * - Good for sequential reads/writes
 * - Slower than NVS for small key-value pairs
 * - Can store larger files than NVS
 *
 * Example:
 * @code
 * storage::SpiffsConfig config;
 * config.setBasePath("/spiffs").setFormatIfFailed(true);
 * SpiffsStorage storage(config);
 * storage.writeString("app.log", "Application started");
 * std::string log;
 * storage.readString("app.log", log);
 * @endcode
 */
class SpiffsStorage
{
public:
    /**
     * @brief Construct SPIFFS storage with configuration (RECOMMENDED)
     *
     * @param config SpiffsConfig with all settings
     *
     * Example:
     * @code
     * SpiffsConfig config;
     * config.setBasePath("/spiffs")
     *       .setMaxFiles(10)
     *       .setFormatIfFailed(true);
     * SpiffsStorage storage(config);
     * @endcode
     */
    explicit SpiffsStorage(const storage::SpiffsConfig &config);

    /**
     * @brief Construct SPIFFS storage with base path (DEPRECATED - use config constructor)
     *
     * @param basePath Mount point for SPIFFS (default: "/spiffs")
     *
     * @deprecated Use SpiffsStorage(const SpiffsConfig&) instead
     * @note Automatically initializes SPIFFS if not already initialized
     * @note Will format SPIFFS if mount fails and format_if_mount_failed is true
     */
    [[deprecated("Use SpiffsStorage(const SpiffsConfig&) constructor instead")]] explicit SpiffsStorage(
        const std::string &basePath = "/spiffs");

    /**
     * @brief Destructor - unmounts SPIFFS if this was the initializer
     */
    ~SpiffsStorage();

    // Storage interface methods
    bool write(const std::string &key, const std::string &data);
    bool write(const std::string &key, const std::vector<uint8_t> &data);
    std::optional<std::string> read(const std::string &key);
    std::optional<std::vector<uint8_t>> readBinary(const std::string &key);
    bool exists(const std::string &key);
    std::vector<std::string> listKeys();
    bool remove(const std::string &key);
    size_t getTotalSize() const;
    size_t getUsedSize() const;
    size_t getFreeSize() const;
    StorageType getType() const
    {
        return StorageType::SPIFFS;
    }

    // SPIFFS-specific operations

    /**
     * @brief Format the SPIFFS partition
     *
     * @return true if format succeeded, false otherwise
     *
     * @warning This erases all data on the partition!
     */
    bool format();

    /**
     * @brief Check SPIFFS filesystem integrity
     *
     * @return true if filesystem is healthy, false otherwise
     *
     * @note Only available on ESP-IDF >= 4.4.0
     */
    bool check();

    /**
     * @brief Get the base path for this storage
     *
     * @return Base path string (e.g., "/spiffs")
     */
    const std::string &getBasePath() const
    {
        return basePath_;
    }

private:
    std::string basePath_;     ///< Base path for SPIFFS mount point
    bool initialized_;         ///< Whether SPIFFS was initialized by this instance
    mutable std::mutex mutex_; ///< Mutex for thread safety

    /**
     * @brief Get full file path from key
     *
     * @param key Relative key (e.g., "config.json")
     * @return Full path (e.g., "/spiffs/config.json")
     */
    std::string getFullPath(const std::string &key) const;

    /**
     * @brief Initialize SPIFFS if not already initialized
     *
     * @return true if initialization succeeded, false otherwise
     */
    bool initialize();

    /**
     * @brief Check if SPIFFS is already mounted
     *
     * @return true if mounted, false otherwise
     */
    bool isMounted() const;
};

} // namespace lopcore
