/**
 * @file test_logger.cpp
 * @brief Unit tests for LopCore Logger
 *
 * Week 2 - Logging System
 */

#include <memory>
#include <sstream>

#include <gtest/gtest.h>

#include "logging/console_sink.hpp"
#include "logging/logger.hpp"

using namespace lopcore;

/**
 * @brief Test fixture for Logger tests
 */
class LoggerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Clear any existing sinks
        Logger::getInstance().clearSinks();
        Logger::getInstance().setGlobalLevel(LogLevel::VERBOSE);
    }

    void TearDown() override
    {
        Logger::getInstance().clearSinks();
    }
};

/**
 * @brief Test logger singleton
 */
TEST_F(LoggerTest, SingletonInstance)
{
    Logger &logger1 = Logger::getInstance();
    Logger &logger2 = Logger::getInstance();

    // Should be the same instance
    EXPECT_EQ(&logger1, &logger2);
}

/**
 * @brief Test adding sinks
 */
TEST_F(LoggerTest, AddSink)
{
    auto &logger = Logger::getInstance();

    EXPECT_EQ(logger.getSinkCount(), 0u);

    logger.addSink(std::make_unique<ConsoleSink>());
    EXPECT_EQ(logger.getSinkCount(), 1u);

    logger.addSink(std::make_unique<ConsoleSink>());
    EXPECT_EQ(logger.getSinkCount(), 2u);
}

/**
 * @brief Test clearing sinks
 */
TEST_F(LoggerTest, ClearSinks)
{
    auto &logger = Logger::getInstance();

    logger.addSink(std::make_unique<ConsoleSink>());
    logger.addSink(std::make_unique<ConsoleSink>());
    EXPECT_EQ(logger.getSinkCount(), 2u);

    logger.clearSinks();
    EXPECT_EQ(logger.getSinkCount(), 0u);
}

/**
 * @brief Test global log level
 */
TEST_F(LoggerTest, GlobalLogLevel)
{
    auto &logger = Logger::getInstance();

    logger.setGlobalLevel(LogLevel::INFO);
    EXPECT_EQ(logger.getGlobalLevel(), LogLevel::INFO);

    logger.setGlobalLevel(LogLevel::ERROR);
    EXPECT_EQ(logger.getGlobalLevel(), LogLevel::ERROR);

    logger.setGlobalLevel(LogLevel::VERBOSE);
    EXPECT_EQ(logger.getGlobalLevel(), LogLevel::VERBOSE);
}

/**
 * @brief Test log level to string conversion
 */
TEST_F(LoggerTest, LogLevelToString)
{
    EXPECT_STREQ(logLevelToString(LogLevel::NONE), "NONE");
    EXPECT_STREQ(logLevelToString(LogLevel::ERROR), "ERROR");
    EXPECT_STREQ(logLevelToString(LogLevel::WARN), "WARN");
    EXPECT_STREQ(logLevelToString(LogLevel::INFO), "INFO");
    EXPECT_STREQ(logLevelToString(LogLevel::DEBUG), "DEBUG");
    EXPECT_STREQ(logLevelToString(LogLevel::VERBOSE), "VERBOSE");
}

/**
 * @brief Test log level to char conversion
 */
TEST_F(LoggerTest, LogLevelToChar)
{
    EXPECT_EQ(logLevelToChar(LogLevel::NONE), 'N');
    EXPECT_EQ(logLevelToChar(LogLevel::ERROR), 'E');
    EXPECT_EQ(logLevelToChar(LogLevel::WARN), 'W');
    EXPECT_EQ(logLevelToChar(LogLevel::INFO), 'I');
    EXPECT_EQ(logLevelToChar(LogLevel::DEBUG), 'D');
    EXPECT_EQ(logLevelToChar(LogLevel::VERBOSE), 'V');
}

/**
 * @brief Custom test sink that captures log messages
 */
class TestSink : public ILogSink
{
public:
    struct CapturedMessage
    {
        LogLevel level;
        std::string tag;
        std::string message;
    };

    void write(const LogMessage &msg) override
    {
        CapturedMessage captured;
        captured.level = msg.level;
        captured.tag = msg.tag;
        captured.message = msg.message;
        messages_.push_back(captured);
    }

    void flush() override
    {
        flush_called_ = true;
    }

    const char *getName() const override
    {
        return "TestSink";
    }

    const std::vector<CapturedMessage> &getMessages() const
    {
        return messages_;
    }

    bool wasFlushCalled() const
    {
        return flush_called_;
    }

    void clear()
    {
        messages_.clear();
        flush_called_ = false;
    }

private:
    std::vector<CapturedMessage> messages_;
    bool flush_called_ = false;
};

/**
 * @brief Test logging messages
 */
TEST_F(LoggerTest, LogMessages)
{
    auto &logger = Logger::getInstance();
    auto test_sink = std::make_unique<TestSink>();
    auto *sink_ptr = test_sink.get();

    logger.addSink(std::move(test_sink));

    // Log at different levels
    logger.error("TEST", "Error message");
    logger.warn("TEST", "Warning message");
    logger.info("TEST", "Info message");
    logger.debug("TEST", "Debug message");
    logger.verbose("TEST", "Verbose message");

    const auto &messages = sink_ptr->getMessages();
    EXPECT_EQ(messages.size(), 5u);

    EXPECT_EQ(messages[0].level, LogLevel::ERROR);
    EXPECT_EQ(messages[0].tag, "TEST");
    EXPECT_EQ(messages[0].message, "Error message");

    EXPECT_EQ(messages[4].level, LogLevel::VERBOSE);
    EXPECT_EQ(messages[4].tag, "TEST");
    EXPECT_EQ(messages[4].message, "Verbose message");
}

/**
 * @brief Test formatted logging
 */
TEST_F(LoggerTest, FormattedLogging)
{
    auto &logger = Logger::getInstance();
    auto test_sink = std::make_unique<TestSink>();
    auto *sink_ptr = test_sink.get();

    logger.addSink(std::move(test_sink));

    int value = 42;
    const char *str = "hello";

    logger.info("TEST", "Number: %d, String: %s", value, str);

    const auto &messages = sink_ptr->getMessages();
    EXPECT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages[0].message, "Number: 42, String: hello");
}

/**
 * @brief Test log level filtering
 */
TEST_F(LoggerTest, LogLevelFiltering)
{
    auto &logger = Logger::getInstance();
    auto test_sink = std::make_unique<TestSink>();
    auto *sink_ptr = test_sink.get();

    logger.addSink(std::move(test_sink));
    logger.setGlobalLevel(LogLevel::WARN);

    logger.error("TEST", "Error");  // Should log
    logger.warn("TEST", "Warning"); // Should log
    logger.info("TEST", "Info");    // Should NOT log
    logger.debug("TEST", "Debug");  // Should NOT log

    const auto &messages = sink_ptr->getMessages();
    EXPECT_EQ(messages.size(), 2u);
    EXPECT_EQ(messages[0].level, LogLevel::ERROR);
    EXPECT_EQ(messages[1].level, LogLevel::WARN);
}

/**
 * @brief Test flush functionality
 */
TEST_F(LoggerTest, Flush)
{
    auto &logger = Logger::getInstance();
    auto test_sink = std::make_unique<TestSink>();
    auto *sink_ptr = test_sink.get();

    logger.addSink(std::move(test_sink));

    EXPECT_FALSE(sink_ptr->wasFlushCalled());

    logger.flush();

    EXPECT_TRUE(sink_ptr->wasFlushCalled());
}

/**
 * @brief Test multiple sinks
 */
TEST_F(LoggerTest, MultipleSinks)
{
    auto &logger = Logger::getInstance();

    auto test_sink1 = std::make_unique<TestSink>();
    auto test_sink2 = std::make_unique<TestSink>();
    auto *sink1_ptr = test_sink1.get();
    auto *sink2_ptr = test_sink2.get();

    logger.addSink(std::move(test_sink1));
    logger.addSink(std::move(test_sink2));

    logger.info("TEST", "Message to both sinks");

    // Both sinks should receive the message
    EXPECT_EQ(sink1_ptr->getMessages().size(), 1u);
    EXPECT_EQ(sink2_ptr->getMessages().size(), 1u);

    EXPECT_EQ(sink1_ptr->getMessages()[0].message, "Message to both sinks");
    EXPECT_EQ(sink2_ptr->getMessages()[0].message, "Message to both sinks");
}

/**
 * @brief Test console sink
 */
TEST_F(LoggerTest, ConsoleSink)
{
    auto console_sink = std::make_unique<ConsoleSink>();

    EXPECT_STREQ(console_sink->getName(), "ConsoleSink");
    EXPECT_TRUE(console_sink->isEnabled());

    // Test that it doesn't crash when writing
    LogMessage msg;
    msg.level = LogLevel::INFO;
    msg.timestamp_ms = 1000;
    msg.tag = "TEST";
    msg.message = "Test message";
    msg.file = nullptr;
    msg.line = 0;

    EXPECT_NO_THROW(console_sink->write(msg));
    EXPECT_NO_THROW(console_sink->flush());
}

/**
 * @brief Test convenience macros
 */
TEST_F(LoggerTest, ConvenienceMacros)
{
    auto &logger = Logger::getInstance();
    auto test_sink = std::make_unique<TestSink>();
    auto *sink_ptr = test_sink.get();

    logger.addSink(std::move(test_sink));

    LOPCORE_LOGE("MACRO", "Error via macro");
    LOPCORE_LOGW("MACRO", "Warning via macro");
    LOPCORE_LOGI("MACRO", "Info via macro");
    LOPCORE_LOGD("MACRO", "Debug via macro");
    LOPCORE_LOGV("MACRO", "Verbose via macro");

    const auto &messages = sink_ptr->getMessages();
    EXPECT_EQ(messages.size(), 5u);

    EXPECT_EQ(messages[0].tag, "MACRO");
    EXPECT_EQ(messages[0].message, "Error via macro");
}
