#pragma once

#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "esp_task_wdt.h"

inline int64_t espMillis()
{
    return esp_timer_get_time() / 1000;
}

inline static void wdtReset()
{
    if (esp_task_wdt_status(NULL) == ESP_OK)
    {
        esp_task_wdt_reset();
    }
}

inline static void espDelay(TickType_t durationMs)
{
    vTaskDelay(durationMs / portTICK_PERIOD_MS);
}

inline static void espDelayAck(TickType_t durationMs)
{
//  Watchdog is at default of 5 seconds. 4 seconds is a safe threshold to reset the watchdog and avoid a reset during long delays.
    if (durationMs <= 4000)
    {
        wdtReset();
        espDelay(durationMs);
        return;
    }

//  Long delay, keep watchdog updated
    int64_t ts = espMillis();
    while (espMillis() - ts < durationMs)
    {
        wdtReset();
        espDelay(20);
    }
}