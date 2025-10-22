/**
 * @file test_spiffs_storage.cpp
 * @brief Unit tests for SpiffsStorage
 */

#include <sys/stat.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include <gtest/gtest.h>

#include "storage/spiffs_storage.hpp"

using namespace lopcore;

// Test fixture for SpiffsStorage
class SpiffsStorageTest : public ::testing::Test
{
protected:
    std::unique_ptr<SpiffsStorage> storage;
    std::string testPath;

    void SetUp() override
    {
        // Create temporary directory for tests
        testPath = "/tmp/lopcore_spiffs_test";
        mkdir(testPath.c_str(), 0755);
        storage = std::make_unique<SpiffsStorage>(testPath);
    }

    void TearDown() override
    {
        storage.reset();
        // Clean up test files
        system(("rm -rf " + testPath).c_str());
    }
};

// Test 1: Constructor initializes successfully
TEST_F(SpiffsStorageTest, Constructor_InitializesSuccessfully)
{
    ASSERT_NE(storage, nullptr);
    EXPECT_EQ(storage->getType(), StorageType::SPIFFS);
    EXPECT_EQ(storage->getBasePath(), testPath);
}

// Test 2: Write string data returns true
TEST_F(SpiffsStorageTest, Write_ValidStringData_ReturnsTrue)
{
    std::string key = "test.txt";
    std::string data = "Hello, SPIFFS!";

    bool result = storage->write(key, data);

    EXPECT_TRUE(result);
    EXPECT_TRUE(storage->exists(key));
}

// Test 3: Read existing key returns data
TEST_F(SpiffsStorageTest, Read_ExistingKey_ReturnsData)
{
    std::string key = "config.json";
    std::string expectedData = R"({"version": "1.0"})";

    storage->write(key, expectedData);
    auto result = storage->read(key);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, expectedData);
}

// Test 4: Read non-existent key returns nullopt
TEST_F(SpiffsStorageTest, Read_NonExistentKey_ReturnsNullopt)
{
    auto result = storage->read("nonexistent.txt");

    EXPECT_FALSE(result.has_value());
}

// Test 5: Exists returns true for existing key
TEST_F(SpiffsStorageTest, Exists_ExistingKey_ReturnsTrue)
{
    std::string key = "exists_test.txt";
    storage->write(key, "test data");

    bool result = storage->exists(key);

    EXPECT_TRUE(result);
}

// Test 6: Exists returns false for non-existent key
TEST_F(SpiffsStorageTest, Exists_NonExistentKey_ReturnsFalse)
{
    bool result = storage->exists("does_not_exist.txt");

    EXPECT_FALSE(result);
}

// Test 7: Remove existing key returns true
TEST_F(SpiffsStorageTest, Remove_ExistingKey_ReturnsTrue)
{
    std::string key = "to_remove.txt";
    storage->write(key, "data");

    bool result = storage->remove(key);

    EXPECT_TRUE(result);
    EXPECT_FALSE(storage->exists(key));
}

// Test 8: Remove non-existent key returns true (idempotent)
TEST_F(SpiffsStorageTest, Remove_NonExistentKey_ReturnsTrue)
{
    bool result = storage->remove("never_existed.txt");

    // Should still return true (idempotent operation)
    EXPECT_TRUE(result);
}

// Test 9: List keys returns all files
TEST_F(SpiffsStorageTest, ListKeys_MultipleFiles_ReturnsAllKeys)
{
    storage->write("file1.txt", "data1");
    storage->write("file2.txt", "data2");
    storage->write("file3.txt", "data3");

    auto keys = storage->listKeys();

    EXPECT_EQ(keys.size(), 3);
    EXPECT_NE(std::find(keys.begin(), keys.end(), "file1.txt"), keys.end());
    EXPECT_NE(std::find(keys.begin(), keys.end(), "file2.txt"), keys.end());
    EXPECT_NE(std::find(keys.begin(), keys.end(), "file3.txt"), keys.end());
}

// Test 10: Write binary data
TEST_F(SpiffsStorageTest, WriteBinary_ValidData_ReturnsTrue)
{
    std::string key = "binary.bin";
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0xFF, 0xFE};

    bool result = storage->write(key, data);

    EXPECT_TRUE(result);
    EXPECT_TRUE(storage->exists(key));
}

// Test 11: Read binary data
TEST_F(SpiffsStorageTest, ReadBinary_ExistingKey_ReturnsData)
{
    std::string key = "binary_test.bin";
    std::vector<uint8_t> expectedData = {0xDE, 0xAD, 0xBE, 0xEF};

    storage->write(key, expectedData);
    auto result = storage->readBinary(key);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, expectedData);
}

// Test 12: Write large file
TEST_F(SpiffsStorageTest, Write_LargeFile_Success)
{
    std::string key = "large_file.dat";
    std::string largeData(10000, 'X'); // 10KB of 'X'

    bool result = storage->write(key, largeData);

    EXPECT_TRUE(result);

    auto readData = storage->read(key);
    ASSERT_TRUE(readData.has_value());
    EXPECT_EQ(readData->size(), largeData.size());
}

// Test 13: Write overwrites existing file
TEST_F(SpiffsStorageTest, Write_ExistingKey_Overwrites)
{
    std::string key = "overwrite.txt";
    storage->write(key, "original");
    storage->write(key, "updated");

    auto result = storage->read(key);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "updated");
}

// Test 14: Empty string write and read
TEST_F(SpiffsStorageTest, Write_EmptyString_Success)
{
    std::string key = "empty.txt";
    std::string emptyData = "";

    storage->write(key, emptyData);
    auto result = storage->read(key);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, emptyData);
}

// Test 15: Key without subdirectory (SPIFFS doesn't have real directories)
TEST_F(SpiffsStorageTest, Write_KeyWithoutSubdir_Success)
{
    std::string key = "flatfile.txt";
    std::string data = "flat file structure";

    bool result = storage->write(key, data);

    EXPECT_TRUE(result);

    auto readData = storage->read(key);
    ASSERT_TRUE(readData.has_value());
    EXPECT_EQ(*readData, data);
}

// Test 16: Multiple simultaneous operations (thread safety check)
TEST_F(SpiffsStorageTest, Concurrent_MultipleWrites_AllSucceed)
{
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < 5; i++)
    {
        threads.emplace_back([this, i, &successCount]() {
            std::string key = "thread_" + std::to_string(i) + ".txt";
            std::string data = "data from thread " + std::to_string(i);

            if (storage->write(key, data))
            {
                successCount++;
            }
        });
    }

    for (auto &thread : threads)
    {
        thread.join();
    }

    EXPECT_EQ(successCount, 5);
}

// Test 17: Get storage type
TEST_F(SpiffsStorageTest, GetType_ReturnsSpiffs)
{
    EXPECT_EQ(storage->getType(), StorageType::SPIFFS);
}

// Test 18: Read special characters
TEST_F(SpiffsStorageTest, Write_SpecialCharacters_Success)
{
    std::string key = "special.txt";
    std::string data = "Test with special chars: é ñ ü 中文 🎉";

    storage->write(key, data);
    auto result = storage->read(key);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, data);
}

// Main
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
