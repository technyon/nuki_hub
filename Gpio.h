#pragma once


#include "NukiWrapper.h"

enum class PinRole
{
    Disabled,
    InputLock,
    InputUnlock,
    InputUnlatch,
    OutputHighLocked,
    OutputHighUnlocked,
};

struct PinEntry
{
    uint8_t pin = 0;
    PinRole role = PinRole::Disabled;
};

class Gpio
{
public:
    Gpio(Preferences* preferences, NukiWrapper* nuki);
    static void init(NukiWrapper* nuki);

    void loadPinConfiguration();
    void savePinConfiguration(const std::vector<PinEntry>& pinConfiguration);

    const std::vector<uint8_t>& availablePins() const;
    const std::vector<PinEntry>& pinConfiguration() const;

    String getRoleDescription(PinRole role) const;
    void getConfigurationText(String& text, const std::vector<PinEntry>& pinConfiguration) const;

    const std::vector<PinRole>& getAllRoles() const;

private:
    const std::vector<uint8_t> _availablePins = { 2, 4, 5, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 32, 33 };
    const std::vector<PinRole> _allRoles =
        {
            PinRole::Disabled,
            PinRole::InputLock,
            PinRole::InputUnlock,
            PinRole::InputUnlatch,
            PinRole::OutputHighLocked,
            PinRole::OutputHighUnlocked
        };

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
