#pragma once

#include <functional>
#include <Preferences.h>
#include <vector>

enum class PinRole
{
    Disabled,
    InputLock,
    InputUnlock,
    InputUnlatch,
    InputElectricStrikeActuation,
    InputActivateRTO,
    InputActivateCM,
    InputDeactivateRtoCm,
    OutputHighLocked,
    OutputHighUnlocked,
    OutputHighMotorBlocked,
    OutputHighRtoActive,
    OutputHighCmActive,
    OutputHighRtoOrCmActive
};

enum class GpioAction
{
    Lock,
    Unlock,
    Unlatch,
    ElectricStrikeActuation,
    ActivateRTO,
    ActivateCM,
    DeactivateRtoCm
};

struct PinEntry
{
    uint8_t pin = 0;
    PinRole role = PinRole::Disabled;
};

class Gpio
{
public:
    Gpio(Preferences* preferences);
    static void init();

    void migrateObsoleteSetting();

    void addCallback(std::function<void(const GpioAction&)> callback);

    void loadPinConfiguration();
    void savePinConfiguration(const std::vector<PinEntry>& pinConfiguration);

    const std::vector<uint8_t>& availablePins() const;
    const std::vector<PinEntry>& pinConfiguration() const;

    String getRoleDescription(PinRole role) const;
    void getConfigurationText(String& text, const std::vector<PinEntry>& pinConfiguration, const String& linebreak = "\n") const;

    const std::vector<PinRole>& getAllRoles() const;

    void setPinOutput(const uint8_t& pin, const uint8_t& state);

private:
    void notify(const GpioAction& action);

    const std::vector<uint8_t> _availablePins = { 2, 4, 5, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 32, 33 };
    const std::vector<PinRole> _allRoles =
        {
            PinRole::Disabled,
            PinRole::InputLock,
            PinRole::InputUnlock,
            PinRole::InputUnlatch,
            PinRole::InputElectricStrikeActuation,
            PinRole::InputActivateRTO,
            PinRole::InputActivateCM,
            PinRole::InputDeactivateRtoCm,
            PinRole::OutputHighLocked,
            PinRole::OutputHighUnlocked,
            PinRole::OutputHighRtoActive,
            PinRole::OutputHighCmActive,
            PinRole::OutputHighRtoOrCmActive,
        };

    std::vector<PinEntry> _pinConfiguration;
    static const uint _debounceTime;

    static void IRAM_ATTR isrLock();
    static void IRAM_ATTR isrUnlock();
    static void IRAM_ATTR isrUnlatch();
    static void IRAM_ATTR isrElectricStrikeActuation();
    static void IRAM_ATTR isrActivateRTO();
    static void IRAM_ATTR isrActivateCM();
    static void IRAM_ATTR isrDeactivateRtoCm();

    std::vector<std::function<void(const GpioAction&)>> _callbacks;

    static Gpio* _inst;
    static unsigned long _debounceTs;

    Preferences* _preferences = nullptr;
};
