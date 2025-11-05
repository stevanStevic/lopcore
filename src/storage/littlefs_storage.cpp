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
static const char *FILE_EXTENSION = ".txt";

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

        // Remove file extension if it matches
        if (filename.size() > strlen(FILE_EXTENSION))
        {
            std::string ext = filename.substr(filename.size() - strlen(FILE_EXTENSION));
            if (ext == FILE_EXTENSION)
            {
                filename = filename.substr(0, filename.size() - strlen(FILE_EXTENSION));
            }
        }

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

// ============================================================================
// Helper Methods
// ============================================================================

std::string LittleFsStorage::getFullPath(const std::string &key) const
{
    return config_.basePath + "/" + key + FILE_EXTENSION;
}

} // namespace lopcore
