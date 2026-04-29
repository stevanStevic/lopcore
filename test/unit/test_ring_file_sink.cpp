/**
 * @file test_ring_file_sink.cpp
 * @brief Unit tests for RingFileSink
 */

#include <sys/stat.h>

#include <cstdio>
#include <fstream>

#include <gtest/gtest.h>

#include "lopcore/logging/ring_file_sink.hpp"

using namespace lopcore;

class RingFileSinkTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_dir_ = "/tmp/lopcore_ring_test";
        mkdir(test_dir_.c_str(), 0755);
        test_file_ = test_dir_ + "/ring.log";
        remove(test_file_.c_str());
    }

    void TearDown() override
    {
        remove(test_file_.c_str());
        rmdir(test_dir_.c_str());
    }

    LogMessage makeMsg(const char *tag, const char *msg, uint32_t ts = 1000)
    {
        LogMessage m{};
        m.level = LogLevel::INFO;
        m.timestamp_ms = ts;
        m.tag = tag;
        m.message = msg;
        m.file = nullptr;
        m.line = 0;
        return m;
    }

    RingFileSinkConfig makeConfig(size_t maxBody)
    {
        RingFileSinkConfig c;
        c.setBasePath(test_dir_).setFilename("ring.log").setMaxBodyBytes(maxBody);
        return c;
    }

    std::string test_dir_;
    std::string test_file_;
};

TEST_F(RingFileSinkTest, EmptySinkReadAllReturnsEmpty)
{
    RingFileSink sink(makeConfig(1024));
    EXPECT_TRUE(sink.readAll().empty());
}

TEST_F(RingFileSinkTest, FileCreatedAtConstruction)
{
    RingFileSink sink(makeConfig(256));
    struct stat st{};
    ASSERT_EQ(0, stat(test_file_.c_str(), &st));
    // 8-byte header + 256-byte body = 264.
    EXPECT_EQ(264, st.st_size);
}

TEST_F(RingFileSinkTest, SingleWriteRecoverable)
{
    RingFileSink sink(makeConfig(1024));
    sink.write(makeMsg("APP", "hello world"));

    const std::string body = sink.readAll();
    EXPECT_NE(std::string::npos, body.find("hello world"));
    EXPECT_NE(std::string::npos, body.find("APP"));
}

TEST_F(RingFileSinkTest, WritesWrapAtMaxBytes)
{
    RingFileSink sink(makeConfig(256));
    for (int i = 0; i < 30; ++i)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "iter-%02d-message", i);
        sink.write(makeMsg("T", buf));
    }

    const std::string body = sink.readAll();
    EXPECT_LE(body.size(), 256u);
    // Newest must be present.
    EXPECT_NE(std::string::npos, body.find("iter-29-message"));
    // Oldest must have been overwritten.
    EXPECT_EQ(std::string::npos, body.find("iter-00-message"));
}

TEST_F(RingFileSinkTest, WritePosPersistsAcrossInstances)
{
    {
        RingFileSink sink(makeConfig(1024));
        sink.write(makeMsg("X", "first"));
        EXPECT_GT(sink.getWritePos(), 0u);
    }
    {
        RingFileSink sink(makeConfig(1024));
        sink.write(makeMsg("X", "second"));
        const std::string body = sink.readAll();
        EXPECT_NE(std::string::npos, body.find("first"));
        EXPECT_NE(std::string::npos, body.find("second"));
        // "first" must precede "second" since both fit before any wrap.
        EXPECT_LT(body.find("first"), body.find("second"));
    }
}

TEST_F(RingFileSinkTest, WritePosMonotonicAcrossWrap)
{
    RingFileSink sink(makeConfig(64));
    for (int i = 0; i < 10; ++i)
    {
        sink.write(makeMsg("T", "padding-string-here"));
    }
    // After many writes the file should have wrapped; writePos exceeds body size.
    EXPECT_GT(sink.getWritePos(), 64u);
}

TEST_F(RingFileSinkTest, GetNameReturnsRingFileSink)
{
    RingFileSink sink(makeConfig(64));
    EXPECT_STREQ("RingFileSink", sink.getName());
}
