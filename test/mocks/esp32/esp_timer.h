/**
 * @file esp_timer.h
 * @brief Mock ESP-IDF timer API for host testing
 */

#pragma once

#include <cstdint>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get time since boot in microseconds
 * @return Time in microseconds (mock returns incrementing value)
 */
inline int64_t esp_timer_get_time(void)
{
    // Simple mock: return a fixed value or use system time
    static int64_t mock_time = 0;
    return mock_time += 1000; // Increment by 1ms each call
}

#ifdef __cplusplus
}
#endif
