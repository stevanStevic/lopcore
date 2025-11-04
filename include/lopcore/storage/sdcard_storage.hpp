/**
 * @file sdcard_storage.hpp
 * @brief SD Card storage implementation using FAT filesystem
 *
 * Provides file-based storage on SD card with FAT filesystem support.
 * Automatically handles card initialization and mounting.
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "storage_config.hpp"
#include "storage_type.hpp"

#ifdef ESP_PLATFORM
#include <driver/sdmmc_host.h>
#include <driver/sdspi_host.h>
#include <sdmmc_cmd.h>
#endif

namespace lopcore
{

/**
 * @brief SD Card storage implementation
 *
 * File-based storage using FAT filesystem on SD card.
 * Supports both SDMMC and SPI modes.
 *
 * Example usage:
 * @code
 * auto config = storage::SdCardConfig()
 *     .setMountPoint("/sdcard")
 *     .setMaxFiles(5)
 *     .setFormatIfFailed(false)
 *     .setSpiPins(23, 19, 18, 5);  // MOSI, MISO, CLK, CS
 *
 * SdCardStorage storage(config);
 * storage.write("config.json", jsonData);
 * auto data = storage.read("config.json");
 * @endcode
 */
class SdCardStorage
{
public:
    /**
     * @brief Construct SD card storage with configuration
     *
     * @param config SD card configuration
     * @throws std::runtime_error if initialization fails
     */
    explicit SdCardStorage(const storage::SdCardConfig &config);

    /**
     * @brief Construct with mount point only (use defaults)
     *
     * @param mountPoint SD card mount point
     * @deprecated Use config-based constructor
     */
    explicit SdCardStorage(const std::string &mountPoint);

    /**
     * @brief Destructor - unmounts SD card
     */
    ~SdCardStorage();

    // Delete copy constructor and assignment
    SdCardStorage(const SdCardStorage &) = delete;
    SdCardStorage &operator=(const SdCardStorage &) = delete;

    // Storage interface methods

    /**
     * @brief Write string data to file
     *
     * @param key File path relative to mount point
     * @param data String data to write
     * @return true if write successful, false otherwise
     */
    bool write(const std::string &key, const std::string &data);

    /**
     * @brief Write binary data to file
     *
     * @param key File path relative to mount point
     * @param data Binary data to write
     * @return true if write successful, false otherwise
     */
    bool write(const std::string &key, const std::vector<uint8_t> &data);

    /**
     * @brief Read string data from file
     *
     * @param key File path relative to mount point
     * @return Optional containing data if successful, nullopt otherwise
     */
    std::optional<std::string> read(const std::string &key);

    /**
     * @brief Read binary data from file
     *
     * @param key File path relative to mount point
     * @return Optional containing data if successful, nullopt otherwise
     */
    std::optional<std::vector<uint8_t>> readBinary(const std::string &key);

    /**
     * @brief Check if file exists
     *
     * @param key File path relative to mount point
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
     * @param key File path relative to mount point
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
     * @return StorageType::SDCARD
     */
    StorageType getType() const
    {
        return StorageType::SDCARD;
    }

    /**
     * @brief Get mount point path
     *
     * @return Mount point string
     */
    const std::string &getMountPoint() const
    {
        return config_.mountPoint;
    }

    /**
     * @brief Check if SD card is mounted
     *
     * @return true if mounted, false otherwise
     */
    bool isMounted() const
    {
        return initialized_;
    }

private:
    storage::SdCardConfig config_; ///< SD card configuration
    bool initialized_;             ///< Whether SD card was successfully mounted
    mutable std::mutex mutex_;     ///< Mutex for thread safety

#ifdef ESP_PLATFORM
    sdmmc_card_t *card_; ///< SD card handle
#endif

    /**
     * @brief Get full file path from key
     *
     * @param key Relative key (e.g., "config.json")
     * @return Full path (e.g., "/sdcard/config.json")
     */
    std::string getFullPath(const std::string &key) const;

    /**
     * @brief Initialize SD card if not already initialized
     *
     * @return true if initialization succeeded, false otherwise
     */
    bool initialize();
};

} // namespace lopcore
