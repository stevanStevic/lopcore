/**
 * @file littlefs_storage.hpp
 * @brief LittleFS storage implementation
 *
 * Provides file-based storage using LittleFS filesystem.
 * LittleFS is designed for microcontrollers with wear leveling and power-loss resilience.
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "storage_config.hpp"
#include "storage_type.hpp"

namespace lopcore
{

/**
 * @brief LittleFS storage implementation
 *
 * File-based storage using LittleFS filesystem optimized for flash memory.
 * Features include:
 * - Wear leveling
 * - Power-loss resilience
 * - Dynamic wear leveling
 * - Small RAM/ROM footprint
 *
 * Example usage:
 * @code
 * auto config = storage::LittleFsConfig()
 *     .setBasePath("/littlefs")
 *     .setPartitionLabel("littlefs")
 *     .setFormatIfFailed(true)
 *     .setGrowOnMount(true);
 *
 * LittleFsStorage storage(config);
 * storage.write("data.bin", binaryData);
 * auto data = storage.readBinary("data.bin");
 * @endcode
 */
class LittleFsStorage
{
public:
    /**
     * @brief Construct LittleFS storage with configuration
     *
     * @param config LittleFS configuration
     * @throws std::runtime_error if initialization fails
     */
    explicit LittleFsStorage(const storage::LittleFsConfig &config);

    /**
     * @brief Construct with base path only (use defaults)
     *
     * @param basePath LittleFS mount point
     * @deprecated Use config-based constructor
     */
    explicit LittleFsStorage(const std::string &basePath);

    /**
     * @brief Destructor - unmounts filesystem
     */
    ~LittleFsStorage();

    // Delete copy constructor and assignment
    LittleFsStorage(const LittleFsStorage &) = delete;
    LittleFsStorage &operator=(const LittleFsStorage &) = delete;

    // Storage interface methods

    /**
     * @brief Write string data to file
     *
     * @param key File path relative to base path
     * @param data String data to write
     * @return true if write successful, false otherwise
     */
    bool write(const std::string &key, const std::string &data);

    /**
     * @brief Write binary data to file
     *
     * @param key File path relative to base path
     * @param data Binary data to write
     * @return true if write successful, false otherwise
     */
    bool write(const std::string &key, const std::vector<uint8_t> &data);

    /**
     * @brief Read string data from file
     *
     * @param key File path relative to base path
     * @return Optional containing data if successful, nullopt otherwise
     */
    std::optional<std::string> read(const std::string &key);

    /**
     * @brief Read binary data from file
     *
     * @param key File path relative to base path
     * @return Optional containing data if successful, nullopt otherwise
     */
    std::optional<std::vector<uint8_t>> readBinary(const std::string &key);

    /**
     * @brief Check if file exists
     *
     * @param key File path relative to base path
     * @return true if file exists, false otherwise
     */
    bool exists(const std::string &key);

    /**
     * @brief List all files in directory
     *
     * @param directory Directory path (empty for root)
     * @return Vector of file paths
     */
    std::vector<std::string> listKeys(const std::string &directory = "");

    /**
     * @brief Remove file
     *
     * @param key File path relative to base path
     * @return true if removal successful, false otherwise
     */
    bool remove(const std::string &key);

    /**
     * @brief Get total storage size in bytes
     *
     * @return Total size in bytes
     */
    size_t getTotalSize() const;

    /**
     * @brief Get used storage size in bytes
     *
     * @return Used size in bytes
     */
    size_t getUsedSize() const;

    /**
     * @brief Get free storage size in bytes
     *
     * @return Free size in bytes
     */
    size_t getFreeSize() const;

    /**
     * @brief Get storage type
     *
     * @return StorageType::LITTLEFS
     */
    StorageType getType() const
    {
        return StorageType::LITTLEFS;
    }

    /**
     * @brief Get base path
     *
     * @return Base path string (e.g., "/littlefs")
     */
    const std::string &getBasePath() const
    {
        return config_.basePath;
    }

    /**
     * @brief Check if filesystem is mounted
     *
     * @return true if mounted, false otherwise
     */
    bool isMounted() const
    {
        return initialized_;
    }

private:
    storage::LittleFsConfig config_; ///< LittleFS configuration
    bool initialized_;               ///< Whether LittleFS was successfully mounted
    mutable std::mutex mutex_;       ///< Mutex for thread safety

    /**
     * @brief Get full file path from key
     *
     * @param key Relative key (e.g., "data.bin")
     * @return Full path (e.g., "/littlefs/data.bin")
     */
    std::string getFullPath(const std::string &key) const;

    /**
     * @brief Initialize LittleFS if not already initialized
     *
     * @return true if initialization succeeded, false otherwise
     */
    bool initialize();

    /**
     * @brief Check if LittleFS is already mounted
     *
     * @return true if mounted, false otherwise
     */
    bool isMountedInternal() const;
};

} // namespace lopcore
