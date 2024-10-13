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
    InputLockNgo,
    InputLockNgoUnlatch,
    InputElectricStrikeActuation,
    InputActivateRTO,
    InputActivateCM,
    InputDeactivateRtoCm,
    InputDeactivateRTO,
    InputDeactivateCM,
    OutputHighLocked,
    OutputHighUnlocked,
    OutputHighMotorBlocked,
    OutputHighRtoActive,
    OutputHighCmActive,
    OutputHighRtoOrCmActive,
    GeneralOutput,
    GeneralInputPullDown,
    GeneralInputPullUp,
    Ethernet
};

enum class GpioAction
{
    Lock,
    Unlock,
    Unlatch,
    LockNgo,
    LockNgoUnlatch,
    ElectricStrikeActuation,
    ActivateRTO,
    ActivateCM,
    DeactivateRtoCm,
    DeactivateRTO,
    DeactivateCM,
    GeneralInput
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

    void addCallback(std::function<void(const GpioAction&, const int&)> callback);

    void loadPinConfiguration();
    void savePinConfiguration(const std::vector<PinEntry>& pinConfiguration);

    const std::vector<uint8_t>& availablePins() const;
    const std::vector<PinEntry>& pinConfiguration() const;
    const std::vector<int> getDisabledPins() const;
    const PinRole getPinRole(const int& pin) const;

    String getRoleDescription(PinRole role) const;
    void getConfigurationText(String& text, const std::vector<PinEntry>& pinConfiguration, const String& linebreak = "\n") const;

    const std::vector<PinRole>& getAllRoles() const;

    void setPinOutput(const uint8_t& pin, const uint8_t& state);

private:
    void IRAM_ATTR notify(const GpioAction& action, const int& pin);
    void IRAM_ATTR onTimer();
    bool IRAM_ATTR isTriggered(const PinEntry& pinEntry);
    static void inputCallback(const int & pin);

    #if defined(CONFIG_IDF_TARGET_ESP32C3)
    //Based on https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/peripherals/gpio.html and https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf
    const std::vector<uint8_t> _availablePins = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 18, 19, 20, 21 };
    #elif defined(CONFIG_IDF_TARGET_ESP32S3)
    //Based on https://github.com/atomic14/esp32-s3-pinouts?tab=readme-ov-file and https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32s3/api-reference/peripherals/gpio.html and https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf
    const std::vector<uint8_t> _availablePins = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48 };
    #elif defined(CONFIG_IDF_TARGET_ESP32C6)
    //Based on https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32c6/api-reference/peripherals/gpio.html and https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf
    const std::vector<uint8_t> _availablePins = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 13, 15, 16, 17, 18, 19, 20, 21, 22, 23 };
    #elif defined(CONFIG_IDF_TARGET_ESP32H2)
    //Based on https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32h2/api-reference/peripherals/gpio.html and https://www.espressif.com/sites/default/files/documentation/esp32-h2_datasheet_en.pdf
    const std::vector<uint8_t> _availablePins = { 0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 14, 22, 23, 24, 25, 26, 27 };
    #else
    //Based on https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
    const std::vector<uint8_t> _availablePins = { 2, 4, 5, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 32, 33 };
    #endif
    const std::vector<PinRole> _allRoles =
        {
            PinRole::Disabled,
            PinRole::InputLock,
            PinRole::InputUnlock,
            PinRole::InputUnlatch,
            PinRole::InputLockNgo,
            PinRole::InputLockNgoUnlatch,
            PinRole::InputElectricStrikeActuation,
            PinRole::InputActivateRTO,
            PinRole::InputActivateCM,
            PinRole::InputDeactivateRtoCm,
            PinRole::InputDeactivateRTO,
            PinRole::InputDeactivateCM,
            PinRole::OutputHighLocked,
            PinRole::OutputHighUnlocked,
            PinRole::OutputHighRtoActive,
            PinRole::OutputHighCmActive,
            PinRole::OutputHighRtoOrCmActive,
            PinRole::GeneralInputPullDown,
            PinRole::GeneralInputPullUp,
            PinRole::GeneralOutput,
            PinRole::Ethernet
        };

    std::vector<PinEntry> _pinConfiguration;
    static const uint _debounceTime;

    static void IRAM_ATTR isrOnTimer();
    static void IRAM_ATTR isrLock();
    static void IRAM_ATTR isrUnlock();
    static void IRAM_ATTR isrUnlatch();
    static void IRAM_ATTR isrLockNgo();
    static void IRAM_ATTR isrLockNgoUnlatch();
    static void IRAM_ATTR isrElectricStrikeActuation();
    static void IRAM_ATTR isrActivateRTO();
    static void IRAM_ATTR isrActivateCM();
    static void IRAM_ATTR isrDeactivateRtoCm();
    static void IRAM_ATTR isrDeactivateRTO();
    static void IRAM_ATTR isrDeactivateCM();

    std::vector<std::function<void(const GpioAction&, const int&)>> _callbacks;

    static Gpio* _inst;
    static int64_t _debounceTs;

    int asd = 0;
    std::vector<int8_t> _triggerCount;
    hw_timer_t* timer = nullptr;

    Preferences* _preferences = nullptr;
};
