/**
 * @file nvs_storage.hpp
 * @brief NVS storage implementation for LopCore
 *
 * Wraps ESP-IDF NVS (Non-Volatile Storage) APIs to provide IStorage interface.
 * NVS is best suited for:
 * - Configuration key-value pairs
 * - Small data (<4KB per entry typically)
 * - Encrypted sensitive data
 * - Fast read/write operations
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

#ifdef ESP_PLATFORM
#include <nvs.h>
#include <nvs_flash.h>
#endif

namespace lopcore
{

/**
 * @brief NVS implementation of IStorage interface
 *
 * Provides key-value storage using ESP-IDF's NVS (Non-Volatile Storage).
 * Thread-safe: All operations are protected by mutex.
 *
 * NVS Characteristics:
 * - Fast read/write (<1ms typically)
 * - Wear leveling built-in
 * - Optional encryption support
 * - Limited to 15-character keys
 * - Good for small data (<4KB per entry)
 * - Survives reboots and power cycles
 *
 * Example:
 * @code
 * storage::NvsConfig config;
 * config.setNamespace("app_config").setReadOnly(false);
 * NvsStorage storage(config);
 * storage.writeString("wifi_ssid", "MyNetwork");
 * std::string ssid;
 * storage.readString("wifi_ssid", ssid);
 * @endcode
 */
class NvsStorage
{
public:
    /**
     * @brief Construct NVS storage with configuration (RECOMMENDED)
     *
     * @param config NvsConfig with all settings
     *
     * Example:
     * @code
     * NvsConfig config;
     * config.setNamespace("my_app")
     *       .setReadOnly(false);
     * NvsStorage storage(config);
     * @endcode
     */
    explicit NvsStorage(const storage::NvsConfig &config);

    /**
     * @brief Construct NVS storage with namespace (DEPRECATED - use config constructor)
     *
     * @param namespaceName NVS namespace (default: "lopcore")
     *
     * @deprecated Use NvsStorage(const NvsConfig&) instead
     * @note Automatically initializes NVS flash if not already initialized
     * @note Namespace must be <= 15 characters
     */
    [[deprecated("Use NvsStorage(const NvsConfig&) constructor instead")]] explicit NvsStorage(
        const std::string &namespaceName = "lopcore");

    /**
     * @brief Destructor - closes NVS handle
     */
    ~NvsStorage();

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
        return StorageType::NVS;
    }

    // NVS-specific operations

    /**
     * @brief Erase entire namespace
     *
     * @return true if erase succeeded, false otherwise
     *
     * @warning This erases all keys in the namespace!
     */
    bool eraseNamespace();

    /**
     * @brief Commit pending writes to flash
     *
     * @return true if commit succeeded, false otherwise
     *
     * @note Writes are automatically committed, but you can manually commit for performance
     */
    bool commit();

    /**
     * @brief Get the namespace name
     *
     * @return Namespace string
     */
    const std::string &getNamespace() const
    {
        return config_.namespaceName;
    }

private:
    storage::NvsConfig config_; ///< NVS configuration
    mutable std::mutex mutex_;  ///< Mutex for thread safety
    bool initialized_;          ///< Whether NVS was initialized by this instance

#ifdef ESP_PLATFORM
    nvs_handle_t handle_; ///< NVS handle for operations

    /**
     * @brief Open NVS handle
     *
     * @return true if open succeeded, false otherwise
     */
    bool openHandle();

    /**
     * @brief Close NVS handle
     */
    void closeHandle();
#endif

    /**
     * @brief Initialize NVS flash if not already initialized
     *
     * @return true if initialization succeeded, false otherwise
     */
    bool initialize();

    /**
     * @brief Validate key length (NVS keys must be <= 15 characters)
     *
     * @param key Key to validate
     * @return true if valid, false otherwise
     */
    bool isValidKey(const std::string &key) const;
};

} // namespace lopcore
