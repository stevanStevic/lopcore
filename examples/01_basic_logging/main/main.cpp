/**
 * @file main.cpp
 * @brief Basic logging example for LopCore
 *
 * Demonstrates:
 * - Initializing the logger
 * - Adding console sink
 * - Using different log levels
 * - Using logging macros
 */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lopcore/logging/console_sink.hpp"
#include "lopcore/logging/logger.hpp"

#include "esp_random.h"
#include "esp_system.h"
#include "nvs_flash.h"

extern "C" void app_main(void)
{
    // Initialize NVS (required by ESP-IDF)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Get logger instance
    auto &logger = lopcore::Logger::getInstance();

    // Add console sink with colored output
    logger.addSink(std::make_unique<lopcore::ConsoleSink>());

    // Set global log level
    logger.setGlobalLevel(lopcore::LogLevel::INFO);

    // Log startup banner
    LOPCORE_LOGI("APP", "===========================================");
    LOPCORE_LOGI("APP", "LopCore Basic Logging Example");
    LOPCORE_LOGI("APP", "===========================================");

    // Demonstrate different log levels
    LOPCORE_LOGV("APP", "This is a verbose message (won't show at INFO level)");
    LOPCORE_LOGD("APP", "This is a debug message (won't show at INFO level)");
    LOPCORE_LOGI("APP", "This is an info message");
    LOPCORE_LOGW("APP", "This is a warning message");
    LOPCORE_LOGE("APP", "This is an error message");

    // Log with formatted strings
    int temperature = 25;
    float humidity = 65.5;
    LOPCORE_LOGI("SENSOR", "Temperature: %d°C, Humidity: %.1f%%", temperature, humidity);

    // Change log level at runtime
    LOPCORE_LOGI("APP", "Changing log level to DEBUG...");
    logger.setGlobalLevel(lopcore::LogLevel::DEBUG);

    LOPCORE_LOGD("APP", "Debug messages now visible!");
    LOPCORE_LOGV("APP", "Verbose messages still hidden (need VERBOSE level)");

    // Simulate periodic logging
    LOPCORE_LOGI("APP", "Starting periodic logging every 5 seconds...");

    int counter = 0;
    while (true)
    {
        LOPCORE_LOGI("LOOP", "Iteration %d - System running normally", counter++);

        // Simulate some sensor readings
        int sensorValue = esp_random() % 100;
        if (sensorValue > 80)
        {
            LOPCORE_LOGW("SENSOR", "High sensor value detected: %d", sensorValue);
        }
        else
        {
            LOPCORE_LOGD("SENSOR", "Sensor reading: %d", sensorValue);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
