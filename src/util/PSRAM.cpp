#include "esp_psram.h"
#include "esp32-hal.h"
#include "PSRAM.h"

const bool PSRAM::found(void)
{
#if CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || DISABLE_PSRAMCHECK || CORE32SOLO1
    return psramFound();
#else
    return psramFound() && esp_psram_is_initialized();
#endif
}
