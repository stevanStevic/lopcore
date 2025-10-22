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
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <mutex>
#include <string>

#include "istorage.hpp"

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
 * auto storage = std::make_unique<NvsStorage>("app_config");
 * storage->write("wifi_ssid", "MyNetwork");
 * auto ssid = storage->read("wifi_ssid");
 * @endcode
 */
class NvsStorage : public IStorage
{
public:
    /**
     * @brief Construct NVS storage with namespace
     *
     * @param namespaceName NVS namespace (default: "lopcore")
     *
     * @note Automatically initializes NVS flash if not already initialized
     * @note Namespace must be <= 15 characters
     */
    explicit NvsStorage(const std::string &namespaceName = "lopcore");

    /**
     * @brief Destructor - closes NVS handle
     */
    ~NvsStorage() override;

    // IStorage interface implementation
    bool write(const std::string &key, const std::string &data) override;
    bool write(const std::string &key, const std::vector<uint8_t> &data) override;
    std::optional<std::string> read(const std::string &key) override;
    std::optional<std::vector<uint8_t>> readBinary(const std::string &key) override;
    bool exists(const std::string &key) override;
    std::vector<std::string> listKeys() override;
    bool remove(const std::string &key) override;
    size_t getTotalSize() const override;
    size_t getUsedSize() const override;
    size_t getFreeSize() const override;
    StorageType getType() const override
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
        return namespaceName_;
    }

private:
    std::string namespaceName_; ///< NVS namespace name
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
