/**
 * @file FreeRTOS.h
 * @brief Mock FreeRTOS API for host testing
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <thread>

#ifdef __cplusplus
extern "C" {
#endif

// FreeRTOS types
typedef void *TaskHandle_t;
typedef int32_t BaseType_t;
typedef uint32_t TickType_t;
typedef uint32_t UBaseType_t;

// FreeRTOS constants
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS pdTRUE
#define pdFAIL pdFALSE

// Tick conversion (assuming 1000 Hz tick rate)
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(ms) ((TickType_t) (ms))
#define pdTICKS_TO_MS(ticks) ((TickType_t) (ticks))

// Task priorities
#define tskIDLE_PRIORITY 0
#define configMAX_PRIORITIES 25

// Mock vTaskDelay - use std::this_thread::sleep_for
inline void vTaskDelay(TickType_t xTicksToDelay)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(xTicksToDelay));
}

// Mock task tick count
inline TickType_t xTaskGetTickCount(void)
{
    static TickType_t tick_count = 0;
    return tick_count++;
}

#ifdef __cplusplus
}
#endif
