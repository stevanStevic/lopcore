/**
 * @file test_storage_traits.cpp
 * @brief Unit tests for storage type traits
 *
 * Tests compile-time trait detection for storage capabilities.
 *
 * @copyright Copyright (c) 2025 LopCore Contributors
 * @license MIT License
 */

#include "lopcore/storage/nvs_storage.hpp"
#include "lopcore/storage/spiffs_storage.hpp"
#include "lopcore/storage/storage_traits.hpp"

#include <gtest/gtest.h>

using namespace lopcore;
using namespace lopcore::storage;
using namespace lopcore::storage::traits;

// ========================================
// Compile-Time Trait Verification
// ========================================

// File-based storage
static_assert(is_file_based_v<SpiffsStorage>, "SpiffsStorage should be file-based");
static_assert(!is_file_based_v<NvsStorage>, "NvsStorage should NOT be file-based");

// Key-value storage
static_assert(!is_key_value_v<SpiffsStorage>, "SpiffsStorage should NOT be key-value");
static_assert(is_key_value_v<NvsStorage>, "NvsStorage should be key-value");

// Typed operations (NVS specific)
static_assert(!has_typed_operations_v<SpiffsStorage>, "SpiffsStorage should NOT have typed operations");
static_assert(has_typed_operations_v<NvsStorage>, "NvsStorage should have typed operations");

// Commit support
static_assert(!requires_commit_v<SpiffsStorage>, "SpiffsStorage should NOT require commit");
static_assert(requires_commit_v<NvsStorage>, "NvsStorage should require commit");

// Format support
static_assert(supports_format_v<SpiffsStorage>, "SpiffsStorage should support format");
static_assert(supports_format_v<NvsStorage>, "NvsStorage should support format");

// String operations
static_assert(supports_strings_v<SpiffsStorage>, "SpiffsStorage should support strings");
static_assert(supports_strings_v<NvsStorage>, "NvsStorage should support strings");

// ========================================
// Runtime Tests (Documentation)
// ========================================

TEST(StorageTraitsTest, SpiffsTraits)
{
    // Verify SpiffsStorage traits at runtime
    EXPECT_TRUE(is_file_based_v<SpiffsStorage>);
    EXPECT_FALSE(is_key_value_v<SpiffsStorage>);
    EXPECT_FALSE(has_typed_operations_v<SpiffsStorage>);
    EXPECT_FALSE(requires_commit_v<SpiffsStorage>);
    EXPECT_TRUE(supports_format_v<SpiffsStorage>);
    EXPECT_TRUE(supports_strings_v<SpiffsStorage>);
}

TEST(StorageTraitsTest, NvsTraits)
{
    // Verify NvsStorage traits at runtime
    EXPECT_FALSE(is_file_based_v<NvsStorage>);
    EXPECT_TRUE(is_key_value_v<NvsStorage>);
    EXPECT_TRUE(has_typed_operations_v<NvsStorage>);
    EXPECT_TRUE(requires_commit_v<NvsStorage>);
    EXPECT_TRUE(supports_format_v<NvsStorage>);
    EXPECT_TRUE(supports_strings_v<NvsStorage>);
}

// ========================================
// Generic Algorithm Example
// ========================================

/**
 * @brief Example generic config manager that adapts to storage type
 */
template <typename Storage>
class TestConfigManager
{
public:
    static_assert(supports_strings_v<Storage>, "Storage must support string operations");

    explicit TestConfigManager(Storage &storage) : storage_(storage)
    {
    }

    bool saveConfig(const std::string &key, const std::string &value)
    {
        auto result = storage_.writeString(key, value);

        // Commit if storage requires it (NVS only)
        if constexpr (requires_commit_v<Storage>)
        {
            return result == ESP_OK && storage_.commit() == ESP_OK;
        }

        return result == ESP_OK;
    }

    bool loadConfig(const std::string &key, std::string &value)
    {
        return storage_.readString(key, value) == ESP_OK;
    }

private:
    Storage &storage_;
};

TEST(StorageTraitsTest, GenericAlgorithmWithSpiffs)
{
    // Generic algorithm works with SPIFFS
    SpiffsStorage spiffs;
    TestConfigManager<SpiffsStorage> manager(spiffs);

    // Note: This test just verifies compilation
    // Actual functionality tested in integration tests
}

TEST(StorageTraitsTest, GenericAlgorithmWithNvs)
{
    // Same generic algorithm works with NVS
    NvsStorage nvs;
    TestConfigManager<NvsStorage> manager(nvs);

    // Note: This test just verifies compilation
    // The template automatically handles commit() for NVS
}

// ========================================
// Configuration Tests
// ========================================

TEST(StorageConfigTest, SpiffsConfigBuilder)
{
    SpiffsConfig config;
    config.setBasePath("/custom")
        .setPartitionLabel("my_partition")
        .setMaxFiles(10)
        .setFormatIfFailed(true);

    EXPECT_EQ(config.basePath, "/custom");
    EXPECT_EQ(config.partitionLabel, "my_partition");
    EXPECT_EQ(config.maxFiles, 10);
    EXPECT_TRUE(config.formatIfFailed);
}

TEST(StorageConfigTest, SpiffsConfigDefaults)
{
    SpiffsConfig config;

    EXPECT_EQ(config.basePath, "/spiffs");
    EXPECT_EQ(config.partitionLabel, "storage");
    EXPECT_EQ(config.maxFiles, 5);
    EXPECT_FALSE(config.formatIfFailed);
}

TEST(StorageConfigTest, NvsConfigBuilder)
{
    NvsConfig config;
    config.setNamespace("my_app").setReadOnly(true);

    EXPECT_EQ(config.namespaceName, "my_app");
    EXPECT_TRUE(config.readOnly);
}

TEST(StorageConfigTest, NvsConfigDefaults)
{
    NvsConfig config;

    EXPECT_EQ(config.namespaceName, "lopcore");
    EXPECT_FALSE(config.readOnly);
}

// ========================================
// Construction Tests
// ========================================

TEST(StorageConstructionTest, SpiffsWithConfig)
{
    SpiffsConfig config;
    config.setBasePath("/test_spiffs");

    // Should compile and construct
    SpiffsStorage storage(config);

    // Note: Actual initialization tested in integration tests
}

TEST(StorageConstructionTest, NvsWithConfig)
{
    NvsConfig config;
    config.setNamespace("test_ns");

    // Should compile and construct
    NvsStorage storage(config);

    // Note: Actual initialization tested in integration tests
}

TEST(StorageConstructionTest, SpiffsDeprecatedConstructor)
{
    // Old constructor still works (but deprecated)
    // Suppress deprecation warnings for this test
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    SpiffsStorage storage("/test");
#pragma GCC diagnostic pop

    // Should still compile
}

TEST(StorageConstructionTest, NvsDeprecatedConstructor)
{
    // Old constructor still works (but deprecated)
    // Suppress deprecation warnings for this test
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    NvsStorage storage("test_ns");
#pragma GCC diagnostic pop

    // Should still compile
}
