#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "EspMillis.h"

// The nuki and the network task look for work every 50 ms. Whoever hands one of them work
// (a command that arrived, a result to send) wakes it, so that the work starts at once and
// not at the task's next look.
//
// A semaphore of its own and not a task notification: NimBLE uses the notification of the
// calling task for its blocking reads and writes, a second user would end those early.
struct TaskWake
{
    SemaphoreHandle_t signal = nullptr;

    void begin()
    {
        if(signal == nullptr)
        {
            signal = xSemaphoreCreateBinary();
        }
    }

    void wake()
    {
        if(signal != nullptr)
        {
            xSemaphoreGive(signal);
        }
    }

    // Like espDelayAck(), ended early by wake(). For waits of up to four seconds.
    void wait(TickType_t durationMs)
    {
        wdtReset();
        if(signal != nullptr)
        {
            xSemaphoreTake(signal, pdMS_TO_TICKS(durationMs));
        }
        else
        {
            espDelay(durationMs);
        }
    }
};

inline TaskWake nukiTaskWake;
inline TaskWake networkTaskWake;
