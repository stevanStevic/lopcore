/**
 * @file test_file_sink.cpp
 * @brief Unit tests for FileSink
 *
 * Week 2 - Logging System
 */

#include <sys/stat.h>

#include <cstdio>
#include <fstream>

#include <gtest/gtest.h>

#include "logging/file_sink.hpp"
#include "logging/logger.hpp"

using namespace lopcore;

/**
 * @brief Test fixture for FileSink tests
 */
class FileSinkTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create test directory
        test_dir_ = "/tmp/lopcore_test";
        mkdir(test_dir_.c_str(), 0755);

        test_file_ = test_dir_ + "/test.log";

        // Clean up any existing test file
        remove(test_file_.c_str());
    }

    void TearDown() override
    {
        // Clean up test files
        remove(test_file_.c_str());
        rmdir(test_dir_.c_str());
    }

    std::string test_dir_;
    std::string test_file_;
};

/**
 * @brief Test FileSink configuration
 */
TEST_F(FileSinkTest, Configuration)
{
    FileSinkConfig config;
    config.base_path = test_dir_;
    config.filename = "test.log";
    config.max_file_size = 50 * 1024;
    config.buffer_size = 256;

    FileSink sink(config);

    EXPECT_EQ(sink.getMaxFileSize(), 50 * 1024u);
    EXPECT_EQ(sink.getFilePath(), test_file_);
}

/**
 * @brief Test file creation
 */
TEST_F(FileSinkTest, FileCreation)
{
    FileSinkConfig config;
    config.base_path = test_dir_;
    config.filename = "test.log";

    FileSink sink(config);

    // File should be created
    EXPECT_TRUE(sink.isFileOpen());

    struct stat st;
    EXPECT_EQ(stat(test_file_.c_str(), &st), 0);
}

/**
 * @brief Test writing log messages
 */
TEST_F(FileSinkTest, WriteMessages)
{
    FileSinkConfig config;
    config.base_path = test_dir_;
    config.filename = "test.log";
    config.buffer_size = 64; // Small buffer to force flush

    FileSink sink(config);

    LogMessage msg;
    msg.level = LogLevel::INFO;
    msg.timestamp_ms = 1000;
    msg.tag = "TEST";
    msg.message = "Test message";

    sink.write(msg);
    sink.flush();

    // Read back the file
    std::ifstream file(test_file_);
    std::string line;
    ASSERT_TRUE(std::getline(file, line));

    EXPECT_NE(line.find("TEST"), std::string::npos);
    EXPECT_NE(line.find("Test message"), std::string::npos);
}

/**
 * @brief Test automatic flush on buffer full
 */
TEST_F(FileSinkTest, AutoFlush)
{
    FileSinkConfig config;
    config.base_path = test_dir_;
    config.filename = "test.log";
    config.buffer_size = 100; // Small buffer

    FileSink sink(config);

    LogMessage msg;
    msg.level = LogLevel::INFO;
    msg.timestamp_ms = 1000;
    msg.tag = "TEST";
    msg.message = "This is a longer message that will trigger auto-flush";

    // Write multiple messages to exceed buffer
    for (int i = 0; i < 5; i++)
    {
        sink.write(msg);
    }

    // Should have auto-flushed
    size_t file_size = sink.getFileSize();
    EXPECT_GT(file_size, 0u);
}

/**
 * @brief Test log rotation
 */
TEST_F(FileSinkTest, Rotation)
{
    FileSinkConfig config;
    config.base_path = test_dir_;
    config.filename = "test.log";
    config.max_file_size = 200; // Small size for testing
    config.auto_rotate = true;

    FileSink sink(config);

    LogMessage msg;
    msg.level = LogLevel::INFO;
    msg.timestamp_ms = 1000;
    msg.tag = "TEST";
    msg.message = "Message that will cause rotation when repeated";

    // Write messages until rotation
    size_t initial_size = 0;
    for (int i = 0; i < 10; i++)
    {
        sink.write(msg);
        sink.flush();

        size_t current_size = sink.getFileSize();
        if (i > 0 && current_size < initial_size)
        {
            // Rotation occurred!
            SUCCEED();
            return;
        }
        initial_size = current_size;
    }

    // If we get here, check that file size is limited
    EXPECT_LE(sink.getFileSize(), config.max_file_size * 2); // Allow some overshoot
}

/**
 * @brief Test manual rotation
 */
TEST_F(FileSinkTest, ManualRotation)
{
    FileSinkConfig config;
    config.base_path = test_dir_;
    config.filename = "test.log";

    FileSink sink(config);

    LogMessage msg;
    msg.level = LogLevel::INFO;
    msg.timestamp_ms = 1000;
    msg.tag = "TEST";
    msg.message = "Message before rotation";

    sink.write(msg);
    sink.flush();

    size_t size_before = sink.getFileSize();
    EXPECT_GT(size_before, 0u);

    // Manually rotate
    EXPECT_TRUE(sink.rotate());

    // File should be smaller/empty
    size_t size_after = sink.getFileSize();
    EXPECT_LT(size_after, size_before);
}

/**
 * @brief Test message formatting
 */
TEST_F(FileSinkTest, MessageFormatting)
{
    FileSinkConfig config;
    config.base_path = test_dir_;
    config.filename = "test.log";

    FileSink sink(config);

    LogMessage msg;
    msg.level = LogLevel::ERROR;
    msg.timestamp_ms = 12345;
    msg.tag = "MYAPP";
    msg.message = "Error occurred";

    sink.write(msg);
    sink.flush();

    // Read and verify format
    std::ifstream file(test_file_);
    std::string line;
    ASSERT_TRUE(std::getline(file, line));

    // Format: [timestamp] LEVEL (tag): message
    EXPECT_NE(line.find("["), std::string::npos);
    EXPECT_NE(line.find("12345"), std::string::npos);
    EXPECT_NE(line.find("E"), std::string::npos);
    EXPECT_NE(line.find("MYAPP"), std::string::npos);
    EXPECT_NE(line.find("Error occurred"), std::string::npos);
}

/**
 * @brief Test persistence across instances
 */
TEST_F(FileSinkTest, Persistence)
{
    FileSinkConfig config;
    config.base_path = test_dir_;
    config.filename = "test.log";

    // First instance - write message
    {
        FileSink sink(config);
        LogMessage msg;
        msg.level = LogLevel::INFO;
        msg.timestamp_ms = 1000;
        msg.tag = "TEST";
        msg.message = "Persistent message";

        sink.write(msg);
        sink.flush();
    }

    // Second instance - append message
    {
        FileSink sink(config);
        LogMessage msg;
        msg.level = LogLevel::INFO;
        msg.timestamp_ms = 2000;
        msg.tag = "TEST";
        msg.message = "Another message";

        sink.write(msg);
        sink.flush();
    }

    // Verify both messages exist
    std::ifstream file(test_file_);
    std::string line1, line2;
    ASSERT_TRUE(std::getline(file, line1));
    ASSERT_TRUE(std::getline(file, line2));

    EXPECT_NE(line1.find("Persistent message"), std::string::npos);
    EXPECT_NE(line2.find("Another message"), std::string::npos);
}

/**
 * @brief Test with Logger integration
 */
TEST_F(FileSinkTest, LoggerIntegration)
{
    auto &logger = Logger::getInstance();
    logger.clearSinks();

    FileSinkConfig config;
    config.base_path = test_dir_;
    config.filename = "test.log";

    logger.addSink(std::make_unique<FileSink>(config));

    logger.info("APP", "Integrated log message");
    logger.error("APP", "Error message");
    logger.flush();

    // Verify messages in file
    std::ifstream file(test_file_);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    EXPECT_NE(content.find("Integrated log message"), std::string::npos);
    EXPECT_NE(content.find("Error message"), std::string::npos);

    logger.clearSinks();
}

/**
 * @brief Test getSinkName
 */
TEST_F(FileSinkTest, GetName)
{
    FileSinkConfig config;
    config.base_path = test_dir_;
    config.filename = "test.log";

    FileSink sink(config);

    EXPECT_STREQ(sink.getName(), "FileSink");
}

/**
 * @brief Test destructor flushes
 */
TEST_F(FileSinkTest, DestructorFlush)
{
    FileSinkConfig config;
    config.base_path = test_dir_;
    config.filename = "test.log";

    {
        FileSink sink(config);
        LogMessage msg;
        msg.level = LogLevel::INFO;
        msg.timestamp_ms = 1000;
        msg.tag = "TEST";
        msg.message = "Message without explicit flush";

        sink.write(msg);
        // Don't call flush - rely on destructor
    }

    // Verify message was written
    std::ifstream file(test_file_);
    std::string line;
    ASSERT_TRUE(std::getline(file, line));
    EXPECT_NE(line.find("Message without explicit flush"), std::string::npos);
}
