/**
 * @file log_level.hpp
 * @brief Log level enumeration for LopCore logging system
 *
 * Week 2 - Logging System
 */

#pragma once

#include <cstdint>

namespace lopcore
{

/**
 * @brief Log severity levels
 *
 * Matches ESP-IDF log levels for compatibility while providing
 * type-safe enum interface.
 */
enum class LogLevel : uint8_t
{
    NONE = 0,   ///< No logging
    ERROR = 1,  ///< Critical errors that require immediate attention
    WARN = 2,   ///< Warning messages
    INFO = 3,   ///< Informational messages
    DEBUG = 4,  ///< Debug messages for development
    VERBOSE = 5 ///< Verbose/trace messages
};

/**
 * @brief Convert log level to string representation
 * @param level Log level to convert
 * @return String representation (e.g., "ERROR", "INFO")
 */
inline const char *logLevelToString(LogLevel level)
{
    switch (level)
    {
        case LogLevel::NONE:
            return "NONE";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::WARN:
            return "WARN";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::VERBOSE:
            return "VERBOSE";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Convert log level to single character
 * @param level Log level to convert
 * @return Single character (e.g., 'E', 'I', 'D')
 */
inline char logLevelToChar(LogLevel level)
{
    switch (level)
    {
        case LogLevel::NONE:
            return 'N';
        case LogLevel::ERROR:
            return 'E';
        case LogLevel::WARN:
            return 'W';
        case LogLevel::INFO:
            return 'I';
        case LogLevel::DEBUG:
            return 'D';
        case LogLevel::VERBOSE:
            return 'V';
        default:
            return '?';
    }
}

} // namespace lopcore
