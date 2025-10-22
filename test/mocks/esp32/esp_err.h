/**
 * @file esp_err.h
 * @brief Mock ESP-IDF error types for host testing
 */

#pragma once

#include <cstdint>

typedef int32_t esp_err_t;

// Common ESP-IDF error codes
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_INVALID_RESPONSE 0x108
#define ESP_ERR_INVALID_CRC 0x109
#define ESP_ERR_INVALID_VERSION 0x10A
#define ESP_ERR_INVALID_MAC 0x10B

// Error checking macros
#define ESP_ERROR_CHECK(x)                                                                                   \
    do                                                                                                       \
    {                                                                                                        \
        esp_err_t err_rc_ = (x);                                                                             \
        if (err_rc_ != ESP_OK)                                                                               \
        {                                                                                                    \
            /* In real ESP-IDF, this aborts */                                                               \
        }                                                                                                    \
    } while (0)

#define ESP_ERROR_CHECK_WITHOUT_ABORT(x) (x)

// Error to string conversion
inline const char *esp_err_to_name(esp_err_t error)
{
    switch (error)
    {
        case ESP_OK:
            return "ESP_OK";
        case ESP_FAIL:
            return "ESP_FAIL";
        case ESP_ERR_NO_MEM:
            return "ESP_ERR_NO_MEM";
        case ESP_ERR_INVALID_ARG:
            return "ESP_ERR_INVALID_ARG";
        case ESP_ERR_INVALID_STATE:
            return "ESP_ERR_INVALID_STATE";
        case ESP_ERR_TIMEOUT:
            return "ESP_ERR_TIMEOUT";
        default:
            return "UNKNOWN_ERROR";
    }
}
