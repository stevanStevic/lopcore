#pragma once

#include <functional>
#include <optional>
#include <string>
#include <map>

namespace lopcore {
namespace prov {

/**
 * Function type for writing a key-value pair to storage
 * 
 * @param key Configuration key
 * @param value Configuration value
 * @return true if write succeeded, false otherwise
 */
using StorageWriteFunc = std::function<bool(const std::string& key, const std::string& value)>;

/**
 * Function type for reading a value from storage
 * 
 * @param key Configuration key
 * @return Value if found, std::nullopt otherwise
 */
using StorageReadFunc = std::function<std::optional<std::string>(const std::string& key)>;

/**
 * Storage callback pair
 * 
 * Contains write and read function callbacks for a storage backend.
 * Lambdas typically capture a shared_ptr to the underlying storage instance,
 * ensuring lifetime management.
 * 
 * Example:
 *   StorageCallbacks callbacks = {
 *       .write = [storage](const std::string& k, const std::string& v) {
 *           return storage->writeString(k, v);
 *       },
 *       .read = [storage](const std::string& k) -> std::optional<std::string> {
 *           std::string value;
 *           if (storage->readString(k, value)) return value;
 *           return std::nullopt;
 *       }
 *   };
 */
struct StorageCallbacks {
    StorageWriteFunc write;
    StorageReadFunc read;
};

/**
 * Map of configuration key to storage callbacks
 * 
 * Allows different config keys to use different storage backends:
 * - Critical configs (endpoint, thing_name) -> Encrypted NVS
 * - Large files (root_ca) -> SPIFFS
 * - Metadata (template_name) -> Plain NVS
 */
using StorageCallbackMap = std::map<std::string, StorageCallbacks>;

} // namespace prov
} // namespace lopcore
