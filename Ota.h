#pragma once

#include <stdint.h>
#include <cstddef>
#include "esp_ota_ops.h"

class Ota
{
public:
    void updateFirmware(uint8_t* buf, size_t size);

    bool updateStarted();

private:
    bool _updateStarted = false;
    esp_ota_handle_t otaHandler = 0;
};
