/**
 * @file test_nvs_storage.cpp
 * @brief Unit tests for NvsStorage
 */

#include <gtest/gtest.h>

#include "storage/nvs_storage.hpp"

using namespace lopcore;

// Test fixture for NvsStorage
class NvsStorageTest : public ::testing::Test
{
protected:
    std::unique_ptr<NvsStorage> storage;

    void SetUp() override
    {
        storage = std::make_unique<NvsStorage>("test_namespace");
    }

    void TearDown() override
    {
        // Clean up namespace
        if (storage)
        {
            storage->eraseNamespace();
        }
        storage.reset();
    }
};

// Test 1: Constructor initializes successfully
TEST_F(NvsStorageTest, Constructor_InitializesSuccessfully)
{
    ASSERT_NE(storage, nullptr);
    EXPECT_EQ(storage->getType(), StorageType::NVS);
    EXPECT_EQ(storage->getNamespace(), "test_namespace");
}

// Test 2: Write string data returns true
TEST_F(NvsStorageTest, Write_ValidStringData_ReturnsTrue)
{
    std::string key = "wifi_ssid";
    std::string data = "MyNetwork";

    bool result = storage->write(key, data);

    EXPECT_TRUE(result);
    EXPECT_TRUE(storage->exists(key));
}

// Test 3: Read existing key returns data
TEST_F(NvsStorageTest, Read_ExistingKey_ReturnsData)
{
    std::string key = "mqtt_broker";
    std::string expectedData = "mqtt.example.com";

    storage->write(key, expectedData);
    auto result = storage->read(key);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, expectedData);
}

// Test 4: Read non-existent key returns nullopt
TEST_F(NvsStorageTest, Read_NonExistentKey_ReturnsNullopt)
{
    auto result = storage->read("nonexistent_key");

    EXPECT_FALSE(result.has_value());
}

// Test 5: Exists returns true for existing key
TEST_F(NvsStorageTest, Exists_ExistingKey_ReturnsTrue)
{
    std::string key = "test_key";
    storage->write(key, "test value");

    bool result = storage->exists(key);

    EXPECT_TRUE(result);
}

// Test 6: Exists returns false for non-existent key
TEST_F(NvsStorageTest, Exists_NonExistentKey_ReturnsFalse)
{
    bool result = storage->exists("does_not_exist");

    EXPECT_FALSE(result);
}

// Test 7: Remove existing key returns true
TEST_F(NvsStorageTest, Remove_ExistingKey_ReturnsTrue)
{
    std::string key = "to_remove";
    storage->write(key, "data");

    bool result = storage->remove(key);

    EXPECT_TRUE(result);
    EXPECT_FALSE(storage->exists(key));
}

// Test 8: Write binary data
TEST_F(NvsStorageTest, WriteBinary_ValidData_ReturnsTrue)
{
    std::string key = "binary_key";
    std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC, 0xDD};

    bool result = storage->write(key, data);

    EXPECT_TRUE(result);
    EXPECT_TRUE(storage->exists(key));
}

// Test 9: Read binary data
TEST_F(NvsStorageTest, ReadBinary_ExistingKey_ReturnsData)
{
    std::string key = "cert_data";
    std::vector<uint8_t> expectedData = {0x01, 0x02, 0x03, 0x04};

    storage->write(key, expectedData);
    auto result = storage->readBinary(key);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, expectedData);
}

// Test 10: Invalid key (too long) returns false
TEST_F(NvsStorageTest, Write_TooLongKey_ReturnsFalse)
{
    std::string key = "this_key_is_way_too_long_for_nvs"; // > 15 chars
    std::string data = "test";

    bool result = storage->write(key, data);

    EXPECT_FALSE(result);
}

// Test 11: Empty key returns false
TEST_F(NvsStorageTest, Write_EmptyKey_ReturnsFalse)
{
    std::string key = "";
    std::string data = "test";

    bool result = storage->write(key, data);

    EXPECT_FALSE(result);
}

// Test 12: Write overwrites existing key
TEST_F(NvsStorageTest, Write_ExistingKey_Overwrites)
{
    std::string key = "update_key";
    storage->write(key, "original");
    storage->write(key, "updated");

    auto result = storage->read(key);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "updated");
}

// Test 13: Erase namespace removes all keys
TEST_F(NvsStorageTest, EraseNamespace_RemovesAllKeys)
{
    storage->write("key1", "value1");
    storage->write("key2", "value2");
    storage->write("key3", "value3");

    bool result = storage->eraseNamespace();

    EXPECT_TRUE(result);
    EXPECT_FALSE(storage->exists("key1"));
    EXPECT_FALSE(storage->exists("key2"));
    EXPECT_FALSE(storage->exists("key3"));
}

// Test 14: Commit returns true
TEST_F(NvsStorageTest, Commit_ReturnsTrue)
{
    storage->write("key", "value");

    bool result = storage->commit();

    EXPECT_TRUE(result);
}

// Test 15: Multiple key writes
TEST_F(NvsStorageTest, Write_MultipleKeys_AllSucceed)
{
    std::map<std::string, std::string> testData = {{"wifi_ssid", "TestNetwork"},
                                                   {"wifi_pass", "TestPass123"},
                                                   {"mqtt_broker", "test.mqtt.com"},
                                                   {"log_level", "INFO"}};

    for (const auto &[key, value] : testData)
    {
        EXPECT_TRUE(storage->write(key, value));
    }

    for (const auto &[key, value] : testData)
    {
        auto result = storage->read(key);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, value);
    }
}

// Test 16: Get storage type
TEST_F(NvsStorageTest, GetType_ReturnsNvs)
{
    EXPECT_EQ(storage->getType(), StorageType::NVS);
}

// Test 17: List keys (host mock only)
TEST_F(NvsStorageTest, ListKeys_OnHost_ReturnsKeys)
{
    storage->write("key1", "value1");
    storage->write("key2", "value2");

    auto keys = storage->listKeys();

    // On host (mock), should return keys
    // On target (ESP32), may return empty (not efficiently supported)
#ifndef ESP_PLATFORM
    EXPECT_GE(keys.size(), 2);
#endif
}

// Test 18: Special characters in value
TEST_F(NvsStorageTest, Write_SpecialCharactersInValue_Success)
{
    std::string key = "special_val";
    std::string data = "Value with 特殊字符 and symbols: @#$%";

    storage->write(key, data);
    auto result = storage->read(key);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, data);
}

// Test 19: Empty string value
TEST_F(NvsStorageTest, Write_EmptyStringValue_Success)
{
    std::string key = "empty_val";
    std::string data = "";

    storage->write(key, data);
    auto result = storage->read(key);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, data);
}

// Test 20: Max length key (15 characters)
TEST_F(NvsStorageTest, Write_MaxLengthKey_Success)
{
    std::string key = "fifteen_char_12"; // Exactly 15 chars
    std::string data = "test data";

    bool result = storage->write(key, data);

    EXPECT_TRUE(result);

    auto readData = storage->read(key);
    ASSERT_TRUE(readData.has_value());
    EXPECT_EQ(*readData, data);
}

// Main
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
