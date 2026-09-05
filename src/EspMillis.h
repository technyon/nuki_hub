#pragma once

#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "esp_task_wdt.h"

inline int64_t espMillis()
{
    return esp_timer_get_time() / 1000;
}

inline static void espDelay(TickType_t durationMs)
{
    vTaskDelay(durationMs / portTICK_PERIOD_MS);
}

inline static void espDelayAck(TickType_t durationMs)
{
    if (esp_task_wdt_status(NULL) == ESP_OK)
    {
        esp_task_wdt_reset();
    }
    espDelay(durationMs);
}