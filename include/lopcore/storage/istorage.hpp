/**
 * @file istorage.hpp
 * @brief Abstract storage interface for LopCore
 *
 * Provides a unified API for accessing different storage backends (SPIFFS, NVS, SD Card).
 * This interface abstracts away the underlying storage implementation, allowing code to
 * work with any storage backend through a common API.
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "storage_type.hpp"

namespace lopcore
{

/**
 * @brief Abstract interface for storage operations
 *
 * Defines a common API for CRUD operations across different storage backends.
 * Implementations handle backend-specific details (SPIFFS file I/O, NVS key-value, etc.).
 *
 * Thread Safety: Implementations should be thread-safe unless documented otherwise.
 *
 * Example usage:
 * @code
 * auto storage = StorageFactory::createSpiffs("/spiffs");
 * storage->write("config.json", jsonData);
 * auto data = storage->read("config.json");
 * @endcode
 */
class IStorage
{
public:
    virtual ~IStorage() = default;

    // ========== Write Operations ==========

    /**
     * @brief Write string data to storage
     *
     * @param key Storage key (file path for SPIFFS, key name for NVS)
     * @param data String data to write
     * @return true if write succeeded, false otherwise
     *
     * @note For SPIFFS, key is relative to base path (e.g., "config.json" -> "/spiffs/config.json")
     * @note For NVS, key is limited to 15 characters
     */
    virtual bool write(const std::string &key, const std::string &data) = 0;

    /**
     * @brief Write binary data to storage
     *
     * @param key Storage key (file path for SPIFFS, key name for NVS)
     * @param data Binary data to write
     * @return true if write succeeded, false otherwise
     *
     * @note Useful for certificates, images, and other binary files
     */
    virtual bool write(const std::string &key, const std::vector<uint8_t> &data) = 0;

    // ========== Read Operations ==========

    /**
     * @brief Read string data from storage
     *
     * @param key Storage key to read
     * @return Optional containing data if key exists, std::nullopt otherwise
     *
     * @note Returns std::nullopt if key doesn't exist or read fails
     */
    virtual std::optional<std::string> read(const std::string &key) = 0;

    /**
     * @brief Read binary data from storage
     *
     * @param key Storage key to read
     * @return Optional containing binary data if key exists, std::nullopt otherwise
     *
     * @note Useful for reading certificates, images, and other binary files
     */
    virtual std::optional<std::vector<uint8_t>> readBinary(const std::string &key) = 0;

    // ========== Query Operations ==========

    /**
     * @brief Check if a key exists in storage
     *
     * @param key Storage key to check
     * @return true if key exists, false otherwise
     *
     * @note More efficient than read() if you only need to check existence
     */
    virtual bool exists(const std::string &key) = 0;

    /**
     * @brief List all keys in storage
     *
     * @return Vector of all keys in storage
     *
     * @note For SPIFFS, returns all filenames in base path
     * @note For NVS, returns all keys in namespace (may be slow)
     * @note For SD_CARD, returns all filenames in base path
     */
    virtual std::vector<std::string> listKeys() = 0;

    // ========== Delete Operations ==========

    /**
     * @brief Remove a key from storage
     *
     * @param key Storage key to remove
     * @return true if removal succeeded, false otherwise
     *
     * @note Returns true if key didn't exist (idempotent operation)
     */
    virtual bool remove(const std::string &key) = 0;

    // ========== Storage Info ==========

    /**
     * @brief Get total storage capacity
     *
     * @return Total size in bytes
     *
     * @note For SPIFFS, returns partition size
     * @note For NVS, returns namespace capacity
     * @note For SD_CARD, returns card capacity
     */
    virtual size_t getTotalSize() const = 0;

    /**
     * @brief Get used storage space
     *
     * @return Used size in bytes
     */
    virtual size_t getUsedSize() const = 0;

    /**
     * @brief Get free storage space
     *
     * @return Free size in bytes (typically getTotalSize() - getUsedSize())
     */
    virtual size_t getFreeSize() const = 0;

    /**
     * @brief Get storage backend type
     *
     * @return StorageType enum value (SPIFFS, NVS, or SD_CARD)
     *
     * @note Useful for backend-specific optimizations
     */
    virtual StorageType getType() const = 0;
};

} // namespace lopcore
