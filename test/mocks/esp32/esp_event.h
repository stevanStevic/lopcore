/**
 * @file esp_event.h
 * @brief Mock ESP-IDF event types for host testing
 *
 * This file provides minimal mock definitions of ESP-IDF event system types
 * to allow compiling ESP-IDF dependent code in host test environment.
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <cstdint>

/**
 * @brief Mock type for ESP event base
 *
 * In real ESP-IDF, this is a const char* that identifies event families.
 * For host testing, we just need a compatible type definition.
 */
typedef const char *esp_event_base_t;
