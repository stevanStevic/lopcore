/**
 * @file spiffs_storage.cpp
 * @brief SPIFFS (SPI Flash File System) storage implementation
 *
 * Provides file-based storage using ESP-IDF's SPIFFS library.
 * Supports both ESP32 platform and host-based testing.
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#include "lopcore/storage/spiffs_storage.hpp"

#ifdef ESP_PLATFORM
#include <esp_idf_version.h>
#include <esp_log.h>
#include <esp_spiffs.h>
#else
// Host mocks
#include <iostream>
#define ESP_LOGI(tag, format, ...) std::cout << "[INFO] " << tag << ": " << format << std::endl
#define ESP_LOGE(tag, format, ...) std::cerr << "[ERROR] " << tag << ": " << format << std::endl
#define ESP_OK 0
#define ESP_FAIL -1
#endif

#include <dirent.h>
#include <sys/stat.h>

#include <cstdio>
#include <cstring>

static const char *TAG = "SpiffsStorage";

namespace lopcore
{

SpiffsStorage::SpiffsStorage(const storage::SpiffsConfig &config) : config_(config), initialized_(false)
{
    initialized_ = initialize();
}

SpiffsStorage::SpiffsStorage(const std::string &basePath)
    : config_(storage::SpiffsConfig(basePath)), initialized_(false)
{
    initialized_ = initialize();
}

SpiffsStorage::~SpiffsStorage()
{
#ifdef ESP_PLATFORM
    if (initialized_)
    {
        esp_vfs_spiffs_unregister("spiffs_storage");
        ESP_LOGI(TAG, "SPIFFS unmounted");
    }
#endif
}

bool SpiffsStorage::initialize()
{
#ifdef ESP_PLATFORM
    // Check if already mounted
    if (isMounted())
    {
        ESP_LOGI(TAG, "SPIFFS already mounted at %s", config_.basePath.c_str());
        return true;
    }

    esp_vfs_spiffs_conf_t conf = {.base_path = config_.basePath.c_str(),
                                  .partition_label = config_.partitionLabel.empty()
                                                         ? nullptr
                                                         : config_.partitionLabel.c_str(),
                                  .max_files = config_.maxFiles,
                                  .format_if_mount_failed = config_.formatIfFailed};

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS: %d", ret);
        return false;
    }

    ESP_LOGI(TAG, "SPIFFS initialized at %s", config_.basePath.c_str());

    // Log partition info
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Partition size: total=%zu, used=%zu", total, used);
    }

    return true;
#else
    // Host: Just check/create directory
    struct stat st;
    if (stat(config_.basePath.c_str(), &st) != 0)
    {
        // Directory doesn't exist, try to create it
        if (mkdir(config_.basePath.c_str(), 0755) != 0)
        {
            ESP_LOGE(TAG, "Failed to create directory: %s", config_.basePath.c_str());
            return false;
        }
    }
    return true;
#endif
}

bool SpiffsStorage::isMounted() const
{
#ifdef ESP_PLATFORM
    struct stat st;
    return (stat(config_.basePath.c_str(), &st) == 0);
#else
    return true; // Host always considers it mounted
#endif
}

std::string SpiffsStorage::getFullPath(const std::string &key) const
{
    // Handle keys that already start with base path
    if (key.find(config_.basePath) == 0)
    {
        return key;
    }

    // Handle keys starting with /
    if (!key.empty() && key[0] == '/')
    {
        return config_.basePath + key;
    }

    // Normal case: basePath + / + key
    return config_.basePath + "/" + key;
}

bool SpiffsStorage::write(const std::string &key, const std::string &data)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        ESP_LOGE(TAG, "Storage not initialized");
        return false;
    }

    std::string fullPath = getFullPath(key);
    FILE *file = fopen(fullPath.c_str(), "w");
    if (file == nullptr)
    {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", fullPath.c_str());
        return false;
    }

    size_t written = fwrite(data.c_str(), 1, data.size(), file);
    fclose(file);

    if (written != data.size())
    {
        ESP_LOGE(TAG, "Failed to write complete data to: %s", fullPath.c_str());
        return false;
    }

    ESP_LOGI(TAG, "Wrote %zu bytes to: %s", written, fullPath.c_str());
    return true;
}

bool SpiffsStorage::write(const std::string &key, const std::vector<uint8_t> &data)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        ESP_LOGE(TAG, "Storage not initialized");
        return false;
    }

    std::string fullPath = getFullPath(key);
    FILE *file = fopen(fullPath.c_str(), "wb");
    if (file == nullptr)
    {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", fullPath.c_str());
        return false;
    }

    size_t written = fwrite(data.data(), 1, data.size(), file);
    fclose(file);

    if (written != data.size())
    {
        ESP_LOGE(TAG, "Failed to write complete data to: %s", fullPath.c_str());
        return false;
    }

    ESP_LOGI(TAG, "Wrote %zu bytes to: %s", written, fullPath.c_str());
    return true;
}

std::optional<std::string> SpiffsStorage::read(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        ESP_LOGE(TAG, "Storage not initialized");
        return std::nullopt;
    }

    std::string fullPath = getFullPath(key);
    FILE *file = fopen(fullPath.c_str(), "r");
    if (file == nullptr)
    {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", fullPath.c_str());
        return std::nullopt;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0)
    {
        ESP_LOGE(TAG, "Failed to get file size: %s", fullPath.c_str());
        fclose(file);
        return std::nullopt;
    }

    // Read file content
    std::string content(size, '\0');
    size_t bytesRead = fread(&content[0], 1, size, file);
    fclose(file);

    if (bytesRead != static_cast<size_t>(size))
    {
        ESP_LOGE(TAG, "Failed to read complete file: %s", fullPath.c_str());
        return std::nullopt;
    }

    ESP_LOGI(TAG, "Read %zu bytes from: %s", bytesRead, fullPath.c_str());
    return content;
}

std::optional<std::vector<uint8_t>> SpiffsStorage::readBinary(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        ESP_LOGE(TAG, "Storage not initialized");
        return std::nullopt;
    }

    std::string fullPath = getFullPath(key);
    FILE *file = fopen(fullPath.c_str(), "rb");
    if (file == nullptr)
    {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", fullPath.c_str());
        return std::nullopt;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0)
    {
        ESP_LOGE(TAG, "Failed to get file size: %s", fullPath.c_str());
        fclose(file);
        return std::nullopt;
    }

    // Read file content
    std::vector<uint8_t> content(size);
    size_t bytesRead = fread(content.data(), 1, size, file);
    fclose(file);

    if (bytesRead != static_cast<size_t>(size))
    {
        ESP_LOGE(TAG, "Failed to read complete file: %s", fullPath.c_str());
        return std::nullopt;
    }

    ESP_LOGI(TAG, "Read %zu bytes from: %s", bytesRead, fullPath.c_str());
    return content;
}

bool SpiffsStorage::exists(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        return false;
    }

    std::string fullPath = getFullPath(key);
    struct stat st;
    return (stat(fullPath.c_str(), &st) == 0);
}

std::vector<std::string> SpiffsStorage::listKeys()
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> keys;

    if (!initialized_)
    {
        ESP_LOGE(TAG, "Storage not initialized");
        return keys;
    }

    DIR *dir = opendir(config_.basePath.c_str());
    if (dir == nullptr)
    {
        ESP_LOGE(TAG, "Failed to open directory: %s", config_.basePath.c_str());
        return keys;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }
        keys.push_back(entry->d_name);
    }

    closedir(dir);
    ESP_LOGI(TAG, "Listed %zu keys", keys.size());
    return keys;
}

bool SpiffsStorage::remove(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        ESP_LOGE(TAG, "Storage not initialized");
        return false;
    }

    std::string fullPath = getFullPath(key);

    // Check if file exists first
    struct stat st;
    if (stat(fullPath.c_str(), &st) != 0)
    {
        // File doesn't exist, consider it a success (idempotent)
        ESP_LOGI(TAG, "File doesn't exist (already removed): %s", fullPath.c_str());
        return true;
    }

    if (::remove(fullPath.c_str()) == 0)
    {
        ESP_LOGI(TAG, "Removed file: %s", fullPath.c_str());
        return true;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to remove file: %s", fullPath.c_str());
        return false;
    }
}

size_t SpiffsStorage::getTotalSize() const
{
    std::lock_guard<std::mutex> lock(mutex_);

#ifdef ESP_PLATFORM
    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info("spiffs_storage", &total, &used);
    if (ret == ESP_OK)
    {
        return total;
    }
#endif
    return 0;
}

size_t SpiffsStorage::getUsedSize() const
{
    std::lock_guard<std::mutex> lock(mutex_);

#ifdef ESP_PLATFORM
    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info("spiffs_storage", &total, &used);
    if (ret == ESP_OK)
    {
        return used;
    }
#endif
    return 0;
}

size_t SpiffsStorage::getFreeSize() const
{
    return getTotalSize() - getUsedSize();
}

bool SpiffsStorage::format()
{
    std::lock_guard<std::mutex> lock(mutex_);

#ifdef ESP_PLATFORM
    ESP_LOGI(TAG, "Formatting SPIFFS partition...");
    esp_err_t ret = esp_spiffs_format("spiffs_storage");
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "SPIFFS formatted successfully");
        return true;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to format SPIFFS: %d", ret);
        return false;
    }
#else
    ESP_LOGI(TAG, "Format not supported on host (would delete %s/*)", config_.basePath.c_str());
    return false;
#endif
}

bool SpiffsStorage::check()
{
    std::lock_guard<std::mutex> lock(mutex_);

#ifdef ESP_PLATFORM
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
    ESP_LOGI(TAG, "Checking SPIFFS filesystem integrity...");
    esp_err_t ret = esp_spiffs_check("spiffs_storage");
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "SPIFFS check passed");
        return true;
    }
    else
    {
        ESP_LOGE(TAG, "SPIFFS check failed: %d", ret);
        return false;
    }
#else
    ESP_LOGI(TAG, "SPIFFS check not available (requires ESP-IDF >= 4.4.0)");
    return false;
#endif
#else
    ESP_LOGI(TAG, "SPIFFS check not supported on host");
    return false;
#endif
}

} // namespace lopcore
