#pragma once

#include <stdint.h>
#include <cstddef>
#include "esp_ota_ops.h"

class Ota
{
public:
    void updateFirmware(uint8_t* buf, size_t size);

    bool updateStarted();
    bool updateCompleted();
    void restart();

private:
    bool _updateStarted = false;
    bool _updateCompleted = false;
    esp_ota_handle_t otaHandler = 0;
};
