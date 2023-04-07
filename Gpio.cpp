#include <esp32-hal.h>
#include "Gpio.h"
#include "Arduino.h"
#include "Pins.h"
#include "Logger.h"
#include "PreferencesKeys.h"

Gpio* Gpio::_inst = nullptr;
NukiWrapper* Gpio::_nuki = nullptr;
unsigned long Gpio::_lockedTs = 0;
const uint Gpio::_debounceTime = 1000;

Gpio::Gpio(Preferences* preferences, NukiWrapper* nuki)
: _preferences(preferences)
{
    loadPinConfiguration();

    _inst = this;
    _inst->init(nuki);
}

void Gpio::init(NukiWrapper* nuki)
{
    _nuki = nuki;

    for(const auto& entry : _inst->_pinConfiguration)
    {
        switch(entry.role)
        {
            case PinRole::InputLock:
                pinMode(entry.pin, INPUT_PULLUP);
                attachInterrupt(entry.pin, isrLock, FALLING);
                break;
            case PinRole::InputUnlock:
                pinMode(entry.pin, INPUT_PULLUP);
                attachInterrupt(entry.pin, isrUnlock, FALLING);
                break;
            case PinRole::InputUnlatch:
                pinMode(entry.pin, INPUT_PULLUP);
                attachInterrupt(entry.pin, isrUnlatch, FALLING);
                break;
            default:
                pinMode(entry.pin, OUTPUT);
                break;
        }
    }
}

void Gpio::isrLock()
{
    if(millis() < _lockedTs) return;
    _nuki->lock();
    _lockedTs = millis() + _debounceTime;
}

void Gpio::isrUnlock()
{
    if(millis() < _lockedTs) return;
    _nuki->unlock();
    _lockedTs = millis() + _debounceTime;
}

void Gpio::isrUnlatch()
{
    if(millis() < _lockedTs) return;
    _nuki->unlatch();
    _lockedTs = millis() + _debounceTime;
}

const std::vector<uint8_t>& Gpio::availablePins() const
{
    return _availablePins;
}

void Gpio::loadPinConfiguration()
{
    uint8_t serialized[_availablePins.size() * 2];

    size_t size = _preferences->getBytes(preference_gpio_configuration, serialized, _availablePins.size() * 2);
    if(size == 0)
    {
        return;
    }

    size_t numEntries = size / 2;

    _pinConfiguration.clear();
    _pinConfiguration.reserve(numEntries);
    for(int i=0; i < numEntries; i++)
    {
        PinEntry entry;
        entry.pin = i * 2;
        entry.role = (PinRole)(i * 2 +1);
        _pinConfiguration.push_back(entry);
    }
}

void Gpio::savePinConfiguration(const std::vector<PinEntry> &pinConfiguration)
{
    uint8_t serialized[pinConfiguration.size() * 2];

    int len = pinConfiguration.size();
    for(int i=0; i < len; i++)
    {
        const auto& entry = pinConfiguration[i];
        serialized[i * 2] = entry.pin;
        serialized[i * 2 + 1] = (int8_t)entry.role;
    }

    _preferences->putBytes(preference_gpio_configuration, serialized, sizeof(serialized));
}
