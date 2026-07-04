/**
 * @file semphr.h
 * @brief Mock FreeRTOS semaphore API for host testing
 *
 * Functional binary semaphore backed by std::mutex + condition_variable so
 * host tests exercising task-stop handshakes behave like the real thing.
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *SemaphoreHandle_t;

namespace MockFreeRTOS
{
struct MockBinarySemaphore
{
    std::mutex m;
    std::condition_variable cv;
    bool available{false};
};
} // namespace MockFreeRTOS

inline SemaphoreHandle_t xSemaphoreCreateBinary(void)
{
    return new MockFreeRTOS::MockBinarySemaphore();
}

inline void vSemaphoreDelete(SemaphoreHandle_t xSemaphore)
{
    delete static_cast<MockFreeRTOS::MockBinarySemaphore *>(xSemaphore);
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore)
{
    auto *sem = static_cast<MockFreeRTOS::MockBinarySemaphore *>(xSemaphore);
    if (sem == nullptr)
    {
        return pdFALSE;
    }
    {
        std::lock_guard<std::mutex> lock(sem->m);
        sem->available = true;
    }
    sem->cv.notify_one();
    return pdTRUE;
}

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait)
{
    auto *sem = static_cast<MockFreeRTOS::MockBinarySemaphore *>(xSemaphore);
    if (sem == nullptr)
    {
        return pdFALSE;
    }
    std::unique_lock<std::mutex> lock(sem->m);
    if (!sem->cv.wait_for(lock, std::chrono::milliseconds(xTicksToWait), [sem] { return sem->available; }))
    {
        return pdFALSE;
    }
    sem->available = false;
    return pdTRUE;
}

#ifdef __cplusplus
}
#endif
