/**
 * @file storage_type.hpp
 * @brief Storage backend type enumeration for LopCore
 *
 * Defines the types of storage backends available in the system.
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

namespace lopcore
{

/**
 * @brief Storage backend types supported by LopCore
 *
 * Different storage backends have different characteristics:
 * - SPIFFS: Good for files, logs, certificates (slow writes, fast reads)
 * - NVS: Good for config, key-value pairs (fast, encrypted, limited size)
 * - SDCARD: Good for large media files (removable, large capacity)
 * - LITTLEFS: Good for embedded flash storage (wear leveling, power-loss resilient)
 */
enum class StorageType
{
    SPIFFS,   ///< SPI Flash File System (internal flash, file-based)
    NVS,      ///< Non-Volatile Storage (internal flash, key-value store)
    SDCARD,   ///< External SD card (removable, file-based, FAT)
    LITTLEFS, ///< LittleFS (internal flash, file-based, power-loss resilient)
};

} // namespace lopcore
