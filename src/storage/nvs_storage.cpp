/**
 * @file nvs_storage.cpp
 * @brief NVS (Non-Volatile Storage) implementation
 *
 * Provides persistent key-value storage using ESP-IDF's NVS library.
 * Supports both ESP32 platform and host-based testing.
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#include "lopcore/storage/nvs_storage.hpp"

#ifdef ESP_PLATFORM
#include <esp_log.h>
#else
// Host mocks
#include <iostream>
#include <map>
#define ESP_LOGI(tag, format, ...) std::cout << "[INFO] " << tag << ": " << format << std::endl
#define ESP_LOGE(tag, format, ...) std::cerr << "[ERROR] " << tag << ": " << format << std::endl
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NVS_NOT_FOUND 0x1106
#endif

static const char *TAG = "NvsStorage";
static constexpr size_t NVS_KEY_MAX_LENGTH = 15; // NVS key length limit

namespace lopcore
{

// Host mock storage (for unit tests)
#ifndef ESP_PLATFORM
static std::map<std::string, std::map<std::string, std::vector<uint8_t>>> g_nvsData;
#endif

NvsStorage::NvsStorage(const storage::NvsConfig &config)
    : config_(config), initialized_(false)
#ifdef ESP_PLATFORM
      ,
      handle_(0)
#endif
{
    // Don't auto-initialize - let user call initialize() explicitly
}

NvsStorage::NvsStorage(const std::string &namespaceName)
    : config_(storage::NvsConfig(namespaceName)), initialized_(false)
#ifdef ESP_PLATFORM
      ,
      handle_(0)
#endif
{
    // Don't auto-initialize - let user call initialize() explicitly
}

NvsStorage::~NvsStorage()
{
#ifdef ESP_PLATFORM
    closeHandle();
#endif
}

bool NvsStorage::initialize()
{
#ifdef ESP_PLATFORM
    // Initialize NVS flash if not already done
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated or version changed
        // Erase and re-initialize
        ESP_LOGI(TAG, "NVS needs re-initialization, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS flash: %d", ret);
        return false;
    }

    ESP_LOGI(TAG, "NVS initialized with namespace: %s", config_.namespaceName.c_str());
    return true;
#else
    // Host: Mock initialization
    ESP_LOGI(TAG, "NVS initialized (mock) with namespace: %s", config_.namespaceName.c_str());
    return true;
#endif
}

#ifdef ESP_PLATFORM
bool NvsStorage::openHandle()
{
    if (handle_ != 0)
    {
        return true; // Already open
    }

    nvs_open_mode_t mode = config_.readOnly ? NVS_READONLY : NVS_READWRITE;
    esp_err_t ret = nvs_open(config_.namespaceName.c_str(), mode, &handle_);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS handle: %d", ret);
        return false;
    }

    return true;
}

void NvsStorage::closeHandle()
{
    if (handle_ != 0)
    {
        nvs_close(handle_);
        handle_ = 0;
    }
}
#endif

bool NvsStorage::isValidKey(const std::string &key) const
{
    if (key.empty() || key.length() > NVS_KEY_MAX_LENGTH)
    {
        ESP_LOGE(TAG, "Invalid key length: %zu (must be 1-15 chars)", key.length());
        return false;
    }
    return true;
}

bool NvsStorage::write(const std::string &key, const std::string &data)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        ESP_LOGE(TAG, "Storage not initialized");
        return false;
    }

    if (!isValidKey(key))
    {
        return false;
    }

#ifdef ESP_PLATFORM
    if (!openHandle())
    {
        return false;
    }

    esp_err_t ret = nvs_set_str(handle_, key.c_str(), data.c_str());
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write key '%s': %d", key.c_str(), ret);
        return false;
    }

    // Commit to flash
    ret = nvs_commit(handle_);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS: %d", ret);
        return false;
    }

    ESP_LOGI(TAG, "Wrote string key '%s' (%zu bytes)", key.c_str(), data.length());
    return true;
#else
    // Host mock
    std::vector<uint8_t> bytes(data.begin(), data.end());
    bytes.push_back('\0'); // Null terminator
    g_nvsData[config_.namespaceName][key] = bytes;
    ESP_LOGI(TAG, "Wrote string key '%s' (%zu bytes) [MOCK]", key.c_str(), data.length());
    return true;
#endif
}

bool NvsStorage::write(const std::string &key, const std::vector<uint8_t> &data)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        ESP_LOGE(TAG, "Storage not initialized");
        return false;
    }

    if (!isValidKey(key))
    {
        return false;
    }

#ifdef ESP_PLATFORM
    if (!openHandle())
    {
        return false;
    }

    esp_err_t ret = nvs_set_blob(handle_, key.c_str(), data.data(), data.size());
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write binary key '%s': %d", key.c_str(), ret);
        return false;
    }

    // Commit to flash
    ret = nvs_commit(handle_);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS: %d", ret);
        return false;
    }

    ESP_LOGI(TAG, "Wrote binary key '%s' (%zu bytes)", key.c_str(), data.size());
    return true;
#else
    // Host mock
    g_nvsData[config_.namespaceName][key] = data;
    ESP_LOGI(TAG, "Wrote binary key '%s' (%zu bytes) [MOCK]", key.c_str(), data.size());
    return true;
#endif
}

std::optional<std::string> NvsStorage::read(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        ESP_LOGE(TAG, "Storage not initialized");
        return std::nullopt;
    }

    if (!isValidKey(key))
    {
        return std::nullopt;
    }

#ifdef ESP_PLATFORM
    if (!openHandle())
    {
        return std::nullopt;
    }

    // First, get the required size
    size_t requiredSize = 0;
    esp_err_t ret = nvs_get_str(handle_, key.c_str(), nullptr, &requiredSize);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "Key not found: '%s'", key.c_str());
        return std::nullopt;
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get size for key '%s': %d", key.c_str(), ret);
        return std::nullopt;
    }

    // Allocate buffer and read
    std::string value(requiredSize - 1, '\0'); // -1 for null terminator
    ret = nvs_get_str(handle_, key.c_str(), &value[0], &requiredSize);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read key '%s': %d", key.c_str(), ret);
        return std::nullopt;
    }

    ESP_LOGI(TAG, "Read string key '%s' (%zu bytes)", key.c_str(), value.length());
    return value;
#else
    // Host mock
    auto nsIt = g_nvsData.find(config_.namespaceName);
    if (nsIt == g_nvsData.end())
    {
        ESP_LOGE(TAG, "Namespace not found: '%s'", config_.namespaceName.c_str());
        return std::nullopt;
    }

    auto keyIt = nsIt->second.find(key);
    if (keyIt == nsIt->second.end())
    {
        ESP_LOGE(TAG, "Key not found: '%s'", key.c_str());
        return std::nullopt;
    }

    // Convert bytes to string (remove null terminator if present)
    const auto &bytes = keyIt->second;
    std::string value(bytes.begin(), bytes.end());
    if (!value.empty() && value.back() == '\0')
    {
        value.pop_back();
    }

    ESP_LOGI(TAG, "Read string key '%s' (%zu bytes) [MOCK]", key.c_str(), value.length());
    return value;
#endif
}

std::optional<std::vector<uint8_t>> NvsStorage::readBinary(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        ESP_LOGE(TAG, "Storage not initialized");
        return std::nullopt;
    }

    if (!isValidKey(key))
    {
        return std::nullopt;
    }

#ifdef ESP_PLATFORM
    if (!openHandle())
    {
        return std::nullopt;
    }

    // First, get the required size
    size_t requiredSize = 0;
    esp_err_t ret = nvs_get_blob(handle_, key.c_str(), nullptr, &requiredSize);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "Key not found: '%s'", key.c_str());
        return std::nullopt;
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get size for key '%s': %d", key.c_str(), ret);
        return std::nullopt;
    }

    // Allocate buffer and read
    std::vector<uint8_t> data(requiredSize);
    ret = nvs_get_blob(handle_, key.c_str(), data.data(), &requiredSize);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read key '%s': %d", key.c_str(), ret);
        return std::nullopt;
    }

    ESP_LOGI(TAG, "Read binary key '%s' (%zu bytes)", key.c_str(), data.size());
    return data;
#else
    // Host mock
    auto nsIt = g_nvsData.find(config_.namespaceName);
    if (nsIt == g_nvsData.end())
    {
        ESP_LOGE(TAG, "Namespace not found: '%s'", config_.namespaceName.c_str());
        return std::nullopt;
    }

    auto keyIt = nsIt->second.find(key);
    if (keyIt == nsIt->second.end())
    {
        ESP_LOGE(TAG, "Key not found: '%s'", key.c_str());
        return std::nullopt;
    }

    ESP_LOGI(TAG, "Read binary key '%s' (%zu bytes) [MOCK]", key.c_str(), keyIt->second.size());
    return keyIt->second;
#endif
}

bool NvsStorage::exists(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        return false;
    }

    if (!isValidKey(key))
    {
        return false;
    }

#ifdef ESP_PLATFORM
    if (!openHandle())
    {
        return false;
    }

    // Try to get size - if key doesn't exist, will return ESP_ERR_NVS_NOT_FOUND
    size_t requiredSize = 0;
    esp_err_t ret = nvs_get_str(handle_, key.c_str(), nullptr, &requiredSize);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        // Try as blob
        ret = nvs_get_blob(handle_, key.c_str(), nullptr, &requiredSize);
    }

    return (ret == ESP_OK);
#else
    // Host mock
    auto nsIt = g_nvsData.find(config_.namespaceName);
    if (nsIt == g_nvsData.end())
    {
        return false;
    }

    return nsIt->second.find(key) != nsIt->second.end();
#endif
}

std::vector<std::string> NvsStorage::listKeys()
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> keys;

    if (!initialized_)
    {
        ESP_LOGE(TAG, "Storage not initialized");
        return keys;
    }

#ifdef ESP_PLATFORM
    // Note: NVS doesn't provide a native way to list all keys
    // This would require iterating through the entire NVS partition
    // which is not efficient. For now, return empty vector.
    ESP_LOGI(TAG, "NVS does not support efficient key listing");
    return keys;
#else
    // Host mock - we can list keys
    auto nsIt = g_nvsData.find(config_.namespaceName);
    if (nsIt != g_nvsData.end())
    {
        for (const auto &pair : nsIt->second)
        {
            keys.push_back(pair.first);
        }
    }

    ESP_LOGI(TAG, "Listed %zu keys [MOCK]", keys.size());
    return keys;
#endif
}

bool NvsStorage::remove(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        ESP_LOGE(TAG, "Storage not initialized");
        return false;
    }

    if (!isValidKey(key))
    {
        return false;
    }

#ifdef ESP_PLATFORM
    if (!openHandle())
    {
        return false;
    }

    esp_err_t ret = nvs_erase_key(handle_, key.c_str());
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        // Key doesn't exist, consider it success (idempotent)
        ESP_LOGI(TAG, "Key doesn't exist (already removed): '%s'", key.c_str());
        return true;
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to erase key '%s': %d", key.c_str(), ret);
        return false;
    }

    // Commit to flash
    ret = nvs_commit(handle_);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS: %d", ret);
        return false;
    }

    ESP_LOGI(TAG, "Removed key: '%s'", key.c_str());
    return true;
#else
    // Host mock
    auto nsIt = g_nvsData.find(config_.namespaceName);
    if (nsIt != g_nvsData.end())
    {
        nsIt->second.erase(key);
    }

    ESP_LOGI(TAG, "Removed key: '%s' [MOCK]", key.c_str());
    return true;
#endif
}

size_t NvsStorage::getTotalSize() const
{
    // NVS size is typically fixed per partition
    // For ESP32, default NVS partition is usually 16-20KB
    return 20 * 1024; // 20KB estimate
}

size_t NvsStorage::getUsedSize() const
{
    // Note: NVS doesn't provide accurate used size calculation
    // Would need to iterate all entries
    return 0;
}

size_t NvsStorage::getFreeSize() const
{
    return getTotalSize() - getUsedSize();
}

bool NvsStorage::eraseNamespace()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        ESP_LOGE(TAG, "Storage not initialized");
        return false;
    }

#ifdef ESP_PLATFORM
    if (!openHandle())
    {
        return false;
    }

    esp_err_t ret = nvs_erase_all(handle_);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to erase namespace '%s': %d", config_.namespaceName.c_str(), ret);
        return false;
    }

    // Commit to flash
    ret = nvs_commit(handle_);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS: %d", ret);
        return false;
    }

    ESP_LOGI(TAG, "Erased namespace: '%s'", config_.namespaceName.c_str());
    return true;
#else
    // Host mock
    g_nvsData[config_.namespaceName].clear();
    ESP_LOGI(TAG, "Erased namespace: '%s' [MOCK]", config_.namespaceName.c_str());
    return true;
#endif
}

bool NvsStorage::commit()
{
    std::lock_guard<std::mutex> lock(mutex_);

#ifdef ESP_PLATFORM
    if (!openHandle())
    {
        return false;
    }

    esp_err_t ret = nvs_commit(handle_);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit NVS: %d", ret);
        return false;
    }

    ESP_LOGI(TAG, "Committed NVS changes");
    return true;
#else
    // Host mock - no-op
    ESP_LOGI(TAG, "Committed NVS changes [MOCK]");
    return true;
#endif
}

} // namespace lopcore
