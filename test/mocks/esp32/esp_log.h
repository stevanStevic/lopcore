/**
 * @file esp_log.h
 * @brief Mock ESP-IDF logging for host testing
 *
 * This mock allows unit tests to compile and run on the host machine
 * without requiring the full ESP-IDF environment.
 */

#pragma once

#include <cstdarg>
#include <cstdio>

// Log levels (matching ESP-IDF)
typedef enum
{
    ESP_LOG_NONE = 0,
    ESP_LOG_ERROR,
    ESP_LOG_WARN,
    ESP_LOG_INFO,
    ESP_LOG_DEBUG,
    ESP_LOG_VERBOSE
} esp_log_level_t;

// Mock log function
inline void esp_log_write(esp_log_level_t level, const char *tag, const char *format, ...)
{
    const char *level_str[] = {"", "E", "W", "I", "D", "V"};

    if (level > ESP_LOG_VERBOSE)
        return;

    printf("%s (%s) ", level_str[level], tag);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}

// Mock logging macros
#define ESP_LOGE(tag, format, ...) esp_log_write(ESP_LOG_ERROR, tag, format, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) esp_log_write(ESP_LOG_WARN, tag, format, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) esp_log_write(ESP_LOG_INFO, tag, format, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) esp_log_write(ESP_LOG_DEBUG, tag, format, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) esp_log_write(ESP_LOG_VERBOSE, tag, format, ##__VA_ARGS__)

// Mock log level control
inline void esp_log_level_set(const char *tag, esp_log_level_t level)
{
    (void) tag;
    (void) level;
}
