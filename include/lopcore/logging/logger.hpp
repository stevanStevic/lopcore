/**
 * @file logger.hpp
 * @brief Main logging interface for LopCore
 *
 * Thread-safe logging system with multiple output sinks.
 * Provides modern C++ interface over ESP-IDF logging.
 *
 * Week 2 - Logging System
 */

#pragma once

#include <stdarg.h> // Use C header instead of cstdarg for C compatibility

#include <memory>
#include <mutex>
#include <vector>

#include "log_level.hpp"
#include "log_sink.hpp"

namespace lopcore
{

/**
 * @brief Thread-safe logging manager (Singleton)
 *
 * Features:
 * - Multiple output sinks (console, file, cloud, etc.)
 * - Per-tag log level filtering
 * - Thread-safe operation (FreeRTOS mutex)
 * - Printf-style formatting
 * - Minimal performance overhead
 *
 * @code
 * // Initialize logger
 * auto& logger = Logger::getInstance();
 * logger.addSink(std::make_unique<ConsoleSink>());
 *
 * // Log messages
 * logger.info("MyTag", "Value: %d", 42);
 * logger.error("Error", "Failed: %s", reason);
 * @endcode
 */
class Logger
{
public:
    /**
     * @brief Get singleton instance
     * @return Reference to global logger
     */
    static Logger &getInstance();

    // Prevent copying
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    /**
     * @brief Add a log output sink
     * @param sink Unique pointer to sink (takes ownership)
     */
    void addSink(std::unique_ptr<ILogSink> sink);

    /**
     * @brief Remove all sinks
     */
    void clearSinks();

    /**
     * @brief Set global minimum log level
     * @param level Minimum level to log
     */
    void setGlobalLevel(LogLevel level);

    /**
     * @brief Set log level for specific tag
     * @param tag Component/module tag
     * @param level Minimum level for this tag
     */
    void setTagLevel(const char *tag, LogLevel level);

    /**
     * @brief Get current global log level
     * @return Current global level
     */
    LogLevel getGlobalLevel() const
    {
        return global_level_;
    }

    /**
     * @brief Log an error message
     * @param tag Component tag
     * @param format Printf-style format string
     * @param ... Format arguments
     */
    void error(const char *tag, const char *format, ...);

    /**
     * @brief Log a warning message
     * @param tag Component tag
     * @param format Printf-style format string
     * @param ... Format arguments
     */
    void warn(const char *tag, const char *format, ...);

    /**
     * @brief Log an info message
     * @param tag Component tag
     * @param format Printf-style format string
     * @param ... Format arguments
     */
    void info(const char *tag, const char *format, ...);

    /**
     * @brief Log a debug message
     * @param tag Component tag
     * @param format Printf-style format string
     * @param ... Format arguments
     */
    void debug(const char *tag, const char *format, ...);

    /**
     * @brief Log a verbose message
     * @param tag Component tag
     * @param format Printf-style format string
     * @param ... Format arguments
     */
    void verbose(const char *tag, const char *format, ...);

    /**
     * @brief Log a message at specified level
     * @param level Log level
     * @param tag Component tag
     * @param format Printf-style format string
     * @param ... Format arguments
     */
    void log(LogLevel level, const char *tag, const char *format, ...);

    /**
     * @brief Flush all sinks
     */
    void flush();

    /**
     * @brief Get number of active sinks
     * @return Sink count
     */
    size_t getSinkCount() const
    {
        return sinks_.size();
    }

private:
    Logger();
    ~Logger();

    /**
     * @brief Internal log implementation with va_list
     */
    void logImpl(LogLevel level, const char *tag, const char *format, va_list args);

    /**
     * @brief Check if message should be logged based on level filtering
     */
    bool shouldLog(LogLevel level, const char *tag) const;

    /**
     * @brief Get current timestamp in milliseconds
     */
    uint32_t getTimestampMs() const;

    std::vector<std::unique_ptr<ILogSink>> sinks_; ///< Registered output sinks
    mutable std::mutex mutex_;                     ///< Thread safety
    LogLevel global_level_;                        ///< Global minimum level

    // Per-tag level filtering (future enhancement)
    // std::unordered_map<std::string, LogLevel> tag_levels_;
};

} // namespace lopcore

/**
 * @brief Convenience macros for logging
 *
 * Usage:
 *   LOPCORE_LOGE("TAG", "Error: %d", code);
 *   LOPCORE_LOGI("TAG", "Started");
 */
#define LOPCORE_LOGE(tag, format, ...) lopcore::Logger::getInstance().error(tag, format, ##__VA_ARGS__)

#define LOPCORE_LOGW(tag, format, ...) lopcore::Logger::getInstance().warn(tag, format, ##__VA_ARGS__)

#define LOPCORE_LOGI(tag, format, ...) lopcore::Logger::getInstance().info(tag, format, ##__VA_ARGS__)

#define LOPCORE_LOGD(tag, format, ...) lopcore::Logger::getInstance().debug(tag, format, ##__VA_ARGS__)

#define LOPCORE_LOGV(tag, format, ...) lopcore::Logger::getInstance().verbose(tag, format, ##__VA_ARGS__)
