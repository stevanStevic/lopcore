/**
 * @file sdcard_storage.cpp
 * @brief SD card storage implementation using FAT filesystem
 *
 * Provides persistent storage on removable SD cards with FAT filesystem support.
 * Supports both SDMMC (high-speed) and SPI modes for SD card interfacing.
 */

#include "lopcore/storage/sdcard_storage.hpp"

#include <dirent.h>
#include <sys/stat.h>

#include <cstring>
#include <fstream>
#include <sstream>

#include "driver/sdspi_host.h"

#include "lopcore/logging/logger.hpp"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

namespace lopcore
{

static const char *TAG = "SdCardStorage";
static const char *FILE_EXTENSION = ".txt";

// ============================================================================
// Constructor & Destructor
// ============================================================================

SdCardStorage::SdCardStorage(const storage::SdCardConfig &config)
    : config_(config), initialized_(false), card_(nullptr)
{
    LOPCORE_LOGI(TAG, "Creating SD card storage with mount point: %s", config_.mountPoint.c_str());
}

SdCardStorage::~SdCardStorage()
{
    if (initialized_)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        LOPCORE_LOGI(TAG, "Cleaning up SD card storage");

        // Unmount
        esp_err_t ret = esp_vfs_fat_sdcard_unmount(config_.mountPoint.c_str(), card_);
        if (ret != ESP_OK)
        {
            LOPCORE_LOGW(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
        }

        // Free SPI bus if we were using SPI mode
        if (!config_.useSdmmc)
        {
            spi_bus_free(SPI3_HOST); // Default SPI host for SD card
        }

        card_ = nullptr;
        initialized_ = false;
    }
}

// ============================================================================
// Lifecycle Management
// ============================================================================

bool SdCardStorage::initialize()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_)
    {
        LOPCORE_LOGW(TAG, "SD card already initialized");
        return true;
    }

    LOPCORE_LOGI(TAG, "Initializing SD card storage at %s", config_.mountPoint.c_str());

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {.format_if_mount_failed = config_.formatIfFailed,
                                                     .max_files = static_cast<int>(config_.maxFiles),
                                                     .allocation_unit_size = config_.allocationUnitSize};

    // Use the explicit useSdmmc flag to determine mode
    if (!config_.useSdmmc)
    {
        // SPI mode
        LOPCORE_LOGI(TAG, "Using SPI mode (MOSI=%d, MISO=%d, CLK=%d, CS=%d)", config_.spiMosi, config_.spiMiso,
                 config_.spiClk, config_.spiCs);

        sdmmc_host_t host = SDSPI_HOST_DEFAULT();

        sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_config.gpio_cs = (gpio_num_t) config_.spiCs;
        slot_config.host_id = (spi_host_device_t) host.slot;

        spi_bus_config_t bus_cfg = {
            .mosi_io_num = config_.spiMosi,
            .miso_io_num = config_.spiMiso,
            .sclk_io_num = config_.spiClk,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 4000,
        };

        esp_err_t ret = spi_bus_initialize((spi_host_device_t) host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
        if (ret != ESP_OK)
        {
            LOPCORE_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
            return false;
        }

        ret = esp_vfs_fat_sdspi_mount(config_.mountPoint.c_str(), &host, &slot_config, &mount_config, &card_);
        if (ret != ESP_OK)
        {
            LOPCORE_LOGE(TAG, "Failed to mount SD card via SPI: %s", esp_err_to_name(ret));
            spi_bus_free((spi_host_device_t) host.slot);
            return false;
        }
    }
    else
    {
        // SDMMC mode
        LOPCORE_LOGI(TAG, "Using SDMMC mode (%d-bit bus)", config_.sdmmcBusWidth);

        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        
        // Configure slot based on bus width
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        slot_config.width = config_.sdmmcBusWidth;
        
        // Configure custom pins if specified
        if (config_.sdmmcClk >= 0)
        {
            slot_config.clk = (gpio_num_t) config_.sdmmcClk;
        }
        if (config_.sdmmcCmd >= 0)
        {
            slot_config.cmd = (gpio_num_t) config_.sdmmcCmd;
        }
        if (config_.sdmmcD0 >= 0)
        {
            slot_config.d0 = (gpio_num_t) config_.sdmmcD0;
        }
        if (config_.sdmmcD1 >= 0 && config_.sdmmcBusWidth == 4)
        {
            slot_config.d1 = (gpio_num_t) config_.sdmmcD1;
        }
        if (config_.sdmmcD2 >= 0 && config_.sdmmcBusWidth == 4)
        {
            slot_config.d2 = (gpio_num_t) config_.sdmmcD2;
        }
        if (config_.sdmmcD3 >= 0 && config_.sdmmcBusWidth == 4)
        {
            slot_config.d3 = (gpio_num_t) config_.sdmmcD3;
        }

        esp_err_t ret = esp_vfs_fat_sdmmc_mount(config_.mountPoint.c_str(), &host, &slot_config,
                                                &mount_config, &card_);
        if (ret != ESP_OK)
        {
            LOPCORE_LOGE(TAG, "Failed to mount SD card via SDMMC: %s", esp_err_to_name(ret));
            return false;
        }
    }

    initialized_ = true;

    // Print card info
    if (card_)
    {
        sdmmc_card_print_info(stdout, card_);
        LOPCORE_LOGI(TAG, "SD card initialized successfully");
        LOPCORE_LOGI(TAG, "Total size: %zu KB", getTotalSize() / 1024);
        LOPCORE_LOGI(TAG, "Used size: %zu KB", getUsedSize() / 1024);
    }

    return true;
}

// ============================================================================
// Core Storage Operations
// ============================================================================

bool SdCardStorage::write(const std::string &key, const std::string &data)
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

std::optional<std::string> SdCardStorage::read(const std::string &key)
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

bool SdCardStorage::write(const std::string &key, const std::vector<uint8_t> &data)
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

std::optional<std::vector<uint8_t>> SdCardStorage::readBinary(const std::string &key)
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

bool SdCardStorage::exists(const std::string &key)
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

std::vector<std::string> SdCardStorage::listKeys(const std::string &directory)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> keys;

    if (!initialized_)
    {
        LOPCORE_LOGE(TAG, "Cannot list keys: storage not initialized");
        return keys;
    }

    // Use mount point + directory for listing
    std::string searchPath = config_.mountPoint;
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

bool SdCardStorage::remove(const std::string &key)
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

size_t SdCardStorage::getTotalSize() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || !card_)
    {
        return 0;
    }

    return ((uint64_t) card_->csd.capacity) * card_->csd.sector_size;
}

size_t SdCardStorage::getUsedSize() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_)
    {
        return 0;
    }

    // Calculate used space by iterating through files
    uint64_t usedSize = 0;
    DIR *dir = opendir(config_.mountPoint.c_str());
    if (!dir)
    {
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        std::string filepath = config_.mountPoint + "/" + entry->d_name;
        struct stat st;
        if (stat(filepath.c_str(), &st) == 0)
        {
            usedSize += st.st_size;
        }
    }

    closedir(dir);
    return usedSize;
}

size_t SdCardStorage::getFreeSize() const
{
    uint64_t total = getTotalSize();
    uint64_t used = getUsedSize();
    return (total > used) ? (total - used) : 0;
}

// ============================================================================
// Helper Methods
// ============================================================================

std::string SdCardStorage::getFullPath(const std::string &key) const
{
    return config_.mountPoint + "/" + key + FILE_EXTENSION;
}

} // namespace lopcore
