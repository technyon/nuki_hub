#pragma once

#if defined(CONFIG_SOC_SPIRAM_SUPPORTED) && defined(CONFIG_SPIRAM)
#include "esp_psram.h"
#endif

inline int certSize()
{
#if defined(CONFIG_SOC_SPIRAM_SUPPORTED) && defined(CONFIG_SPIRAM)
    if(esp_psram_get_size() > 0)
    {
        return 8000;
    }
    return 2200;
#endif

    return 2200;
}