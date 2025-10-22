/**
 * @file log_sink.hpp
 * @brief Abstract interface for log output destinations
 *
 * Week 2 - Logging System
 */

#pragma once

#include <memory>
#include <string>

#include "log_level.hpp"

namespace lopcore
{

/**
 * @brief Log message structure
 *
 * Contains all information about a single log entry.
 */
struct LogMessage
{
    LogLevel level;        ///< Severity level
    uint32_t timestamp_ms; ///< Milliseconds since boot
    const char *tag;       ///< Component/module tag
    const char *message;   ///< Formatted message
    const char *file;      ///< Source file (optional)
    int line;              ///< Source line (optional)
};

/**
 * @brief Abstract base class for log output destinations
 *
 * Implement this interface to create custom log sinks (console, file, network, etc.)
 * Uses the Strategy pattern for flexible log routing.
 */
class ILogSink
{
public:
    virtual ~ILogSink() = default;

    /**
     * @brief Write a log message to this sink
     * @param msg Log message to write
     */
    virtual void write(const LogMessage &msg) = 0;

    /**
     * @brief Flush any buffered log data
     */
    virtual void flush() = 0;

    /**
     * @brief Get the name of this sink (for debugging)
     * @return Sink name
     */
    virtual const char *getName() const = 0;

    /**
     * @brief Check if this sink is enabled
     * @return true if sink is active
     */
    virtual bool isEnabled() const
    {
        return true;
    }

    /**
     * @brief Set minimum log level for this sink
     * @param level Minimum level to log
     */
    virtual void setMinLevel(LogLevel level)
    {
        min_level_ = level;
    }

    /**
     * @brief Get minimum log level
     * @return Current minimum level
     */
    virtual LogLevel getMinLevel() const
    {
        return min_level_;
    }

protected:
    LogLevel min_level_ = LogLevel::VERBOSE; ///< Minimum level to output
};

} // namespace lopcore
