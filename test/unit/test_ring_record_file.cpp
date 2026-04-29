/**
 * @file test_ring_record_file.cpp
 * @brief Unit tests for RingRecordFile<T> template
 */

#include <sys/stat.h>

#include <cstdio>

#include <gtest/gtest.h>

#include "lopcore/storage/ring_record_file.hpp"

using namespace lopcore;

// 8-byte test record. Trivially copyable; no padding because of natural alignment.
struct TestRecord
{
    uint32_t a;
    uint16_t b;
    uint8_t  c;
    uint8_t  d;
};
static_assert(sizeof(TestRecord) == 8, "TestRecord must be 8 bytes");
static_assert(std::is_trivially_copyable_v<TestRecord>, "TestRecord must be trivially copyable");

class RingRecordFileTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_dir_ = "/tmp/lopcore_ring_record_test";
        mkdir(test_dir_.c_str(), 0755);
        test_file_ = test_dir_ + "/records.bin";
        remove(test_file_.c_str());
    }

    void TearDown() override
    {
        remove(test_file_.c_str());
        rmdir(test_dir_.c_str());
    }

    std::string test_dir_;
    std::string test_file_;
};

TEST_F(RingRecordFileTest, EmptyReadAllReturnsEmpty)
{
    RingRecordFile<TestRecord> ring(test_file_, 8);
    EXPECT_EQ(0u, ring.readAll().size());
}

TEST_F(RingRecordFileTest, FilePreallocatedAtConstruction)
{
    RingRecordFile<TestRecord> ring(test_file_, 8);
    struct stat st{};
    ASSERT_EQ(0, stat(test_file_.c_str(), &st));
    // 8 byte header + 8 slots * 8 bytes/slot = 72.
    EXPECT_EQ(72, st.st_size);
}

TEST_F(RingRecordFileTest, SingleAppendRoundTrip)
{
    RingRecordFile<TestRecord> ring(test_file_, 8);
    TestRecord r{42u, 7u, 1, 2};
    ring.append(r);

    auto entries = ring.readAll();
    ASSERT_EQ(1u, entries.size());
    EXPECT_EQ(42u, entries[0].a);
    EXPECT_EQ(7u, entries[0].b);
    EXPECT_EQ(1, entries[0].c);
    EXPECT_EQ(2, entries[0].d);
}

TEST_F(RingRecordFileTest, ChronologicalOrderUnderCapacity)
{
    RingRecordFile<TestRecord> ring(test_file_, 8);
    for (uint32_t i = 1; i <= 5; ++i)
    {
        ring.append(TestRecord{i * 100u, static_cast<uint16_t>(i), 0, 0});
    }
    auto entries = ring.readAll();
    ASSERT_EQ(5u, entries.size());
    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ((i + 1) * 100u, entries[i].a);
    }
}

TEST_F(RingRecordFileTest, WrapDropsOldest)
{
    RingRecordFile<TestRecord> ring(test_file_, 4);
    for (uint32_t i = 1; i <= 5; ++i)
    {
        ring.append(TestRecord{i, 0, 0, 0});
    }
    auto entries = ring.readAll();
    ASSERT_EQ(4u, entries.size());
    EXPECT_EQ(2u, entries[0].a);
    EXPECT_EQ(3u, entries[1].a);
    EXPECT_EQ(4u, entries[2].a);
    EXPECT_EQ(5u, entries[3].a);
}

TEST_F(RingRecordFileTest, ClearResetsCount)
{
    RingRecordFile<TestRecord> ring(test_file_, 8);
    ring.append(TestRecord{1, 0, 0, 0});
    ring.append(TestRecord{2, 0, 0, 0});
    ring.clear();
    EXPECT_EQ(0u, ring.readAll().size());
}

TEST_F(RingRecordFileTest, PersistenceAcrossInstances)
{
    {
        RingRecordFile<TestRecord> ring(test_file_, 8);
        ring.append(TestRecord{99, 0, 0, 0});
    }
    {
        RingRecordFile<TestRecord> ring(test_file_, 8);
        auto entries = ring.readAll();
        ASSERT_EQ(1u, entries.size());
        EXPECT_EQ(99u, entries[0].a);
    }
}

TEST_F(RingRecordFileTest, MaxEntriesAccessor)
{
    RingRecordFile<TestRecord> ring(test_file_, 16);
    EXPECT_EQ(16u, ring.maxEntries());
}
