/**
 * @file test_storage_factory.cpp
 * @brief Unit tests for StorageFactory
 */

#include <gtest/gtest.h>

#include "storage/nvs_storage.hpp"
#include "storage/spiffs_storage.hpp"
#include "storage/storage_factory.hpp"

using namespace lopcore;

// Test fixture for StorageFactory
class StorageFactoryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create temp directory for SPIFFS tests
        testPath_ = "/tmp/lopcore_factory_test";
        mkdir(testPath_.c_str(), 0755);
    }

    void TearDown() override
    {
        // Clean up
        system(("rm -rf " + testPath_).c_str());
    }

    std::string testPath_;
};

// Test 1: Create SPIFFS storage by type
TEST_F(StorageFactoryTest, Create_SpiffsType_ReturnsSpiffsStorage)
{
    auto storage = StorageFactory::create(StorageType::SPIFFS);

    ASSERT_NE(storage, nullptr);
    EXPECT_EQ(storage->getType(), StorageType::SPIFFS);
}

// Test 2: Create NVS storage by type
TEST_F(StorageFactoryTest, Create_NvsType_ReturnsNvsStorage)
{
    auto storage = StorageFactory::create(StorageType::NVS);

    ASSERT_NE(storage, nullptr);
    EXPECT_EQ(storage->getType(), StorageType::NVS);
}

// Test 3: Create SPIFFS with default path
TEST_F(StorageFactoryTest, CreateSpiffs_DefaultPath_Success)
{
    auto storage = StorageFactory::createSpiffs();

    ASSERT_NE(storage, nullptr);
    EXPECT_EQ(storage->getType(), StorageType::SPIFFS);

    // Check it's actually a SpiffsStorage
    auto *spiffsStorage = dynamic_cast<SpiffsStorage *>(storage.get());
    ASSERT_NE(spiffsStorage, nullptr);
    EXPECT_EQ(spiffsStorage->getBasePath(), "/spiffs");
}

// Test 4: Create SPIFFS with custom path
TEST_F(StorageFactoryTest, CreateSpiffs_CustomPath_Success)
{
    auto storage = StorageFactory::createSpiffs(testPath_);

    ASSERT_NE(storage, nullptr);

    auto *spiffsStorage = dynamic_cast<SpiffsStorage *>(storage.get());
    ASSERT_NE(spiffsStorage, nullptr);
    EXPECT_EQ(spiffsStorage->getBasePath(), testPath_);
}

// Test 5: Create NVS with default namespace
TEST_F(StorageFactoryTest, CreateNvs_DefaultNamespace_Success)
{
    auto storage = StorageFactory::createNvs();

    ASSERT_NE(storage, nullptr);
    EXPECT_EQ(storage->getType(), StorageType::NVS);

    auto *nvsStorage = dynamic_cast<NvsStorage *>(storage.get());
    ASSERT_NE(nvsStorage, nullptr);
    EXPECT_EQ(nvsStorage->getNamespace(), "lopcore");
}

// Test 6: Create NVS with custom namespace
TEST_F(StorageFactoryTest, CreateNvs_CustomNamespace_Success)
{
    auto storage = StorageFactory::createNvs("my_app");

    ASSERT_NE(storage, nullptr);

    auto *nvsStorage = dynamic_cast<NvsStorage *>(storage.get());
    ASSERT_NE(nvsStorage, nullptr);
    EXPECT_EQ(nvsStorage->getNamespace(), "my_app");
}

// Test 7: Create SD card storage returns nullptr (not implemented)
TEST_F(StorageFactoryTest, CreateSdCard_NotImplemented_ReturnsNull)
{
    auto storage = StorageFactory::createSdCard();

    EXPECT_EQ(storage, nullptr);
}

// Test 8: Create by SD_CARD type returns nullptr
TEST_F(StorageFactoryTest, Create_SdCardType_ReturnsNull)
{
    auto storage = StorageFactory::create(StorageType::SD_CARD);

    EXPECT_EQ(storage, nullptr);
}

// Test 9: Created SPIFFS storage is functional
TEST_F(StorageFactoryTest, CreateSpiffs_IsFunctional)
{
    auto storage = StorageFactory::createSpiffs(testPath_);

    ASSERT_NE(storage, nullptr);

    // Test write/read
    bool writeResult = storage->write("test.txt", "test data");
    EXPECT_TRUE(writeResult);

    auto readResult = storage->read("test.txt");
    ASSERT_TRUE(readResult.has_value());
    EXPECT_EQ(*readResult, "test data");
}

// Test 10: Created NVS storage is functional
TEST_F(StorageFactoryTest, CreateNvs_IsFunctional)
{
    auto storage = StorageFactory::createNvs("test_ns");

    ASSERT_NE(storage, nullptr);

    // Test write/read
    bool writeResult = storage->write("key1", "value1");
    EXPECT_TRUE(writeResult);

    auto readResult = storage->read("key1");
    ASSERT_TRUE(readResult.has_value());
    EXPECT_EQ(*readResult, "value1");

    // Cleanup
    auto *nvsStorage = dynamic_cast<NvsStorage *>(storage.get());
    if (nvsStorage)
    {
        nvsStorage->eraseNamespace();
    }
}

// Test 11: Multiple instances are independent
TEST_F(StorageFactoryTest, Create_MultipleInstances_AreIndependent)
{
    auto storage1 = StorageFactory::createNvs("namespace1");
    auto storage2 = StorageFactory::createNvs("namespace2");

    ASSERT_NE(storage1, nullptr);
    ASSERT_NE(storage2, nullptr);

    storage1->write("key", "value1");
    storage2->write("key", "value2");

    auto result1 = storage1->read("key");
    auto result2 = storage2->read("key");

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(*result1, "value1");
    EXPECT_EQ(*result2, "value2");

    // Cleanup
    auto *nvs1 = dynamic_cast<NvsStorage *>(storage1.get());
    auto *nvs2 = dynamic_cast<NvsStorage *>(storage2.get());
    if (nvs1)
        nvs1->eraseNamespace();
    if (nvs2)
        nvs2->eraseNamespace();
}

// Main
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
