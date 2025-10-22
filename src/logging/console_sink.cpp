/**
 * @file console_sink.cpp
 * @brief Console logging sink implementation
 *
 * Week 2 - Logging System
 */

#include "lopcore/logging/console_sink.hpp"

#include <cstdio>

#ifdef ESP_PLATFORM
#include "esp_log.h"
#endif

namespace lopcore
{

ConsoleSink::ConsoleSink() : use_colors_(true)
{
}

void ConsoleSink::write(const LogMessage &msg)
{
#ifdef ESP_PLATFORM
    // Use ESP-IDF logging macros for properly formatted and colored output
    switch (msg.level)
    {
        case LogLevel::ERROR:
            ESP_LOGE(msg.tag, "%s", msg.message);
            break;
        case LogLevel::WARN:
            ESP_LOGW(msg.tag, "%s", msg.message);
            break;
        case LogLevel::INFO:
            ESP_LOGI(msg.tag, "%s", msg.message);
            break;
        case LogLevel::DEBUG:
            ESP_LOGD(msg.tag, "%s", msg.message);
            break;
        case LogLevel::VERBOSE:
            ESP_LOGV(msg.tag, "%s", msg.message);
            break;
        default:
            ESP_LOGI(msg.tag, "%s", msg.message);
            break;
    }
#else
    // For host testing, use printf
    const char *level_color = "";
    const char *reset_color = "";

    if (use_colors_)
    {
        switch (msg.level)
        {
            case LogLevel::ERROR:
                level_color = "\033[0;31m";
                break; // Red
            case LogLevel::WARN:
                level_color = "\033[0;33m";
                break; // Yellow
            case LogLevel::INFO:
                level_color = "\033[0;32m";
                break; // Green
            case LogLevel::DEBUG:
                level_color = "\033[0;36m";
                break; // Cyan
            case LogLevel::VERBOSE:
                level_color = "\033[0;37m";
                break; // White
            default:
                level_color = "";
                break;
        }
        reset_color = "\033[0m";
    }

    printf("%s%c (%u) %s: %s%s\n", level_color, logLevelToChar(msg.level), msg.timestamp_ms, msg.tag,
           msg.message, reset_color);
#endif
}

void ConsoleSink::flush()
{
#ifdef ESP_PLATFORM
    // ESP-IDF logs are unbuffered
#else
    fflush(stdout);
#endif
}

const char *ConsoleSink::getName() const
{
    return "ConsoleSink";
}

} // namespace lopcore
