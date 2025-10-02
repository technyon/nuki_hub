#pragma once

#include <cstdint>
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
    Ethernet,
    OutputHighMqttConnected,
    OutputHighNetworkConnected,
    OutputHighBluetoothCommError
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
    GeneralInput,
    None
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

    void addCallback(std::function<void(const GpioAction&, const int&)> callback);

    void loadPinConfiguration();
    void savePinConfiguration(const std::vector<PinEntry>& pinConfiguration);

    const std::vector<uint8_t>& availablePins() const;
    const std::vector<PinEntry>& pinConfiguration() const;
    const std::vector<int> getDisabledPins() const;
    const PinRole getPinRole(const int& pin) const;
    const std::vector<uint8_t> getPinsWithRole(PinRole role) const;

    String getRoleDescription(const PinRole& role) const;

    void getConfigurationText(String& text, const std::vector<PinEntry>& pinConfiguration, const String& linebreak = "\n") const;

    const std::vector<PinRole>& getAllRoles() const;

    void setPinOutput(const uint8_t& pin, const uint8_t& state);
    void setPins();

private:
    void IRAM_ATTR notify(const GpioAction& action, const int& pin);
    void IRAM_ATTR onTimer();
    bool IRAM_ATTR isTriggered(const PinEntry& pinEntry);
    GpioAction IRAM_ATTR getGpioAction(const PinRole& role) const;
    static void IRAM_ATTR isrOnTimer();

#if defined(CONFIG_IDF_TARGET_ESP32C3)
    //Based on https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/gpio.html and https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf
    const std::vector<uint8_t> _availablePins = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 18, 19, 20, 21 };
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    //Based on https://github.com/atomic14/esp32-s3-pinouts?tab=readme-ov-file and https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/gpio.html and https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf
    const std::vector<uint8_t> _availablePins = { 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21, 38, 39, 40, 41, 42 };
#elif defined(CONFIG_IDF_TARGET_ESP32C5)
    //Based on https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/api-reference/peripherals/gpio.html and https://www.espressif.com/sites/default/files/documentation/esp32-c5_datasheet_en.pdf
    const std::vector<uint8_t> _availablePins = { 0, 1, 3, 4, 5, 6, 8, 9, 10, 11, 12, 15, 23, 24, 26 };
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
    //Based on https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/peripherals/gpio.html and https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf
    const std::vector<uint8_t> _availablePins = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 13, 15, 16, 17, 18, 19, 20, 21, 22, 23 };
#elif defined(CONFIG_IDF_TARGET_ESP32P4)
    //Based on https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/peripherals/gpio.html
    const std::vector<uint8_t> _availablePins = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33 };
#elif defined(CONFIG_IDF_TARGET_ESP32H2)
    //Based on https://docs.espressif.com/projects/esp-idf/en/latest/esp32h2/api-reference/peripherals/gpio.html and https://www.espressif.com/sites/default/files/documentation/esp32-h2_datasheet_en.pdf
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
        PinRole::OutputHighMqttConnected,
        PinRole::OutputHighNetworkConnected,
        PinRole::OutputHighBluetoothCommError,
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

    std::vector<std::function<void(const GpioAction&, const int&)>> _callbacks;

    static Gpio* _inst;

    std::vector<uint8_t> _triggerState;
    hw_timer_t* timer = nullptr;

    bool _first = true;

    Preferences* _preferences = nullptr;
};
