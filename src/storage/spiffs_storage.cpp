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

bool SpiffsStorage::write(const std::string &key, const void *data, size_t dataLen)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        ESP_LOGE(TAG, "Storage not initialized");
        return false;
    }

    if (data == nullptr || dataLen == 0)
    {
        ESP_LOGE(TAG, "Invalid data pointer or length");
        return false;
    }

    std::string fullPath = getFullPath(key);
    FILE *file = fopen(fullPath.c_str(), "wb");
    if (file == nullptr)
    {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", fullPath.c_str());
        return false;
    }

    size_t written = fwrite(data, 1, dataLen, file);
    fclose(file);

    if (written != dataLen)
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

bool SpiffsStorage::hasSpace(size_t requiredBytes) const
{
    size_t freeSpace = getFreeSize();

    if (freeSpace < requiredBytes)
    {
        ESP_LOGE(TAG, "Insufficient space! Need %zu bytes, but only %zu bytes free", requiredBytes,
                 freeSpace);
        return false;
    }

    return true;
}

std::optional<size_t> SpiffsStorage::getFileSize(const std::string &key) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        return std::nullopt;
    }

    std::string fullPath = getFullPath(key);
    struct stat file_stat;

    if (stat(fullPath.c_str(), &file_stat) != 0)
    {
        ESP_LOGI(TAG, "Failed to stat file: %s", fullPath.c_str());
        return std::nullopt;
    }

    if (!S_ISREG(file_stat.st_mode))
    {
        ESP_LOGE(TAG, "Not a regular file: %s", fullPath.c_str());
        return std::nullopt;
    }

    return static_cast<size_t>(file_stat.st_size);
}

void SpiffsStorage::displayStats() const
{
    size_t total = getTotalSize();
    size_t used = getUsedSize();
    size_t free = getFreeSize();
    float usage_percent = total > 0 ? (static_cast<float>(used) / total * 100.0f) : 0.0f;

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "SPIFFS Filesystem Statistics:");
    ESP_LOGI(TAG, "  Total size: %.2f MB", total / (1024.0 * 1024.0));
    ESP_LOGI(TAG, "  Used:       %.2f MB [%.1f%%]", used / (1024.0 * 1024.0), usage_percent);
    ESP_LOGI(TAG, "  Free:       %.2f MB", free / (1024.0 * 1024.0));
    ESP_LOGI(TAG, "========================================");
}

bool SpiffsStorage::patternMatch(const char *pattern, const char *str)
{
    const char *star = strchr(pattern, '*');

    if (star == nullptr)
    {
        // No wildcard, exact match required
        return strcmp(pattern, str) == 0;
    }

    // Match prefix (before *)
    size_t prefix_len = star - pattern;
    if (strncmp(pattern, str, prefix_len) != 0)
    {
        return false;
    }

    // Match suffix (after *)
    const char *suffix = star + 1;
    size_t suffix_len = strlen(suffix);
    size_t str_len = strlen(str);

    if (suffix_len > str_len - prefix_len)
    {
        return false;
    }

    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

std::vector<std::string> SpiffsStorage::listKeysByPattern(const std::string &pattern)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> matchingKeys;

    if (!initialized_)
    {
        ESP_LOGE(TAG, "Storage not initialized");
        return matchingKeys;
    }

    DIR *dir = opendir(config_.basePath.c_str());
    if (dir == nullptr)
    {
        ESP_LOGE(TAG, "Failed to open directory: %s", config_.basePath.c_str());
        return matchingKeys;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        std::string filename = entry->d_name;

        // Skip . and ..
        if (filename == "." || filename == "..")
        {
            continue;
        }

        // Check if filename matches pattern
        if (patternMatch(pattern.c_str(), filename.c_str()))
        {
            // Verify it's a regular file
            std::string fullPath = config_.basePath + "/" + filename;
            struct stat file_stat;
            if (stat(fullPath.c_str(), &file_stat) == 0 && S_ISREG(file_stat.st_mode))
            {
                matchingKeys.push_back(filename);
            }
        }
    }

    closedir(dir);
    ESP_LOGI(TAG, "Found %zu keys matching pattern '%s'", matchingKeys.size(), pattern.c_str());

    return matchingKeys;
}

size_t SpiffsStorage::removeByPattern(const std::string &pattern)
{
    // Reuse listKeysByPattern to get matching files, then delete them
    std::vector<std::string> matchingFiles;

    {
        // Lock is released here, listKeysByPattern will acquire its own lock
        matchingFiles = listKeysByPattern(pattern);
    }

    if (matchingFiles.empty())
    {
        ESP_LOGI(TAG, "No files matching pattern '%s' to delete", pattern.c_str());
        return 0;
    }

    ESP_LOGI(TAG, "Deleting %zu file(s) matching pattern '%s'", matchingFiles.size(), pattern.c_str());

    size_t deletedCount = 0;

    // Now delete each matching file
    for (const auto &filename : matchingFiles)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!initialized_)
        {
            ESP_LOGE(TAG, "Storage no longer initialized");
            break;
        }

        std::string fullPath = config_.basePath + "/" + filename;

        if (::remove(fullPath.c_str()) == 0)
        {
            ESP_LOGI(TAG, "Deleted: %s", filename.c_str());
            deletedCount++;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to delete: %s", filename.c_str());
        }
    }

    ESP_LOGI(TAG, "Deletion complete: %zu/%zu files deleted", deletedCount, matchingFiles.size());

    return deletedCount;
}

std::vector<SpiffsStorage::FileInfo> SpiffsStorage::listDetailed()
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<FileInfo> files;

    if (!initialized_)
    {
        ESP_LOGE(TAG, "Storage not initialized");
        return files;
    }

    DIR *dir = opendir(config_.basePath.c_str());
    if (dir == nullptr)
    {
        ESP_LOGE(TAG, "Failed to open directory: %s", config_.basePath.c_str());
        return files;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        std::string filename = entry->d_name;

        // Skip . and ..
        if (filename == "." || filename == "..")
        {
            continue;
        }

        std::string fullPath = config_.basePath + "/" + filename;
        struct stat file_stat;

        if (stat(fullPath.c_str(), &file_stat) == 0)
        {
            FileInfo info;
            info.name = filename;
            info.isDirectory = S_ISDIR(file_stat.st_mode);
            info.size = S_ISREG(file_stat.st_mode) ? static_cast<size_t>(file_stat.st_size) : 0;
            files.push_back(info);
        }
    }

    closedir(dir);
    ESP_LOGI(TAG, "Found %zu entries", files.size());

    return files;
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
