/**
 * @file timers.h
 * @brief Mock FreeRTOS software timers API for host testing
 */

#pragma once

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

// Timer handle type
typedef void *TimerHandle_t;

// Timer callback function type
typedef void (*TimerCallbackFunction_t)(TimerHandle_t xTimer);

// Timer auto-reload constants
#define pdTRUE 1
#define pdFALSE 0

/**
 * @brief Create a timer (mock - returns dummy handle)
 */
inline TimerHandle_t xTimerCreate(const char *const pcTimerName,
                                  const TickType_t xTimerPeriodInTicks,
                                  const UBaseType_t uxAutoReload,
                                  void *const pvTimerID,
                                  TimerCallbackFunction_t pxCallbackFunction)
{
    (void) pcTimerName;
    (void) xTimerPeriodInTicks;
    (void) uxAutoReload;
    (void) pvTimerID;
    (void) pxCallbackFunction;

    // Return a mock timer handle
    return reinterpret_cast<TimerHandle_t>(0x2000);
}

/**
 * @brief Start a timer (mock - always succeeds)
 */
inline BaseType_t xTimerStart(TimerHandle_t xTimer, TickType_t xBlockTime)
{
    (void) xTimer;
    (void) xBlockTime;
    return pdPASS;
}

/**
 * @brief Stop a timer (mock - always succeeds)
 */
inline BaseType_t xTimerStop(TimerHandle_t xTimer, TickType_t xBlockTime)
{
    (void) xTimer;
    (void) xBlockTime;
    return pdPASS;
}

/**
 * @brief Reset a timer (mock - always succeeds)
 */
inline BaseType_t xTimerReset(TimerHandle_t xTimer, TickType_t xBlockTime)
{
    (void) xTimer;
    (void) xBlockTime;
    return pdPASS;
}

/**
 * @brief Delete a timer (mock - always succeeds)
 */
inline BaseType_t xTimerDelete(TimerHandle_t xTimer, TickType_t xBlockTime)
{
    (void) xTimer;
    (void) xBlockTime;
    return pdPASS;
}

/**
 * @brief Get timer ID (mock)
 */
inline void *pvTimerGetTimerID(TimerHandle_t xTimer)
{
    (void) xTimer;
    return nullptr;
}

/**
 * @brief Check if timer is active (mock - always returns false)
 */
inline BaseType_t xTimerIsTimerActive(TimerHandle_t xTimer)
{
    (void) xTimer;
    return pdFALSE;
}

#ifdef __cplusplus
}
#endif
