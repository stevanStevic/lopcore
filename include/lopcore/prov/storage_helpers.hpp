#pragma once

#include "storage_types.hpp"
#include <memory>

// Forward declarations for lopcore storage classes
namespace lopcore {
    class NvsStorage;
    class SpiffsStorage;
    class LittleFsStorage;
    class SdCardStorage;
}

namespace lopcore {
namespace prov {

/**
 * Create storage callbacks for NVS (key/value storage)
 * 
 * NVS stores data as key-value pairs within a namespace.
 * This is ideal for configuration data like WiFi credentials or AWS endpoints.
 * 
 * The "key" parameter in write/read IS the actual NVS key.
 * 
 * @param storage Pre-initialized NVS storage instance (user must call init() first!)
 * @return Storage callbacks wrapping the NVS instance
 * 
 * Example:
 * ```cpp
 * auto nvs = std::make_shared<NvsStorage>("config", false);
 * nvs->init();  // YOU must initialize!
 * 
 * auto callbacks = nvsStorage(nvs);
 * callbacks.write("wifi.ssid", "MyNetwork");  // Stores key="wifi.ssid" in NVS namespace
 * auto ssid = callbacks.read("wifi.ssid");    // Reads from NVS namespace
 * ```
 */
StorageCallbacks nvsStorage(std::shared_ptr<NvsStorage> storage);

/**
 * Create storage callbacks for SPIFFS (file-based storage)
 * 
 * SPIFFS stores data as files. The "key" becomes the filename (relative to mount point).
 * Files are stored in the mounted SPIFFS partition.
 * 
 * The "key" parameter in write/read becomes the FILENAME.
 * 
 * @param storage Pre-initialized SPIFFS storage instance
 * @return Storage callbacks wrapping the SPIFFS instance
 * 
 * Example:
 * ```cpp
 * auto spiffs = std::make_shared<SpiffsStorage>("/spiffs");
 * spiffs->init();  // YOU must initialize!
 * 
 * auto callbacks = spiffsStorage(spiffs);
 * callbacks.write("wifi_ssid.txt", "MyNetwork");  // Creates file "/spiffs/wifi_ssid.txt"
 * auto ssid = callbacks.read("wifi_ssid.txt");    // Reads file "/spiffs/wifi_ssid.txt"
 * ```
 */
StorageCallbacks spiffsStorage(std::shared_ptr<SpiffsStorage> storage);

/**
 * Create storage callbacks for LittleFS (file-based storage)
 * 
 * LittleFS stores data as files. The "key" becomes the filename (relative to mount point).
 * Files are stored in the mounted LittleFS partition.
 * 
 * The "key" parameter in write/read becomes the FILENAME.
 * 
 * @param storage Pre-initialized LittleFS storage instance
 * @return Storage callbacks wrapping the LittleFS instance
 * 
 * Example:
 * ```cpp
 * auto lfs = std::make_shared<LittleFsStorage>("/littlefs");
 * lfs->init();  // YOU must initialize!
 * 
 * auto callbacks = littleFsStorage(lfs);
 * callbacks.write("aws_cert.pem", certPem);  // Creates file "/littlefs/aws_cert.pem"
 * auto cert = callbacks.read("aws_cert.pem"); // Reads file "/littlefs/aws_cert.pem"
 * ```
 */
StorageCallbacks littleFsStorage(std::shared_ptr<LittleFsStorage> storage);

/**
 * Create storage callbacks for SD Card (file-based storage)
 * 
 * SD card stores data as files. The "key" becomes the filename (relative to mount point).
 * Files are stored in the mounted SD card partition.
 * 
 * The "key" parameter in write/read becomes the FILENAME.
 * 
 * @param storage Pre-initialized SD card storage instance
 * @return Storage callbacks wrapping the SD card instance
 * 
 * Example:
 * ```cpp
 * auto sdcard = std::make_shared<SdCardStorage>("/sdcard");
 * sdcard->init();  // YOU must initialize!
 * 
 * auto callbacks = sdCardStorage(sdcard);
 * callbacks.write("logs/boot.log", logData);  // Creates file "/sdcard/logs/boot.log"
 * auto log = callbacks.read("logs/boot.log"); // Reads file "/sdcard/logs/boot.log"
 * ```
 */
StorageCallbacks sdCardStorage(std::shared_ptr<SdCardStorage> storage);

} // namespace prov
} // namespace lopcore
