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
        // Initialize the storage
        ASSERT_TRUE(storage->initialize()) << "Failed to initialize SPIFFS storage";
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

// Test 19: Write binary data using C array pointer
TEST_F(SpiffsStorageTest, Write_CArrayPointer_Success)
{
    std::string key = "c_array.bin";
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    size_t dataLen = sizeof(data);

    bool result = storage->write(key, data, dataLen);

    EXPECT_TRUE(result);
    EXPECT_TRUE(storage->exists(key));

    // Verify data
    auto readData = storage->readBinary(key);
    ASSERT_TRUE(readData.has_value());
    EXPECT_EQ(readData->size(), dataLen);
    EXPECT_EQ(std::memcmp(readData->data(), data, dataLen), 0);
}

// Test 20: Write with null pointer returns false
TEST_F(SpiffsStorageTest, Write_NullPointer_ReturnsFalse)
{
    std::string key = "null_test.bin";

    bool result = storage->write(key, nullptr, 100);

    EXPECT_FALSE(result);
}

// Test 21: Write with zero length returns false
TEST_F(SpiffsStorageTest, Write_ZeroLength_ReturnsFalse)
{
    std::string key = "zero_test.bin";
    uint8_t data[] = {0x01, 0x02};

    bool result = storage->write(key, data, 0);

    EXPECT_FALSE(result);
}

// Test 22: hasSpace returns true when space available
TEST_F(SpiffsStorageTest, HasSpace_SmallAmount_ReturnsTrue)
{
    bool result = storage->hasSpace(1024); // 1KB

    EXPECT_TRUE(result);
}

// Test 23: getFileSize returns correct size
TEST_F(SpiffsStorageTest, GetFileSize_ExistingFile_ReturnsCorrectSize)
{
    std::string key = "sized_file.txt";
    std::string data = "12345678"; // 8 bytes

    storage->write(key, data);
    auto size = storage->getFileSize(key);

    ASSERT_TRUE(size.has_value());
    EXPECT_EQ(size.value(), 8);
}

// Test 24: getFileSize for non-existent file returns nullopt
TEST_F(SpiffsStorageTest, GetFileSize_NonExistentFile_ReturnsNullopt)
{
    auto size = storage->getFileSize("does_not_exist.txt");

    EXPECT_FALSE(size.has_value());
}

// Test 25: listKeysByPattern matches single file
TEST_F(SpiffsStorageTest, ListKeysByPattern_SingleMatch_ReturnsOne)
{
    storage->write("acc_raw_1.bin", "data1");
    storage->write("acc_raw_2.bin", "data2");
    storage->write("other.txt", "data3");

    auto files = storage->listKeysByPattern("acc_raw_1.bin");

    EXPECT_EQ(files.size(), 1);
    EXPECT_EQ(files[0], "acc_raw_1.bin");
}

// Test 26: listKeysByPattern with wildcard matches multiple files
TEST_F(SpiffsStorageTest, ListKeysByPattern_Wildcard_ReturnsMultiple)
{
    storage->write("acc_raw_1.bin", "data1");
    storage->write("acc_raw_2.bin", "data2");
    storage->write("acc_raw_123.bin", "data3");
    storage->write("other.txt", "data4");

    auto files = storage->listKeysByPattern("acc_raw_*.bin");

    EXPECT_EQ(files.size(), 3);
    EXPECT_NE(std::find(files.begin(), files.end(), "acc_raw_1.bin"), files.end());
    EXPECT_NE(std::find(files.begin(), files.end(), "acc_raw_2.bin"), files.end());
    EXPECT_NE(std::find(files.begin(), files.end(), "acc_raw_123.bin"), files.end());
}

// Test 27: listKeysByPattern with no matches returns empty
TEST_F(SpiffsStorageTest, ListKeysByPattern_NoMatches_ReturnsEmpty)
{
    storage->write("file1.txt", "data1");
    storage->write("file2.txt", "data2");

    auto files = storage->listKeysByPattern("acc_raw_*.bin");

    EXPECT_TRUE(files.empty());
}

// Test 28: listKeysByPattern prefix wildcard
TEST_F(SpiffsStorageTest, ListKeysByPattern_PrefixWildcard_Matches)
{
    storage->write("data_1.log", "log1");
    storage->write("data_2.log", "log2");
    storage->write("data_3.txt", "txt1");

    auto files = storage->listKeysByPattern("data_*.log");

    EXPECT_EQ(files.size(), 2);
}

// Test 29: removeByPattern deletes matching files
TEST_F(SpiffsStorageTest, RemoveByPattern_MultipleMatches_DeletesAll)
{
    storage->write("temp_1.tmp", "data1");
    storage->write("temp_2.tmp", "data2");
    storage->write("temp_3.tmp", "data3");
    storage->write("keep.txt", "keep this");

    size_t deletedCount = storage->removeByPattern("temp_*.tmp");

    EXPECT_EQ(deletedCount, 3);
    EXPECT_FALSE(storage->exists("temp_1.tmp"));
    EXPECT_FALSE(storage->exists("temp_2.tmp"));
    EXPECT_FALSE(storage->exists("temp_3.tmp"));
    EXPECT_TRUE(storage->exists("keep.txt"));
}

// Test 30: removeByPattern with no matches returns 0
TEST_F(SpiffsStorageTest, RemoveByPattern_NoMatches_ReturnsZero)
{
    storage->write("file.txt", "data");

    size_t deletedCount = storage->removeByPattern("nonexistent_*.bin");

    EXPECT_EQ(deletedCount, 0);
}

// Test 31: removeByPattern exact match
TEST_F(SpiffsStorageTest, RemoveByPattern_ExactMatch_DeletesOne)
{
    storage->write("specific.txt", "data1");
    storage->write("other.txt", "data2");

    size_t deletedCount = storage->removeByPattern("specific.txt");

    EXPECT_EQ(deletedCount, 1);
    EXPECT_FALSE(storage->exists("specific.txt"));
    EXPECT_TRUE(storage->exists("other.txt"));
}

// Test 32: listDetailed returns file information
TEST_F(SpiffsStorageTest, ListDetailed_MultipleFiles_ReturnsDetails)
{
    storage->write("small.txt", "12345");      // 5 bytes
    storage->write("large.txt", "1234567890"); // 10 bytes

    auto files = storage->listDetailed();

    EXPECT_EQ(files.size(), 2);

    // Find small.txt
    auto smallIt = std::find_if(files.begin(), files.end(),
                                [](const auto &info) { return info.name == "small.txt"; });
    ASSERT_NE(smallIt, files.end());
    EXPECT_EQ(smallIt->size, 5);
    EXPECT_FALSE(smallIt->isDirectory);

    // Find large.txt
    auto largeIt = std::find_if(files.begin(), files.end(),
                                [](const auto &info) { return info.name == "large.txt"; });
    ASSERT_NE(largeIt, files.end());
    EXPECT_EQ(largeIt->size, 10);
    EXPECT_FALSE(largeIt->isDirectory);
}

// Test 33: displayStats doesn't crash
TEST_F(SpiffsStorageTest, DisplayStats_NoFiles_DoesNotCrash)
{
    // Just verify it doesn't crash
    EXPECT_NO_THROW(storage->displayStats());
}

// Test 34: Pattern matching edge cases
TEST_F(SpiffsStorageTest, ListKeysByPattern_MultipleWildcards_MatchesCorrectly)
{
    storage->write("log_2024_01.txt", "data");
    storage->write("log_2024_02.txt", "data");
    storage->write("log_2023_01.txt", "data");

    // Only one wildcard supported, but test prefix/suffix matching
    auto files = storage->listKeysByPattern("log_2024_*.txt");

    EXPECT_EQ(files.size(), 2);
}

// Test 35: Large file pattern deletion
TEST_F(SpiffsStorageTest, RemoveByPattern_LargeNumberOfFiles_DeletesAll)
{
    // Create 20 files
    for (int i = 0; i < 20; i++)
    {
        std::string key = "test_" + std::to_string(i) + ".dat";
        storage->write(key, "data");
    }

    size_t deletedCount = storage->removeByPattern("test_*.dat");

    EXPECT_EQ(deletedCount, 20);

    // Verify all deleted
    for (int i = 0; i < 20; i++)
    {
        std::string key = "test_" + std::to_string(i) + ".dat";
        EXPECT_FALSE(storage->exists(key));
    }
}

// Test 36: C array write with large data
TEST_F(SpiffsStorageTest, Write_LargeCArray_Success)
{
    std::string key = "large_c_array.bin";
    std::vector<uint8_t> largeData(5000);
    for (size_t i = 0; i < largeData.size(); i++)
    {
        largeData[i] = static_cast<uint8_t>(i % 256);
    }

    bool result = storage->write(key, largeData.data(), largeData.size());

    EXPECT_TRUE(result);

    auto readData = storage->readBinary(key);
    ASSERT_TRUE(readData.has_value());
    EXPECT_EQ(*readData, largeData);
}

// Main
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
