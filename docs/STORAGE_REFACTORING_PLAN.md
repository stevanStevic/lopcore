# Storage Refactoring Plan - Standalone Components

**Date:** November 4, 2025  
**Status:** 📋 PLANNING  
**Goal:** Refactor storage components to use standalone pattern (no factory, no interface)

---

## Executive Summary

Following the successful MQTT client refactoring, we will apply the same standalone component pattern to storage backends. This eliminates the `IStorage` interface and `StorageFactory`, replacing them with direct construction and type traits.

### Key Changes

- ❌ Remove `IStorage` interface (virtual functions)
- ❌ Remove `StorageFactory` 
- ✅ Keep `NvsStorage` and `SpiffsStorage` as standalone classes
- ✅ Add `storage_traits.hpp` for compile-time capability detection
- ✅ Enable direct construction with explicit configuration

---

## Problem Analysis

### Current Architecture Issues

```cpp
// ❌ Current: Factory + Interface
auto storage = StorageFactory::createSpiffs("/spiffs");
storage->write("config.json", data);  // Virtual function call

// Problems:
// 1. Virtual function overhead (~5-10 cycles per call)
// 2. Forces common interface (loses unique features)
// 3. Runtime errors for unsupported operations
// 4. Cannot easily integrate 3rd party storage
```

### Storage Mismatch Problem

Different storage backends have **fundamentally different** capabilities:

| Feature | SPIFFS | NVS | LittleFS | SD Card |
|---------|--------|-----|----------|---------|
| File operations | ✅ | ❌ | ✅ | ✅ |
| Key-value | ❌ | ✅ | ❌ | ❌ |
| Directories | ❌ | ❌ | ✅ | ✅ |
| Typed reads (U32, blob) | ❌ | ✅ | ❌ | ❌ |
| Commit() | ❌ | ✅ | ❌ | ❌ |
| Format | ✅ | ✅ | ✅ | ❌ |

**Current interface tries to unify these** → Results in `ESP_ERR_NOT_SUPPORTED` at runtime!

```cpp
class IStorage {
    virtual bool mkdir(const std::string& path) = 0;  // SPIFFS can't do this!
};

class SpiffsStorage : public IStorage {
    bool mkdir(const std::string& path) override {
        return false;  // ❌ Runtime error!
    }
};
```

---

## Proposed Architecture

### 1. Standalone Storage Classes

Each storage backend is a **standalone class** with its **natural interface**:

```cpp
namespace lopcore::storage {

// ========================================
// SPIFFS - File-based storage (flat)
// ========================================
class SpiffsStorage {
public:
    // Construction with explicit configuration
    explicit SpiffsStorage(const SpiffsConfig& config);
    
    // File operations
    esp_err_t read(const std::string& path, std::vector<uint8_t>& data);
    esp_err_t write(const std::string& path, const std::vector<uint8_t>& data);
    esp_err_t readString(const std::string& path, std::string& data);
    esp_err_t writeString(const std::string& path, const std::string& data);
    
    // Metadata
    esp_err_t exists(const std::string& path, bool& exists);
    esp_err_t getSize(const std::string& path, size_t& size);
    esp_err_t remove(const std::string& path);
    
    // Filesystem operations
    esp_err_t format();
    esp_err_t getUsage(size_t& total, size_t& used);
    
    // Configuration
    const SpiffsConfig& getConfig() const;
    
    // NO mkdir() - SPIFFS is flat filesystem
    // NO commit() - not needed
};

// ========================================
// NVS - Key-value storage
// ========================================
class NvsStorage {
public:
    // Construction with explicit configuration
    explicit NvsStorage(const NvsConfig& config);
    
    // String operations
    esp_err_t readString(const std::string& key, std::string& value);
    esp_err_t writeString(const std::string& key, const std::string& value);
    
    // Typed operations (NVS-specific!)
    esp_err_t readU8(const std::string& key, uint8_t& value);
    esp_err_t writeU8(const std::string& key, uint8_t value);
    esp_err_t readU32(const std::string& key, uint32_t& value);
    esp_err_t writeU32(const std::string& key, uint32_t value);
    esp_err_t readBlob(const std::string& key, std::vector<uint8_t>& data);
    esp_err_t writeBlob(const std::string& key, const std::vector<uint8_t>& data);
    
    // NVS operations
    esp_err_t exists(const std::string& key, bool& exists);
    esp_err_t erase(const std::string& key);
    esp_err_t eraseAll();
    esp_err_t commit();  // NVS-specific!
    
    // Configuration
    const NvsConfig& getConfig() const;
    
    // NO read(path, data) - not a filesystem
    // NO mkdir() - no directories
    // NO getSize() - keys don't have sizes
};

} // namespace lopcore::storage
```

### 2. Configuration Structures

Replace factory parameters with explicit configuration:

```cpp
namespace lopcore::storage {

/**
 * @brief SPIFFS configuration
 */
struct SpiffsConfig {
    std::string basePath = "/spiffs";
    std::string partitionLabel = "storage";
    size_t maxFiles = 5;
    bool formatIfFailed = false;
    
    // Builder pattern
    SpiffsConfig& setBasePath(const std::string& path) {
        basePath = path;
        return *this;
    }
    
    SpiffsConfig& setFormatIfFailed(bool format) {
        formatIfFailed = format;
        return *this;
    }
    
    // ... other setters
};

/**
 * @brief NVS configuration
 */
struct NvsConfig {
    std::string namespaceName = "lopcore";
    bool readOnly = false;
    
    // Builder pattern
    NvsConfig& setNamespace(const std::string& ns) {
        namespaceName = ns;
        return *this;
    }
    
    NvsConfig& setReadOnly(bool ro) {
        readOnly = ro;
        return *this;
    }
};

} // namespace lopcore::storage
```

### 3. Type Traits System

Compile-time capability detection:

```cpp
namespace lopcore::storage::traits {

// ========================================
// Trait 1: File-based storage
// ========================================
template<typename T, typename = void>
struct is_file_based : std::false_type {};

template<typename T>
struct is_file_based<T, std::void_t<
    decltype(std::declval<T>().read(
        std::declval<const std::string&>(),
        std::declval<std::vector<uint8_t>&>()
    )),
    decltype(std::declval<T>().write(
        std::declval<const std::string&>(),
        std::declval<const std::vector<uint8_t>&>()
    ))
>> : std::true_type {};

template<typename T>
inline constexpr bool is_file_based_v = is_file_based<T>::value;

// ========================================
// Trait 2: Key-value storage
// ========================================
template<typename T, typename = void>
struct is_key_value : std::false_type {};

template<typename T>
struct is_key_value<T, std::void_t<
    decltype(std::declval<T>().readString(
        std::declval<const std::string&>(),
        std::declval<std::string&>()
    )),
    decltype(std::declval<T>().writeString(
        std::declval<const std::string&>(),
        std::declval<const std::string&>()
    )),
    decltype(std::declval<T>().erase(std::declval<const std::string&>()))
>> : std::true_type {};

template<typename T>
inline constexpr bool is_key_value_v = is_key_value<T>::value;

// ========================================
// Trait 3: Typed operations (NVS)
// ========================================
template<typename T, typename = void>
struct has_typed_operations : std::false_type {};

template<typename T>
struct has_typed_operations<T, std::void_t<
    decltype(std::declval<T>().readU32(
        std::declval<const std::string&>(),
        std::declval<uint32_t&>()
    )),
    decltype(std::declval<T>().writeU32(
        std::declval<const std::string&>(),
        std::declval<uint32_t>()
    )),
    decltype(std::declval<T>().readBlob(
        std::declval<const std::string&>(),
        std::declval<std::vector<uint8_t>&>()
    ))
>> : std::true_type {};

template<typename T>
inline constexpr bool has_typed_operations_v = has_typed_operations<T>::value;

// ========================================
// Trait 4: Commit support (NVS)
// ========================================
template<typename T, typename = void>
struct requires_commit : std::false_type {};

template<typename T>
struct requires_commit<T, std::void_t<
    decltype(std::declval<T>().commit())
>> : std::true_type {};

template<typename T>
inline constexpr bool requires_commit_v = requires_commit<T>::value;

// ========================================
// Trait 5: Format support
// ========================================
template<typename T, typename = void>
struct supports_format : std::false_type {};

template<typename T>
struct supports_format<T, std::void_t<
    decltype(std::declval<T>().format())
>> : std::true_type {};

template<typename T>
inline constexpr bool supports_format_v = supports_format<T>::value;

} // namespace lopcore::storage::traits
```

### 4. Usage Examples

#### Before (Factory + Interface):

```cpp
// ❌ Old way
auto storage = StorageFactory::createSpiffs("/spiffs");
storage->write("config.json", data);
storage->read("config.json");
```

#### After (Standalone):

```cpp
// ✅ New way - explicit construction
SpiffsConfig config;
config.setBasePath("/spiffs")
      .setFormatIfFailed(true);

SpiffsStorage storage(config);
storage.write("config.json", data);
storage.read("config.json");
```

#### Generic Algorithm Example:

```cpp
// Works with ANY storage that supports string operations
template<typename Storage>
class ConfigManager {
public:
    static_assert(
        traits::is_file_based_v<Storage> || traits::is_key_value_v<Storage>,
        "Storage must support string read/write"
    );
    
    explicit ConfigManager(Storage& storage) : storage_(storage) {}
    
    esp_err_t loadConfig(AppConfig& config) {
        if constexpr (traits::is_file_based_v<Storage>) {
            // File-based: read JSON file
            std::string json;
            auto err = storage_.readString("/config.json", json);
            if (err == ESP_OK) {
                parseJson(json, config);
            }
            return err;
            
        } else if constexpr (traits::is_key_value_v<Storage>) {
            // Key-value: read individual keys
            storage_.readString("wifi.ssid", config.wifiSsid);
            storage_.readString("mqtt.endpoint", config.mqttEndpoint);
            
            // Commit if needed
            if constexpr (traits::requires_commit_v<Storage>) {
                storage_.commit();
            }
            return ESP_OK;
        }
    }
    
private:
    Storage& storage_;
};

// Usage - same code works with different storage!
SpiffsStorage spiffs(SpiffsConfig{});
ConfigManager<SpiffsStorage> fileConfig(spiffs);

NvsStorage nvs(NvsConfig{});
ConfigManager<NvsStorage> nvsConfig(nvs);
```

---

## Migration Plan

### Phase 1: Add Standalone Versions (Non-Breaking)

**Goal:** Add new standalone storage classes alongside existing code

**Tasks:**

1. ✅ **Add configuration structures**
   - Create `SpiffsConfig` struct
   - Create `NvsConfig` struct
   - Add builder pattern methods

2. ✅ **Update storage classes**
   - Add explicit constructors taking config structs
   - Keep backward compatibility with existing constructors
   - Mark old constructors as `[[deprecated]]`

3. ✅ **Create storage_traits.hpp**
   - Implement trait: `is_file_based_v`
   - Implement trait: `is_key_value_v`
   - Implement trait: `has_typed_operations_v`
   - Implement trait: `requires_commit_v`
   - Implement trait: `supports_format_v`

4. ✅ **Add unit tests**
   - Test configuration structs
   - Test trait detection (compile-time)
   - Test direct construction

**Deliverables:**

- `include/lopcore/storage/storage_config.hpp` (NEW)
- `include/lopcore/storage/storage_traits.hpp` (NEW)
- Updated `nvs_storage.hpp` with new constructor
- Updated `spiffs_storage.hpp` with new constructor
- `test/unit/storage/test_storage_traits.cpp` (NEW)

**Status:** ⏳ TODO

---

### Phase 2: Deprecate Factory (Non-Breaking)

**Goal:** Mark factory as deprecated, update examples

**Tasks:**

1. ✅ **Mark factory as deprecated**
   - Add `[[deprecated]]` attribute to `StorageFactory` class
   - Add deprecation warnings in comments
   - Update documentation to recommend direct construction

2. ✅ **Update examples**
   - Migrate example 02 (storage basics) to direct construction
   - Add example showing generic algorithms with traits
   - Show before/after comparison

3. ✅ **Update documentation**
   - Document standalone pattern in README
   - Add migration guide
   - Show trait usage examples

**Deliverables:**

- Updated `storage_factory.hpp` with deprecation
- Migrated `examples/02_storage_basics/`
- Updated `docs/STORAGE_MIGRATION_GUIDE.md` (NEW)

**Status:** ⏳ TODO

---

### Phase 3: Remove Interface (Breaking Change)

**Goal:** Remove `IStorage` interface from storage classes

**Tasks:**

1. ✅ **Remove IStorage inheritance**
   - Update `NvsStorage` - remove `: public IStorage`
   - Update `SpiffsStorage` - remove `: public IStorage`
   - Keep all public methods (no API change)

2. ✅ **Update factory**
   - Change return type from `std::unique_ptr<IStorage>` to specific types
   - Or keep factory for convenience but return concrete types

3. ✅ **Update all client code**
   - Search for `IStorage*` or `std::unique_ptr<IStorage>`
   - Replace with concrete types or templates
   - Update function signatures

4. ✅ **Remove virtual functions**
   - Remove `virtual` keyword from all methods
   - Remove `override` keyword from implementations
   - Remove virtual destructors

**Deliverables:**

- Updated `nvs_storage.hpp` (no inheritance)
- Updated `spiffs_storage.hpp` (no inheritance)
- Updated all client code in main application

**Status:** ⏳ TODO

---

### Phase 4: Remove Factory (Breaking Change)

**Goal:** Complete removal of factory pattern

**Tasks:**

1. ✅ **Delete factory files**
   - Delete `include/lopcore/storage/storage_factory.hpp`
   - Delete `src/storage/storage_factory.cpp`
   - Delete `test/unit/storage/test_storage_factory.cpp`

2. ✅ **Update lopcore.hpp**
   - Remove factory include
   - Add storage_config.hpp include
   - Add storage_traits.hpp include

3. ✅ **Update CMake**
   - Remove factory source from build
   - Remove factory tests

4. ✅ **Final cleanup**
   - Remove any remaining factory references
   - Update all documentation
   - Run full test suite

**Deliverables:**

- Deleted factory files
- Updated `include/lopcore.hpp`
- Updated build files
- All tests passing

**Status:** ⏳ TODO

---

## Detailed Implementation

### File: `include/lopcore/storage/storage_config.hpp`

```cpp
/**
 * @file storage_config.hpp
 * @brief Configuration structures for storage backends
 *
 * Provides type-safe configuration with builder pattern.
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#pragma once

#include <string>
#include <cstddef>

namespace lopcore::storage {

/**
 * @brief SPIFFS filesystem configuration
 */
struct SpiffsConfig {
    std::string basePath = "/spiffs";
    std::string partitionLabel = "storage";
    size_t maxFiles = 5;
    bool formatIfFailed = false;
    
    // Builder pattern
    SpiffsConfig& setBasePath(const std::string& path) {
        basePath = path;
        return *this;
    }
    
    SpiffsConfig& setPartitionLabel(const std::string& label) {
        partitionLabel = label;
        return *this;
    }
    
    SpiffsConfig& setMaxFiles(size_t max) {
        maxFiles = max;
        return *this;
    }
    
    SpiffsConfig& setFormatIfFailed(bool format) {
        formatIfFailed = format;
        return *this;
    }
};

/**
 * @brief NVS (Non-Volatile Storage) configuration
 */
struct NvsConfig {
    std::string namespaceName = "lopcore";
    bool readOnly = false;
    
    // Builder pattern
    NvsConfig& setNamespace(const std::string& ns) {
        namespaceName = ns;
        return *this;
    }
    
    NvsConfig& setReadOnly(bool ro) {
        readOnly = ro;
        return *this;
    }
};

} // namespace lopcore::storage
```

### File: `include/lopcore/storage/storage_traits.hpp`

```cpp
/**
 * @file storage_traits.hpp
 * @brief Type traits for compile-time storage capability detection
 *
 * Uses SFINAE to detect storage backend capabilities at compile time.
 * Enables generic algorithms that adapt to storage features.
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#pragma once

#include <type_traits>
#include <string>
#include <vector>

namespace lopcore::storage::traits {

// ========================================
// Trait: File-based storage
// ========================================

/**
 * @brief Detects if storage supports file-based operations
 * 
 * Checks for: read(), write(), exists()
 */
template<typename T, typename = void>
struct is_file_based : std::false_type {};

template<typename T>
struct is_file_based<T, std::void_t<
    decltype(std::declval<T>().read(
        std::declval<const std::string&>(),
        std::declval<std::vector<uint8_t>&>()
    )),
    decltype(std::declval<T>().write(
        std::declval<const std::string&>(),
        std::declval<const std::vector<uint8_t>&>()
    )),
    decltype(std::declval<T>().exists(
        std::declval<const std::string&>(),
        std::declval<bool&>()
    ))
>> : std::true_type {};

template<typename T>
inline constexpr bool is_file_based_v = is_file_based<T>::value;

// ========================================
// Trait: Key-value storage
// ========================================

/**
 * @brief Detects if storage supports key-value operations
 * 
 * Checks for: readString(), writeString(), erase()
 */
template<typename T, typename = void>
struct is_key_value : std::false_type {};

template<typename T>
struct is_key_value<T, std::void_t<
    decltype(std::declval<T>().readString(
        std::declval<const std::string&>(),
        std::declval<std::string&>()
    )),
    decltype(std::declval<T>().writeString(
        std::declval<const std::string&>(),
        std::declval<const std::string&>()
    )),
    decltype(std::declval<T>().erase(std::declval<const std::string&>()))
>> : std::true_type {};

template<typename T>
inline constexpr bool is_key_value_v = is_key_value<T>::value;

// ========================================
// Trait: Typed operations (NVS specific)
// ========================================

/**
 * @brief Detects if storage supports typed read/write (U8, U32, blob)
 * 
 * Checks for: readU32(), writeU32(), readBlob(), writeBlob()
 */
template<typename T, typename = void>
struct has_typed_operations : std::false_type {};

template<typename T>
struct has_typed_operations<T, std::void_t<
    decltype(std::declval<T>().readU32(
        std::declval<const std::string&>(),
        std::declval<uint32_t&>()
    )),
    decltype(std::declval<T>().writeU32(
        std::declval<const std::string&>(),
        std::declval<uint32_t>()
    )),
    decltype(std::declval<T>().readBlob(
        std::declval<const std::string&>(),
        std::declval<std::vector<uint8_t>&>()
    )),
    decltype(std::declval<T>().writeBlob(
        std::declval<const std::string&>(),
        std::declval<const std::vector<uint8_t>&>()
    ))
>> : std::true_type {};

template<typename T>
inline constexpr bool has_typed_operations_v = has_typed_operations<T>::value;

// ========================================
// Trait: Commit support (NVS specific)
// ========================================

/**
 * @brief Detects if storage requires explicit commit()
 * 
 * Checks for: commit()
 */
template<typename T, typename = void>
struct requires_commit : std::false_type {};

template<typename T>
struct requires_commit<T, std::void_t<
    decltype(std::declval<T>().commit())
>> : std::true_type {};

template<typename T>
inline constexpr bool requires_commit_v = requires_commit<T>::value;

// ========================================
// Trait: Format support
// ========================================

/**
 * @brief Detects if storage supports format operation
 * 
 * Checks for: format()
 */
template<typename T, typename = void>
struct supports_format : std::false_type {};

template<typename T>
struct supports_format<T, std::void_t<
    decltype(std::declval<T>().format())
>> : std::true_type {};

template<typename T>
inline constexpr bool supports_format_v = supports_format<T>::value;

} // namespace lopcore::storage::traits
```

---

## Testing Strategy

### Unit Tests

```cpp
// test/unit/storage/test_storage_traits.cpp

#include "lopcore/storage/storage_traits.hpp"
#include "lopcore/storage/nvs_storage.hpp"
#include "lopcore/storage/spiffs_storage.hpp"

using namespace lopcore::storage;
using namespace lopcore::storage::traits;

// Compile-time trait verification
static_assert(is_file_based_v<SpiffsStorage>, "SPIFFS should be file-based");
static_assert(!is_file_based_v<NvsStorage>, "NVS is not file-based");

static_assert(!is_key_value_v<SpiffsStorage>, "SPIFFS is not key-value");
static_assert(is_key_value_v<NvsStorage>, "NVS should be key-value");

static_assert(!has_typed_operations_v<SpiffsStorage>, "SPIFFS has no typed ops");
static_assert(has_typed_operations_v<NvsStorage>, "NVS should have typed ops");

static_assert(!requires_commit_v<SpiffsStorage>, "SPIFFS doesn't need commit");
static_assert(requires_commit_v<NvsStorage>, "NVS requires commit");

static_assert(supports_format_v<SpiffsStorage>, "SPIFFS supports format");
static_assert(supports_format_v<NvsStorage>, "NVS supports format");
```

---

## Benefits

### Performance

- **Zero overhead** - Direct function calls, no virtual dispatch
- **Inlining** - Compiler can inline storage operations
- **Dead code elimination** - Unused features removed at compile time

### Type Safety

- **Compile-time errors** - Can't call unsupported operations
- **No runtime checks** - No `ESP_ERR_NOT_SUPPORTED` surprises
- **Clear APIs** - Each storage has its natural interface

### Flexibility

- **Preserves unique features** - NVS typed operations, SPIFFS format, etc.
- **Easy integration** - Wrap 3rd party storage without refactoring
- **Generic algorithms** - Write once, works with any compatible storage

### Maintainability

- **Less code** - No factory, no interface
- **Simpler** - Direct construction, explicit configuration
- **Testable** - Mock storages by template substitution

---

## Next Steps

1. **Review and approve** this plan
2. **Implement Phase 1** - Add standalone versions alongside existing code
3. **Test thoroughly** - Unit tests, integration tests, examples
4. **Implement Phase 2** - Deprecate factory, update documentation
5. **Implement Phase 3** - Remove interface inheritance
6. **Implement Phase 4** - Complete factory removal

---

## References

- [STANDALONE_COMPONENTS_ANALYSIS.md](./STANDALONE_COMPONENTS_ANALYSIS.md) - Original analysis
- [FACTORY_COMPLETE_REMOVAL.md](./FACTORY_COMPLETE_REMOVAL.md) - MQTT refactoring summary
- [MQTT Type Traits](../include/lopcore/mqtt/mqtt_traits.hpp) - Similar pattern for MQTT

---

**Last Updated:** November 4, 2025  
**Status:** 📋 Planning - awaiting approval to begin implementation
