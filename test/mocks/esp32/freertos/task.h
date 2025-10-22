/**
 * @file task.h
 * @brief Mock FreeRTOS task API for host testing
 */

#pragma once

#include <map>
#include <mutex>
#include <thread>

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

// Task function type
typedef void (*TaskFunction_t)(void *);

// Mock task storage
namespace MockFreeRTOS
{
struct MockTask
{
    std::thread thread;
    bool running{true};
    void *param{nullptr};

    MockTask() = default;
    MockTask(MockTask &&) = default;
    MockTask &operator=(MockTask &&) = default;
    ~MockTask()
    {
        running = false;
        if (thread.joinable())
        {
            thread.join();
        }
    }
};

inline std::map<TaskHandle_t, MockTask> &getTasks()
{
    static std::map<TaskHandle_t, MockTask> tasks;
    return tasks;
}

inline std::mutex &getMutex()
{
    static std::mutex mtx;
    return mtx;
}
} // namespace MockFreeRTOS

/**
 * @brief Create a new task (mock implementation)
 */
inline BaseType_t xTaskCreate(TaskFunction_t pvTaskCode,
                              const char *const pcName,
                              const uint32_t usStackDepth,
                              void *const pvParameters,
                              UBaseType_t uxPriority,
                              TaskHandle_t *const pvCreatedTask)
{
    (void) pcName;
    (void) usStackDepth;
    (void) uxPriority;

    try
    {
        std::lock_guard<std::mutex> lock(MockFreeRTOS::getMutex());

        // Create a mock task handle
        TaskHandle_t handle = reinterpret_cast<TaskHandle_t>(
            static_cast<uintptr_t>(MockFreeRTOS::getTasks().size() + 1));

        // Create the task but don't start the thread in mock
        // (actual threading would complicate tests)
        MockFreeRTOS::MockTask task;
        task.param = pvParameters;
        task.running = true;

        MockFreeRTOS::getTasks()[handle] = std::move(task);

        if (pvCreatedTask)
        {
            *pvCreatedTask = handle;
        }

        return pdPASS;
    }
    catch (...)
    {
        return pdFAIL;
    }
}

/**
 * @brief Delete a task (mock implementation)
 */
inline void vTaskDelete(TaskHandle_t xTaskToDelete)
{
    std::lock_guard<std::mutex> lock(MockFreeRTOS::getMutex());

    if (xTaskToDelete == nullptr)
    {
        // Delete self - in mock, just return
        return;
    }

    auto &tasks = MockFreeRTOS::getTasks();
    auto it = tasks.find(xTaskToDelete);
    if (it != tasks.end())
    {
        it->second.running = false;
        tasks.erase(it);
    }
}

/**
 * @brief Get current task handle (mock)
 */
inline TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
    // Return a dummy handle
    return reinterpret_cast<TaskHandle_t>(0x1000);
}

/**
 * @brief Suspend scheduler (mock - no-op)
 */
inline void vTaskSuspendAll(void)
{
    // No-op in mock
}

/**
 * @brief Resume scheduler (mock - no-op)
 */
inline BaseType_t xTaskResumeAll(void)
{
    return pdTRUE;
}

#ifdef __cplusplus
}
#endif
