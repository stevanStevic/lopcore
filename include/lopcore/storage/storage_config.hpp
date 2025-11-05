/**
 * @file storage_config.hpp
 * @brief Configuration structures for storage backends
 *
 * Provides type-safe configuration with builder pattern for explicit
 * storage construction without factory pattern.
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#pragma once

#include <cstddef>
#include <string>

namespace lopcore
{
namespace storage
{

/**
 * @brief SPIFFS filesystem configuration
 *
 * Configuration for SPIFFS storage backend. Use builder pattern
 * for convenient setup:
 *
 * @code
 * SpiffsConfig config;
 * config.setBasePath("/spiffs")
 *       .setMaxFiles(10)
 *       .setFormatIfFailed(true);
 *
 * SpiffsStorage storage(config);
 * @endcode
 */
struct SpiffsConfig
{
    std::string basePath = "/spiffs";
    std::string partitionLabel = "storage";
    size_t maxFiles = 5;
    bool formatIfFailed = false;

    /**
     * @brief Set SPIFFS mount point path
     *
     * @param path Mount point (e.g., "/spiffs")
     * @return Reference to this config for chaining
     */
    SpiffsConfig &setBasePath(const std::string &path)
    {
        basePath = path;
        return *this;
    }

    /**
     * @brief Set partition label
     *
     * @param label Partition label from partition table
     * @return Reference to this config for chaining
     */
    SpiffsConfig &setPartitionLabel(const std::string &label)
    {
        partitionLabel = label;
        return *this;
    }

    /**
     * @brief Set maximum number of open files
     *
     * @param max Maximum simultaneous open files
     * @return Reference to this config for chaining
     */
    SpiffsConfig &setMaxFiles(size_t max)
    {
        maxFiles = max;
        return *this;
    }

    /**
     * @brief Enable automatic format on mount failure
     *
     * @param format True to format if mount fails
     * @return Reference to this config for chaining
     */
    SpiffsConfig &setFormatIfFailed(bool format)
    {
        formatIfFailed = format;
        return *this;
    }
};

/**
 * @brief NVS (Non-Volatile Storage) configuration
 *
 * Configuration for NVS storage backend. Use builder pattern
 * for convenient setup:
 *
 * @code
 * NvsConfig config;
 * config.setNamespace("my_app")
 *       .setReadOnly(false);
 *
 * NvsStorage storage(config);
 * @endcode
 */
struct NvsConfig
{
    std::string namespaceName = "lopcore";
    bool readOnly = false;

    /**
     * @brief Set NVS namespace
     *
     * @param ns Namespace name (max 15 characters)
     * @return Reference to this config for chaining
     */
    NvsConfig &setNamespace(const std::string &ns)
    {
        namespaceName = ns;
        return *this;
    }

    /**
     * @brief Set read-only mode
     *
     * @param ro True for read-only access
     * @return Reference to this config for chaining
     */
    NvsConfig &setReadOnly(bool ro)
    {
        readOnly = ro;
        return *this;
    }
};

/**
 * @brief SD Card storage configuration
 *
 * Configuration for FAT filesystem on SD card. Supports both SPI and SDMMC modes.
 * Use builder pattern:
 *
 * @code
 * // SPI mode
 * SdCardConfig config;
 * config.setMountPoint("/sdcard")
 *       .setUseSdmmc(false)
 *       .setSpiPins(11, 13, 12, 10);
 *
 * // SDMMC mode (faster, but requires specific pins)
 * SdCardConfig config;
 * config.setMountPoint("/sdcard")
 *       .setUseSdmmc(true)
 *       .setSdmmcPins(15, 16, 17, 18, 19, 20); // CLK, CMD, D0-D3
 *
 * SdCardStorage storage(config);
 * @endcode
 */
struct SdCardConfig
{
    std::string mountPoint = "/sdcard";
    size_t maxFiles = 5;
    bool formatIfFailed = false;
    size_t allocationUnitSize = 16 * 1024; // 16KB default
    bool useSdmmc = false;                 // Use SDMMC mode (true) or SPI mode (false)
    
    // SPI mode pins (used when useSdmmc = false)
    int spiMosi = -1;                      // GPIO pin for MOSI (use -1 for auto/default)
    int spiMiso = -1;                      // GPIO pin for MISO
    int spiClk = -1;                       // GPIO pin for CLK
    int spiCs = -1;                        // GPIO pin for CS
    
    // SDMMC mode pins (used when useSdmmc = true)
    int sdmmcClk = -1;                     // GPIO pin for CLK
    int sdmmcCmd = -1;                     // GPIO pin for CMD
    int sdmmcD0 = -1;                      // GPIO pin for D0
    int sdmmcD1 = -1;                      // GPIO pin for D1 (optional for 1-bit mode)
    int sdmmcD2 = -1;                      // GPIO pin for D2 (optional for 1-bit mode)
    int sdmmcD3 = -1;                      // GPIO pin for D3 (optional for 1-bit mode)
    int sdmmcBusWidth = 4;                 // Bus width: 1 or 4

    /**
     * @brief Set SD card mount point
     *
     * @param path Mount point path (e.g., "/sdcard")
     * @return Reference to this config for chaining
     */
    SdCardConfig &setMountPoint(const std::string &path)
    {
        mountPoint = path;
        return *this;
    }

    /**
     * @brief Set maximum number of open files
     *
     * @param max Maximum simultaneous open files
     * @return Reference to this config for chaining
     */
    SdCardConfig &setMaxFiles(size_t max)
    {
        maxFiles = max;
        return *this;
    }

    /**
     * @brief Enable automatic format on mount failure
     *
     * @param format True to format if mount fails
     * @return Reference to this config for chaining
     */
    SdCardConfig &setFormatIfFailed(bool format)
    {
        formatIfFailed = format;
        return *this;
    }

    /**
     * @brief Set FAT allocation unit size
     *
     * @param size Allocation unit size in bytes (default 16KB)
     * @return Reference to this config for chaining
     */
    SdCardConfig &setAllocationUnitSize(size_t size)
    {
        allocationUnitSize = size;
        return *this;
    }

    /**
     * @brief Set SD card interface mode
     *
     * @param useSdmmcMode True for SDMMC mode (faster), false for SPI mode
     * @return Reference to this config for chaining
     */
    SdCardConfig &setUseSdmmc(bool useSdmmcMode)
    {
        useSdmmc = useSdmmcMode;
        return *this;
    }

    /**
     * @brief Configure SPI pins for SD card (SPI mode only)
     *
     * @param mosi MOSI GPIO pin
     * @param miso MISO GPIO pin
     * @param clk CLK GPIO pin
     * @param cs CS GPIO pin
     * @return Reference to this config for chaining
     */
    SdCardConfig &setSpiPins(int mosi, int miso, int clk, int cs)
    {
        spiMosi = mosi;
        spiMiso = miso;
        spiClk = clk;
        spiCs = cs;
        useSdmmc = false; // Explicitly set to SPI mode
        return *this;
    }

    /**
     * @brief Configure SDMMC pins for SD card (SDMMC mode only)
     *
     * @param clk CLK GPIO pin
     * @param cmd CMD GPIO pin
     * @param d0 D0 GPIO pin
     * @param d1 D1 GPIO pin (use -1 for 1-bit mode)
     * @param d2 D2 GPIO pin (use -1 for 1-bit mode)
     * @param d3 D3 GPIO pin (use -1 for 1-bit mode)
     * @return Reference to this config for chaining
     */
    SdCardConfig &setSdmmcPins(int clk, int cmd, int d0, int d1 = -1, int d2 = -1, int d3 = -1)
    {
        sdmmcClk = clk;
        sdmmcCmd = cmd;
        sdmmcD0 = d0;
        sdmmcD1 = d1;
        sdmmcD2 = d2;
        sdmmcD3 = d3;
        useSdmmc = true; // Explicitly set to SDMMC mode
        
        // Determine bus width based on pins provided
        if (d1 == -1 && d2 == -1 && d3 == -1)
        {
            sdmmcBusWidth = 1; // 1-bit mode
        }
        else
        {
            sdmmcBusWidth = 4; // 4-bit mode
        }
        
        return *this;
    }

    /**
     * @brief Set SDMMC bus width explicitly
     *
     * @param width Bus width (1 or 4)
     * @return Reference to this config for chaining
     */
    SdCardConfig &setSdmmcBusWidth(int width)
    {
        sdmmcBusWidth = width;
        return *this;
    }
};

/**
 * @brief LittleFS filesystem configuration
 *
 * Configuration for LittleFS storage backend. Use builder pattern:
 *
 * @code
 * LittleFsConfig config;
 * config.setBasePath("/littlefs")
 *       .setPartitionLabel("littlefs")
 *       .setFormatIfFailed(true)
 *       .setGrowOnMount(true);
 *
 * LittleFsStorage storage(config);
 * @endcode
 */
struct LittleFsConfig
{
    std::string basePath = "/littlefs";
    std::string partitionLabel = "littlefs";
    bool formatIfFailed = false;
    bool growOnMount = false; // Automatically grow filesystem to partition size

    /**
     * @brief Set LittleFS mount point path
     *
     * @param path Mount point (e.g., "/littlefs")
     * @return Reference to this config for chaining
     */
    LittleFsConfig &setBasePath(const std::string &path)
    {
        basePath = path;
        return *this;
    }

    /**
     * @brief Set partition label
     *
     * @param label Partition label from partition table
     * @return Reference to this config for chaining
     */
    LittleFsConfig &setPartitionLabel(const std::string &label)
    {
        partitionLabel = label;
        return *this;
    }

    /**
     * @brief Enable automatic format on mount failure
     *
     * @param format True to format if mount fails
     * @return Reference to this config for chaining
     */
    LittleFsConfig &setFormatIfFailed(bool format)
    {
        formatIfFailed = format;
        return *this;
    }

    /**
     * @brief Enable automatic filesystem growth on mount
     *
     * @param grow True to grow filesystem to partition size
     * @return Reference to this config for chaining
     */
    LittleFsConfig &setGrowOnMount(bool grow)
    {
        growOnMount = grow;
        return *this;
    }
};

} // namespace storage
} // namespace lopcore
