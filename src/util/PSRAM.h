#pragma once

class PSRAM
{
    public:
    // this function is a replacement for `psramFound()`.
// `psramFound()` can return true even if no PSRAM is actually installed
// This new version also checks `esp_spiram_is_initialized` to know if the PSRAM is initialized
    static const bool found(void);

};
