#pragma once
#include <cstdint>

inline int64_t espMillis()
{
    return esp_timer_get_time() / 1000;
}