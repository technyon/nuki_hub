#pragma once

#include <cstdint>
#include <Preferences.h>

class NukiDeviceId
{
public:
    NukiDeviceId(Preferences* preferences, const std::string& preferencesId);

    uint32_t get();

    void assignId(const uint32_t& id);
    void assignNewId();

private:
    uint32_t getRandomId();

    Preferences* _preferences;
    const std::string _preferencesId;
    uint32_t _deviceId = 0;
};