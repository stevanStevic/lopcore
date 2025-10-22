/**
 * @file logger.cpp
 * @brief Logger implementation
 *
 * Week 2 - Logging System
 */

#include "lopcore/logging/logger.hpp"

#include <cstdio>
#include <cstring>

#ifdef ESP_PLATFORM
#include "esp_timer.h"
#else
#include <chrono>
#endif

namespace lopcore
{

// Static buffer for formatted messages
static constexpr size_t MAX_LOG_MESSAGE_SIZE = 256;

Logger::Logger() : global_level_(LogLevel::INFO)
{
}

Logger::~Logger()
{
    flush();
}

Logger &Logger::getInstance()
{
    static Logger instance;
    return instance;
}

void Logger::addSink(std::unique_ptr<ILogSink> sink)
{
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::clearSinks()
{
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.clear();
}

void Logger::setGlobalLevel(LogLevel level)
{
    std::lock_guard<std::mutex> lock(mutex_);
    global_level_ = level;
}

void Logger::setTagLevel(const char *tag, LogLevel level)
{
    // TODO: Implement per-tag filtering
    // For now, just set global level
    (void) tag;
    setGlobalLevel(level);
}

void Logger::error(const char *tag, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    logImpl(LogLevel::ERROR, tag, format, args);
    va_end(args);
}

void Logger::warn(const char *tag, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    logImpl(LogLevel::WARN, tag, format, args);
    va_end(args);
}

void Logger::info(const char *tag, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    logImpl(LogLevel::INFO, tag, format, args);
    va_end(args);
}

void Logger::debug(const char *tag, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    logImpl(LogLevel::DEBUG, tag, format, args);
    va_end(args);
}

void Logger::verbose(const char *tag, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    logImpl(LogLevel::VERBOSE, tag, format, args);
    va_end(args);
}

void Logger::log(LogLevel level, const char *tag, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    logImpl(level, tag, format, args);
    va_end(args);
}

void Logger::flush()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &sink : sinks_)
    {
        sink->flush();
    }
}

void Logger::logImpl(LogLevel level, const char *tag, const char *format, va_list args)
{
    // Check if we should log this message
    if (!shouldLog(level, tag))
    {
        return;
    }

    // Format the message
    char buffer[MAX_LOG_MESSAGE_SIZE];
    vsnprintf(buffer, sizeof(buffer), format, args);

    // Create log message
    LogMessage msg;
    msg.level = level;
    msg.timestamp_ms = getTimestampMs();
    msg.tag = tag;
    msg.message = buffer;
    msg.file = nullptr;
    msg.line = 0;

    // Send to all sinks
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &sink : sinks_)
    {
        if (sink->isEnabled() && level <= sink->getMinLevel())
        {
            sink->write(msg);
        }
    }
}

bool Logger::shouldLog(LogLevel level, const char *tag) const
{
    (void) tag; // TODO: Implement per-tag filtering
    return level <= global_level_;
}

uint32_t Logger::getTimestampMs() const
{
#ifdef ESP_PLATFORM
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
#else
    // For host testing
    using namespace std::chrono;
    auto now = steady_clock::now().time_since_epoch();
    return static_cast<uint32_t>(duration_cast<milliseconds>(now).count());
#endif
}

} // namespace lopcore
