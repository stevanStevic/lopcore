#include "lopcore/prov/storage_helpers.hpp"
#include "lopcore/storage/nvs_storage.hpp"
#include "lopcore/storage/spiffs_storage.hpp"
#include "lopcore/storage/littlefs_storage.hpp"
#include "lopcore/storage/sdcard_storage.hpp"

namespace lopcore {
namespace prov {

StorageCallbacks nvsStorage(std::shared_ptr<NvsStorage> storage) {
    return StorageCallbacks {
        // Write function: NVS uses key-value pairs
        .write = [storage](const std::string& key, const std::string& value) {
            return storage->write(key, value);
        },
        // Read function: Returns std::optional<string>
        .read = [storage](const std::string& key) {
            return storage->read(key);
        }
    };
}

StorageCallbacks spiffsStorage(std::shared_ptr<SpiffsStorage> storage) {
    return StorageCallbacks {
        // Write function: SPIFFS uses filename (key becomes filename)
        .write = [storage](const std::string& filename, const std::string& content) {
            return storage->write(filename, content);
        },
        // Read function: SPIFFS reads entire file
        .read = [storage](const std::string& filename) {
            return storage->read(filename);
        }
    };
}

StorageCallbacks littleFsStorage(std::shared_ptr<LittleFsStorage> storage) {
    return StorageCallbacks {
        // Write function: LittleFS uses filename (key becomes filename)
        .write = [storage](const std::string& filename, const std::string& content) {
            return storage->write(filename, content);
        },
        // Read function: LittleFS reads entire file
        .read = [storage](const std::string& filename) {
            return storage->read(filename);
        }
    };
}

StorageCallbacks sdCardStorage(std::shared_ptr<SdCardStorage> storage) {
    return StorageCallbacks {
        // Write function: SD card uses filename (key becomes filename)
        .write = [storage](const std::string& filename, const std::string& content) {
            return storage->write(filename, content);
        },
        // Read function: SD card reads entire file
        .read = [storage](const std::string& filename) {
            return storage->read(filename);
        }
    };
}

} // namespace prov
} // namespace lopcore
