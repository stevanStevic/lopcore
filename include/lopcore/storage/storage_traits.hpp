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
 *     static_assert(traits::supports_strings_v<Storage>,
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

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

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
 * @brief Detects if storage is a mounted, file-based backend
 *
 * Checks for presence of:
 * - isMounted() const
 * - write(const std::string& key, const std::vector<uint8_t>& data)
 * - readBinary(const std::string& key)
 * - exists(const std::string& key)
 *
 * File-based storage: SpiffsStorage, LittleFsStorage, SdCardStorage
 * Not file-based: NvsStorage (key-value store, no mount concept)
 */
template<typename T, typename = void>
struct is_file_based : std::false_type
{
};

template<typename T>
struct is_file_based<
    T,
    std::void_t<decltype(std::declval<const T>().isMounted()),
                decltype(std::declval<T>().write(std::declval<const std::string &>(),
                                                 std::declval<const std::vector<uint8_t> &>())),
                decltype(std::declval<T>().readBinary(std::declval<const std::string &>())),
                decltype(std::declval<T>().exists(std::declval<const std::string &>()))>> : std::true_type
{
};

template<typename T>
inline constexpr bool is_file_based_v = is_file_based<T>::value;

// ========================================
// Trait: Key-value storage
// ========================================

/**
 * @brief Detects if storage supports namespace-scoped key-value operations
 *
 * Checks for presence of:
 * - getNamespace() const
 * - eraseNamespace()
 * - write(const std::string& key, const std::string& data)
 * - read(const std::string& key)
 * - remove(const std::string& key)
 *
 * Key-value storage: NvsStorage
 */
template<typename T, typename = void>
struct is_key_value : std::false_type
{
};

template<typename T>
struct is_key_value<T,
                    std::void_t<decltype(std::declval<const T>().getNamespace()),
                                decltype(std::declval<T>().eraseNamespace()),
                                decltype(std::declval<T>().write(std::declval<const std::string &>(),
                                                                 std::declval<const std::string &>())),
                                decltype(std::declval<T>().read(std::declval<const std::string &>())),
                                decltype(std::declval<T>().remove(std::declval<const std::string &>()))>>
    : std::true_type
{
};

template<typename T>
inline constexpr bool is_key_value_v = is_key_value<T>::value;

// ========================================
// Trait: Typed operations (NVS specific)
// ========================================

/**
 * @brief Detects if storage supports typed integer read/write operations
 *
 * Checks for presence of:
 * - readUint32(const std::string& key)
 * - writeUint32(const std::string& key, uint32_t value)
 * - readInt64(const std::string& key)
 * - writeInt64(const std::string& key, int64_t value)
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
    std::void_t<decltype(std::declval<T>().readUint32(std::declval<const std::string &>())),
                decltype(std::declval<T>().writeUint32(std::declval<const std::string &>(),
                                                       std::declval<uint32_t>())),
                decltype(std::declval<T>().readInt64(std::declval<const std::string &>())),
                decltype(std::declval<T>().writeInt64(std::declval<const std::string &>(),
                                                      std::declval<int64_t>()))>> : std::true_type
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
 * SPIFFS supports formatting the partition. NVS does not expose format()
 * (it offers eraseNamespace() instead, which is a different operation).
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
 * - write(const std::string& key, const std::string& data)
 * - read(const std::string& key)
 *
 * All current backends support string operations, either as:
 * - File operations (SPIFFS: read returns file content)
 * - Key-value operations (NVS: read returns string value)
 */
template<typename T, typename = void>
struct supports_strings : std::false_type
{
};

template<typename T>
struct supports_strings<T,
                        std::void_t<decltype(std::declval<T>().write(std::declval<const std::string &>(),
                                                                     std::declval<const std::string &>())),
                                    decltype(std::declval<T>().read(std::declval<const std::string &>()))>>
    : std::true_type
{
};

template<typename T>
inline constexpr bool supports_strings_v = supports_strings<T>::value;

} // namespace traits
} // namespace storage
} // namespace lopcore
