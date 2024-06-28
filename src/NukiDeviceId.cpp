#include <string>
#include <cstring>
#include <Arduino.h>
#include "NukiDeviceId.h"
#include "PreferencesKeys.h"

NukiDeviceId::NukiDeviceId(Preferences* preferences, const std::string& preferencesId)
: _preferences(preferences),
  _preferencesId(preferencesId)
{
    _deviceId = _preferences->getUInt(_preferencesId.c_str());

    if(_deviceId == 0)
    {
        assignNewId();
    }
}

uint32_t NukiDeviceId::get()
{
    return _deviceId;
}

void NukiDeviceId::assignId(const uint32_t& id)
{
    _deviceId = id;
    _preferences->putUInt(_preferencesId.c_str(), id);
}

void NukiDeviceId::assignNewId()
{
    assignId(getRandomId());
}

uint32_t NukiDeviceId::getRandomId()
{
    uint8_t rnd[4];
    for(int i=0; i<4; i++)
    {
        rnd[i] = random(255);
    }
    uint32_t deviceId;
    memcpy(&deviceId, &rnd, sizeof(deviceId));
    return deviceId;
}
