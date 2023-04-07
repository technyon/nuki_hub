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
    _inst = this;
    loadPinConfiguration();

    _inst->init(nuki);
}

void Gpio::init(NukiWrapper* nuki)
{
    _nuki = nuki;

    for(const auto& entry : _inst->_pinConfiguration)
    {
        const auto it = std::find(_inst->availablePins().begin(), _inst->availablePins().end(), entry.pin);

        if(it == _inst->availablePins().end())
        {
            continue;
        }

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

const std::vector<uint8_t>& Gpio::availablePins() const
{
    return _availablePins;
}

void Gpio::loadPinConfiguration()
{
    size_t storedLength = _preferences->getBytesLength(preference_gpio_configuration);
    if(storedLength == 0)
    {
        return;
    }

    uint8_t serialized[storedLength];
    memset(serialized, 0, sizeof(serialized));

    size_t size = _preferences->getBytes(preference_gpio_configuration, serialized, sizeof(serialized));

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
        entry.pin = serialized[i * 2];
        entry.role = (PinRole) serialized[(i * 2 + 1)];
        if(entry.role != PinRole::Disabled)
        {
            _pinConfiguration.push_back(entry);
        }
    }
}

void Gpio::savePinConfiguration(const std::vector<PinEntry> &pinConfiguration)
{
    int8_t serialized[pinConfiguration.size() * 2];
    memset(serialized, 0, sizeof(serialized));

    int len = pinConfiguration.size();
    for(int i=0; i < len; i++)
    {
        const auto& entry = pinConfiguration[i];

        if(entry.role != PinRole::Disabled)
        {
            serialized[i * 2] = entry.pin;
            serialized[i * 2 + 1] = (int8_t) entry.role;
        }
    }

    _preferences->putBytes(preference_gpio_configuration, serialized, sizeof(serialized));
}

const std::vector<PinEntry> &Gpio::pinConfiguration() const
{
    return _pinConfiguration;
}

String Gpio::getRoleDescription(PinRole role) const
{
    switch(role)
    {
        case PinRole::Disabled:
            return "Disabled";
        case PinRole::InputLock:
            return "Input: Lock";
        case PinRole::InputUnlock:
            return "Input: Unlock";
        case PinRole::InputUnlatch:
            return "Input: Unlatch";
        case PinRole::OutputHighLocked:
            return "Output: High when locked";
        case PinRole::OutputHighUnlocked:
            return "Output: High when unlocked";
        default:
            return "Unknown";
    }
}

void Gpio::getConfigurationText(String& text, const std::vector<PinEntry>& pinConfiguration) const
{
    text.clear();

    for(const auto& entry : pinConfiguration)
    {
        if(entry.role != PinRole::Disabled)
        {
            text.concat(entry.pin);
            if(entry.pin < 10)
            {
                text.concat(' ');
            }
            text.concat(": ");
            text.concat(getRoleDescription(entry.role));
            text.concat("\n\r");
        }
    }
}

const std::vector<PinRole>& Gpio::getAllRoles() const
{
    return _allRoles;
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