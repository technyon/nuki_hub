#pragma once

#include "esp_err.h"

enum class PsramInitState
{
    NotConfigured,
    InitialisedAtBoot,
    InitialisedLate,
    Failed
};

void psramLateInit();
PsramInitState psramInitState();
esp_err_t psramLateInitError();
