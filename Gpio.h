#pragma once


#include "NukiWrapper.h"

enum class PinRole
{
    Undefined,
    InputLock,
    InputUnlock,
    InputUnlatch,
    OutputHighLocked,
    OutputHighUnlocked,
};

struct PinEntry
{
    uint8_t pin = 0;
    PinRole role = PinRole::Undefined;
};

class Gpio
{
public:
    Gpio(Preferences* preferences, NukiWrapper* nuki);
    static void init(NukiWrapper* nuki);

    void loadPinConfiguration();
    void savePinConfiguration(const std::vector<PinEntry>& pinConfiguration);

    const std::vector<uint8_t>& availablePins() const;

private:
    const std::vector<uint8_t> _availablePins = { 2, 4, 5, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 32, 33 };

    std::vector<PinEntry> _pinConfiguration;
    static const uint _debounceTime;

    static void IRAM_ATTR isrLock();
    static void IRAM_ATTR isrUnlock();
    static void IRAM_ATTR isrUnlatch();

    static Gpio* _inst;
    static NukiWrapper* _nuki;
    static unsigned long _lockedTs;

    Preferences* _preferences = nullptr;
};
