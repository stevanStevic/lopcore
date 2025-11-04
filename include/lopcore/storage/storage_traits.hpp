/**
 * @file storage_traits.hpp
 * @brief Type traits for compile-time storage capability detection
 *
 * Uses SFINAE to detect storage backend capabilities at compile time.
 * Enables generic algorithms that adapt to storage features without
 * runtime overhead or virtual functions.
 *
 * Example usage:
 * @code
 * template<typename Storage>
 * class ConfigManager {
 *     static_assert(traits::is_file_based_v<Storage> ||
 *                   traits::is_key_value_v<Storage>,
 *                   "Storage must support string operations");
 *
 *     void save() {
 *         if constexpr (traits::requires_commit_v<Storage>) {
 *             storage_.commit();  // Only called for NVS
 *         }
 *     }
 * };
 * @endcode
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#pragma once

#include <string>
#include <type_traits>
#include <vector>

#include <esp_err.h>

namespace lopcore
{
namespace storage
{
namespace traits
{

// ========================================
// Trait: File-based storage
// ========================================

/**
 * @brief Detects if storage supports file-based operations
 *
 * Checks for presence of:
 * - read(const std::string& path, std::vector<uint8_t>& data)
 * - write(const std::string& path, const std::vector<uint8_t>& data)
 * - exists(const std::string& path, bool& exists)
 *
 * File-based storage: SpiffsStorage, LittleFsStorage, SdCardStorage
 * Not file-based: NvsStorage (key-value store)
 */
template<typename T, typename = void>
struct is_file_based : std::false_type
{
};

template<typename T>
struct is_file_based<
    T,
    std::void_t<decltype(std::declval<T>().read(std::declval<const std::string &>(),
                                                std::declval<std::vector<uint8_t> &>())),
                decltype(std::declval<T>().write(std::declval<const std::string &>(),
                                                 std::declval<const std::vector<uint8_t> &>())),
                decltype(std::declval<T>().exists(std::declval<const std::string &>(),
                                                  std::declval<bool &>()))>> : std::true_type
{
};

template<typename T>
inline constexpr bool is_file_based_v = is_file_based<T>::value;

// ========================================
// Trait: Key-value storage
// ========================================

/**
 * @brief Detects if storage supports key-value operations
 *
 * Checks for presence of:
 * - readString(const std::string& key, std::string& value)
 * - writeString(const std::string& key, const std::string& value)
 * - erase(const std::string& key)
 *
 * Key-value storage: NvsStorage
 * Also supports strings: SpiffsStorage (as file operations)
 */
template<typename T, typename = void>
struct is_key_value : std::false_type
{
};

template<typename T>
struct is_key_value<T,
                    std::void_t<decltype(std::declval<T>().readString(std::declval<const std::string &>(),
                                                                      std::declval<std::string &>())),
                                decltype(std::declval<T>().writeString(std::declval<const std::string &>(),
                                                                       std::declval<const std::string &>())),
                                decltype(std::declval<T>().erase(std::declval<const std::string &>()))>>
    : std::true_type
{
};

template<typename T>
inline constexpr bool is_key_value_v = is_key_value<T>::value;

// ========================================
// Trait: Typed operations (NVS specific)
// ========================================

/**
 * @brief Detects if storage supports typed read/write operations
 *
 * Checks for presence of:
 * - readU32(const std::string& key, uint32_t& value)
 * - writeU32(const std::string& key, uint32_t value)
 * - readBlob(const std::string& key, std::vector<uint8_t>& data)
 * - writeBlob(const std::string& key, const std::vector<uint8_t>& data)
 *
 * NVS-specific feature for type-safe key-value storage.
 */
template<typename T, typename = void>
struct has_typed_operations : std::false_type
{
};

template<typename T>
struct has_typed_operations<
    T,
    std::void_t<
        decltype(std::declval<T>().readU32(std::declval<const std::string &>(), std::declval<uint32_t &>())),
        decltype(std::declval<T>().writeU32(std::declval<const std::string &>(), std::declval<uint32_t>())),
        decltype(std::declval<T>().readBlob(std::declval<const std::string &>(),
                                            std::declval<std::vector<uint8_t> &>())),
        decltype(std::declval<T>().writeBlob(std::declval<const std::string &>(),
                                             std::declval<const std::vector<uint8_t> &>()))>> : std::true_type
{
};

template<typename T>
inline constexpr bool has_typed_operations_v = has_typed_operations<T>::value;

// ========================================
// Trait: Commit support (NVS specific)
// ========================================

/**
 * @brief Detects if storage requires explicit commit
 *
 * Checks for presence of:
 * - commit()
 *
 * NVS requires explicit commit to persist writes.
 * File-based storage (SPIFFS) writes are immediate.
 */
template<typename T, typename = void>
struct requires_commit : std::false_type
{
};

template<typename T>
struct requires_commit<T, std::void_t<decltype(std::declval<T>().commit())>> : std::true_type
{
};

template<typename T>
inline constexpr bool requires_commit_v = requires_commit<T>::value;

// ========================================
// Trait: Format support
// ========================================

/**
 * @brief Detects if storage supports format operation
 *
 * Checks for presence of:
 * - format()
 *
 * Both SPIFFS and NVS support formatting.
 * SD card storage typically doesn't expose format.
 */
template<typename T, typename = void>
struct supports_format : std::false_type
{
};

template<typename T>
struct supports_format<T, std::void_t<decltype(std::declval<T>().format())>> : std::true_type
{
};

template<typename T>
inline constexpr bool supports_format_v = supports_format<T>::value;

// ========================================
// Trait: String operations
// ========================================

/**
 * @brief Detects if storage supports string read/write
 *
 * Checks for presence of:
 * - readString(const std::string& key, std::string& value)
 * - writeString(const std::string& key, const std::string& value)
 *
 * Most storage backends support string operations, either as:
 * - File operations (SPIFFS: readString reads file content)
 * - Key-value operations (NVS: readString reads string value)
 */
template<typename T, typename = void>
struct supports_strings : std::false_type
{
};

template<typename T>
struct supports_strings<
    T,
    std::void_t<decltype(std::declval<T>().readString(std::declval<const std::string &>(),
                                                      std::declval<std::string &>())),
                decltype(std::declval<T>().writeString(std::declval<const std::string &>(),
                                                       std::declval<const std::string &>()))>>
    : std::true_type
{
};

template<typename T>
inline constexpr bool supports_strings_v = supports_strings<T>::value;

} // namespace traits
} // namespace storage
} // namespace lopcore
