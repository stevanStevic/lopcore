/**
 * @file littlefs_storage.cpp
 * @brief LittleFS storage implementation for flash memory
 *
 * Provides persistent storage using LittleFS filesystem optimized for flash memory.
 * Features wear leveling, power-loss resilience, and efficient flash usage.
 */

#include "lopcore/storage/littlefs_storage.hpp"

#include <dirent.h>
#include <sys/stat.h>

#include <cstring>
#include <fstream>
#include <sstream>

#include "lopcore/logging/logger.hpp"

#include "esp_littlefs.h"

namespace lopcore
{

static const char *TAG = "LittleFsStorage";

// ============================================================================
// Constructor & Destructor
// ============================================================================

LittleFsStorage::LittleFsStorage(const storage::LittleFsConfig &config) : config_(config), initialized_(false)
{
    LOPCORE_LOGI(TAG, "Creating LittleFS storage with base path: %s", config_.basePath.c_str());
}

LittleFsStorage::~LittleFsStorage()
{
    if (initialized_)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        LOPCORE_LOGI(TAG, "Cleaning up LittleFS storage");

        esp_err_t ret = esp_vfs_littlefs_unregister(config_.partitionLabel.c_str());
        if (ret != ESP_OK)
        {
            LOPCORE_LOGW(TAG, "Failed to unregister LittleFS: %s", esp_err_to_name(ret));
        }

        initialized_ = false;
    }
}

// ============================================================================
// Lifecycle Management
// ============================================================================

bool LittleFsStorage::initialize()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_)
    {
        LOPCORE_LOGW(TAG, "LittleFS already initialized");
        return true;
    }

    LOPCORE_LOGI(TAG, "Initializing LittleFS at %s (partition: %s)", config_.basePath.c_str(),
                 config_.partitionLabel.c_str());

    esp_vfs_littlefs_conf_t conf = {.base_path = config_.basePath.c_str(),
                                    .partition_label = config_.partitionLabel.c_str(),
                                    .format_if_mount_failed = config_.formatIfFailed,
                                    .dont_mount = false,
                                    .grow_on_mount = config_.growOnMount};

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            LOPCORE_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            LOPCORE_LOGE(TAG, "Failed to find LittleFS partition '%s'", config_.partitionLabel.c_str());
        }
        else
        {
            LOPCORE_LOGE(TAG, "Failed to initialize LittleFS: %s", esp_err_to_name(ret));
        }
        return false;
    }

    initialized_ = true;

    // Print filesystem info
    size_t total = 0, used = 0;
    ret = esp_littlefs_info(config_.partitionLabel.c_str(), &total, &used);
    if (ret == ESP_OK)
    {
        LOPCORE_LOGI(TAG, "LittleFS initialized successfully");
        LOPCORE_LOGI(TAG, "Total size: %zu KB, Used: %zu KB, Free: %zu KB", total / 1024, used / 1024,
                     (total - used) / 1024);
    }
    else
    {
        LOPCORE_LOGW(TAG, "LittleFS initialized but couldn't get filesystem info");
    }

    return true;
}

// ============================================================================
// Core Storage Operations
// ============================================================================

bool LittleFsStorage::write(const std::string &key, const std::string &data)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        LOPCORE_LOGE(TAG, "Cannot write: storage not initialized");
        return false;
    }

    std::string filepath = getFullPath(key);

    std::ofstream file(filepath, std::ios::out | std::ios::trunc);
    if (!file.is_open())
    {
        LOPCORE_LOGE(TAG, "Failed to open file for writing: %s", filepath.c_str());
        return false;
    }

    file << data;
    file.close();

    if (!file.good())
    {
        LOPCORE_LOGE(TAG, "Error writing to file: %s", filepath.c_str());
        return false;
    }

    LOPCORE_LOGD(TAG, "Wrote %zu bytes to key '%s'", data.size(), key.c_str());
    return true;
}

std::optional<std::string> LittleFsStorage::read(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        LOPCORE_LOGE(TAG, "Cannot read: storage not initialized");
        return std::nullopt;
    }

    std::string filepath = getFullPath(key);

    std::ifstream file(filepath);
    if (!file.is_open())
    {
        LOPCORE_LOGD(TAG, "File not found: %s", filepath.c_str());
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    std::string data = buffer.str();
    LOPCORE_LOGD(TAG, "Read %zu bytes from key '%s'", data.size(), key.c_str());

    return data;
}

bool LittleFsStorage::write(const std::string &key, const std::vector<uint8_t> &data)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        LOPCORE_LOGE(TAG, "Cannot write binary: storage not initialized");
        return false;
    }

    std::string filepath = getFullPath(key);

    std::ofstream file(filepath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        LOPCORE_LOGE(TAG, "Failed to open file for binary writing: %s", filepath.c_str());
        return false;
    }

    file.write(reinterpret_cast<const char *>(data.data()), data.size());
    file.close();

    if (!file.good())
    {
        LOPCORE_LOGE(TAG, "Error writing binary to file: %s", filepath.c_str());
        return false;
    }

    LOPCORE_LOGD(TAG, "Wrote %zu bytes (binary) to key '%s'", data.size(), key.c_str());
    return true;
}

bool LittleFsStorage::write(const std::string &key, const void *data, size_t dataLen)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        LOPCORE_LOGE(TAG, "Cannot write binary: storage not initialized");
        return false;
    }

    if (data == nullptr || dataLen == 0)
    {
        LOPCORE_LOGE(TAG, "Invalid data pointer or length");
        return false;
    }

    std::string filepath = getFullPath(key);

    std::ofstream file(filepath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        LOPCORE_LOGE(TAG, "Failed to open file for binary writing: %s", filepath.c_str());
        return false;
    }

    file.write(reinterpret_cast<const char *>(data), dataLen);
    file.close();

    if (!file.good())
    {
        LOPCORE_LOGE(TAG, "Error writing binary to file: %s", filepath.c_str());
        return false;
    }

    LOPCORE_LOGD(TAG, "Wrote %zu bytes (binary) to key '%s'", dataLen, key.c_str());
    return true;
}

std::optional<std::vector<uint8_t>> LittleFsStorage::readBinary(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        LOPCORE_LOGE(TAG, "Cannot read binary: storage not initialized");
        return std::nullopt;
    }

    std::string filepath = getFullPath(key);

    std::ifstream file(filepath, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        LOPCORE_LOGD(TAG, "File not found: %s", filepath.c_str());
        return std::nullopt;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    if (!file.read(reinterpret_cast<char *>(data.data()), size))
    {
        LOPCORE_LOGE(TAG, "Error reading binary from file: %s", filepath.c_str());
        return std::nullopt;
    }

    file.close();
    LOPCORE_LOGD(TAG, "Read %zu bytes (binary) from key '%s'", data.size(), key.c_str());

    return data;
}

bool LittleFsStorage::exists(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        return false;
    }

    std::string filepath = getFullPath(key);
    struct stat st;
    return (stat(filepath.c_str(), &st) == 0);
}

std::vector<std::string> LittleFsStorage::listKeys(const std::string &directory)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> keys;

    if (!initialized_)
    {
        LOPCORE_LOGE(TAG, "Cannot list keys: storage not initialized");
        return keys;
    }

    // Use base path + directory for listing
    std::string searchPath = config_.basePath;
    if (!directory.empty())
    {
        searchPath += "/" + directory;
    }

    DIR *dir = opendir(searchPath.c_str());
    if (!dir)
    {
        LOPCORE_LOGE(TAG, "Failed to open directory: %s", searchPath.c_str());
        return keys;
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

        // Add all files without modifying extensions
        keys.push_back(filename);
    }

    closedir(dir);
    LOPCORE_LOGD(TAG, "Found %zu keys", keys.size());

    return keys;
}

bool LittleFsStorage::remove(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        LOPCORE_LOGE(TAG, "Cannot remove: storage not initialized");
        return false;
    }

    std::string filepath = getFullPath(key);

    if (unlink(filepath.c_str()) != 0)
    {
        LOPCORE_LOGE(TAG, "Failed to remove file: %s", filepath.c_str());
        return false;
    }

    LOPCORE_LOGD(TAG, "Removed key '%s'", key.c_str());
    return true;
}

// ============================================================================
// Storage Information
// ============================================================================

size_t LittleFsStorage::getTotalSize() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        return 0;
    }

    size_t total = 0, used = 0;
    esp_err_t ret = esp_littlefs_info(config_.partitionLabel.c_str(), &total, &used);
    if (ret != ESP_OK)
    {
        LOPCORE_LOGE(TAG, "Failed to get LittleFS info: %s", esp_err_to_name(ret));
        return 0;
    }

    return total;
}

size_t LittleFsStorage::getUsedSize() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        return 0;
    }

    size_t total = 0, used = 0;
    esp_err_t ret = esp_littlefs_info(config_.partitionLabel.c_str(), &total, &used);
    if (ret != ESP_OK)
    {
        LOPCORE_LOGE(TAG, "Failed to get LittleFS info: %s", esp_err_to_name(ret));
        return 0;
    }

    return used;
}

size_t LittleFsStorage::getFreeSize() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        return 0;
    }

    size_t total = 0, used = 0;
    esp_err_t ret = esp_littlefs_info(config_.partitionLabel.c_str(), &total, &used);
    if (ret != ESP_OK)
    {
        LOPCORE_LOGE(TAG, "Failed to get LittleFS info: %s", esp_err_to_name(ret));
        return 0;
    }

    return (total > used) ? (total - used) : 0;
}

bool LittleFsStorage::hasSpace(size_t requiredBytes) const
{
    size_t freeSpace = getFreeSize();

    if (freeSpace < requiredBytes)
    {
        LOPCORE_LOGE(TAG, "Insufficient space! Need %zu bytes, but only %zu bytes free", requiredBytes,
                     freeSpace);
        return false;
    }

    return true;
}

std::optional<size_t> LittleFsStorage::getFileSize(const std::string &key) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        return std::nullopt;
    }

    std::string filepath = getFullPath(key);
    struct stat file_stat;

    if (stat(filepath.c_str(), &file_stat) != 0)
    {
        LOPCORE_LOGD(TAG, "Failed to stat file: %s", filepath.c_str());
        return std::nullopt;
    }

    if (!S_ISREG(file_stat.st_mode))
    {
        LOPCORE_LOGW(TAG, "Not a regular file: %s", filepath.c_str());
        return std::nullopt;
    }

    return static_cast<size_t>(file_stat.st_size);
}

void LittleFsStorage::displayStats() const
{
    size_t total = getTotalSize();
    size_t used = getUsedSize();
    size_t free = getFreeSize();
    float usage_percent = total > 0 ? (static_cast<float>(used) / total * 100.0f) : 0.0f;

    LOPCORE_LOGI(TAG, "========================================");
    LOPCORE_LOGI(TAG, "LittleFS Filesystem Statistics:");
    LOPCORE_LOGI(TAG, "  Total size: %.2f MB", total / (1024.0 * 1024.0));
    LOPCORE_LOGI(TAG, "  Used:       %.2f MB [%.1f%%]", used / (1024.0 * 1024.0), usage_percent);
    LOPCORE_LOGI(TAG, "  Free:       %.2f MB", free / (1024.0 * 1024.0));
    LOPCORE_LOGI(TAG, "========================================");
}

std::vector<std::string> LittleFsStorage::listKeysByPattern(const std::string &directory,
                                                            const std::string &pattern)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> matchingKeys;

    if (!initialized_)
    {
        LOPCORE_LOGE(TAG, "Cannot list keys by pattern: storage not initialized");
        return matchingKeys;
    }

    // Use base path + directory for searching
    std::string searchPath = config_.basePath;
    if (!directory.empty())
    {
        searchPath += "/" + directory;
    }

    DIR *dir = opendir(searchPath.c_str());
    if (!dir)
    {
        LOPCORE_LOGE(TAG, "Failed to open directory: %s", searchPath.c_str());
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
            std::string fullPath = searchPath + "/" + filename;
            struct stat file_stat;
            if (stat(fullPath.c_str(), &file_stat) == 0 && S_ISREG(file_stat.st_mode))
            {
                matchingKeys.push_back(filename);
            }
        }
    }

    closedir(dir);
    LOPCORE_LOGD(TAG, "Found %zu keys matching pattern '%s'", matchingKeys.size(), pattern.c_str());

    return matchingKeys;
}

size_t LittleFsStorage::removeByPattern(const std::string &directory, const std::string &pattern)
{
    // Reuse listKeysByPattern to get matching files, then delete them
    // Note: We can't hold the lock during listKeysByPattern call, so we temporarily unlock
    std::vector<std::string> matchingFiles;

    {
        // Lock is released here, listKeysByPattern will acquire its own lock
        matchingFiles = listKeysByPattern(directory, pattern);
    }

    if (matchingFiles.empty())
    {
        LOPCORE_LOGI(TAG, "No files matching pattern '%s' to delete", pattern.c_str());
        return 0;
    }

    LOPCORE_LOGI(TAG, "Deleting %zu file(s) matching pattern '%s'", matchingFiles.size(), pattern.c_str());

    size_t deletedCount = 0;
    std::string searchPath = config_.basePath;
    if (!directory.empty())
    {
        searchPath += "/" + directory;
    }

    // Now delete each matching file
    for (const auto &filename : matchingFiles)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!initialized_)
        {
            LOPCORE_LOGE(TAG, "Storage no longer initialized");
            break;
        }

        std::string fullPath = searchPath + "/" + filename;

        if (unlink(fullPath.c_str()) == 0)
        {
            LOPCORE_LOGD(TAG, "Deleted: %s", filename.c_str());
            deletedCount++;
        }
        else
        {
            LOPCORE_LOGE(TAG, "Failed to delete: %s", filename.c_str());
        }
    }

    LOPCORE_LOGI(TAG, "Deletion complete: %zu/%zu files deleted", deletedCount, matchingFiles.size());

    return deletedCount;
}

std::vector<LittleFsStorage::FileInfo> LittleFsStorage::listDetailed(const std::string &directory)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<FileInfo> files;

    if (!initialized_)
    {
        LOPCORE_LOGE(TAG, "Cannot list detailed: storage not initialized");
        return files;
    }

    // Use base path + directory for listing
    std::string searchPath = config_.basePath;
    if (!directory.empty())
    {
        searchPath += "/" + directory;
    }

    DIR *dir = opendir(searchPath.c_str());
    if (!dir)
    {
        LOPCORE_LOGE(TAG, "Failed to open directory: %s", searchPath.c_str());
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

        std::string fullPath = searchPath + "/" + filename;
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
    LOPCORE_LOGD(TAG, "Found %zu entries", files.size());

    return files;
}

// ============================================================================
// Helper Methods
// ============================================================================

std::string LittleFsStorage::getFullPath(const std::string &key) const
{
    return config_.basePath + "/" + key;
}

bool LittleFsStorage::patternMatch(const char *pattern, const char *str)
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

} // namespace lopcore
