#pragma once

#include "NetworkLock.h"
#include "NukiConstants.h"
#include "NukiDataTypes.h"
#include "BleScanner.h"
#include "NukiLock.h"
#include "Gpio.h"
#include "LockActionResult.h"
#include "NukiDeviceId.h"

class NukiWrapper : public Nuki::SmartlockEventHandler
{
public:
    NukiWrapper(const std::string& deviceName, NukiDeviceId* deviceId, BleScanner::Scanner* scanner, NetworkLock* network, Gpio* gpio, Preferences* preferences);
    virtual ~NukiWrapper();

    void initialize(const bool& firstStart);
    void update();

    void lock();
    void unlock();
    void unlatch();
    void lockngo();
    void lockngounlatch();

    bool isPinSet();
    bool isPinValid();
    void setPin(const uint16_t pin);
    void unpair();

    void disableHASS();

    void disableWatchdog();

    const NukiLock::KeyTurnerState& keyTurnerState();
    const bool isPaired() const;
    const bool hasKeypad() const;
    bool hasDoorSensor() const;
    const BLEAddress getBleAddress() const;

    std::string firmwareVersion() const;
    std::string hardwareVersion() const;

    void notify(Nuki::EventType eventType) override;

private:
    static LockActionResult onLockActionReceivedCallback(const char* value);
    static void onConfigUpdateReceivedCallback(const char* value);
    static void onKeypadCommandReceivedCallback(const char* command, const uint& id, const String& name, const String& code, const int& enabled);
    static void onKeypadJsonCommandReceivedCallback(const char* value);
    static void onTimeControlCommandReceivedCallback(const char* value);
    static void gpioActionCallback(const GpioAction& action, const int& pin);
    void onKeypadCommandReceived(const char* command, const uint& id, const String& name, const String& code, const int& enabled);
    void onConfigUpdateReceived(const char* value);
    void onKeypadJsonCommandReceived(const char* value);
    void onTimeControlCommandReceived(const char* value);

    void updateKeyTurnerState();
    void updateBatteryState();
    void updateConfig();
    void updateAuthData();
    void updateKeypad();
    void updateTimeControl(bool retrieved);
    void postponeBleWatchdog();

    void updateGpioOutputs();

    void readConfig();
    void readAdvancedConfig();

    void setupHASS();

    void printCommandResult(Nuki::CmdResult result);

    NukiLock::LockAction lockActionToEnum(const char* str); // char array at least 14 characters
    Nuki::AdvertisingMode advertisingModeToEnum(const char* str);
    Nuki::TimeZoneId timeZoneToEnum(const char* str);
    uint8_t fobActionToInt(const char *str);
    NukiLock::ButtonPressAction buttonPressActionToEnum(const char* str);
    Nuki::BatteryType batteryTypeToEnum(const char* str);

    std::string _deviceName;
    NukiDeviceId* _deviceId = nullptr;
    NukiLock::NukiLock _nukiLock;
    BleScanner::Scanner* _bleScanner = nullptr;
    NetworkLock* _network = nullptr;
    Gpio* _gpio = nullptr;
    Preferences* _preferences;
    int _intervalLockstate = 0; // seconds
    int _intervalBattery = 0; // seconds
    int _intervalConfig = 60 * 60; // seconds
    int _intervalKeypad = 0; // seconds
    int _restartBeaconTimeout = 0; // seconds
    bool _publishAuthData = false;
    bool _clearAuthData = false;
    std::vector<uint16_t> _keypadCodeIds;
    std::vector<uint8_t> _timeControlIds;

    NukiLock::KeyTurnerState _lastKeyTurnerState;
    NukiLock::KeyTurnerState _keyTurnerState;

    NukiLock::BatteryReport _batteryReport;
    NukiLock::BatteryReport _lastBatteryReport;

    NukiLock::Config _nukiConfig = {0};
    NukiLock::AdvancedConfig _nukiAdvancedConfig = {0};
    bool _nukiConfigValid = false;
    bool _nukiAdvancedConfigValid = false;
    bool _hassEnabled = false;
    bool _hassSetupCompleted = false;

    bool _paired = false;
    bool _statusUpdated = false;
    bool _hasKeypad = false;
    bool _keypadEnabled = false;
    uint _maxKeypadCodeCount = 0;
    bool _configRead = false;
    int _nrOfRetries = 0;
    int _retryDelay = 0;
    int _retryCount = 0;
    int _retryConfigCount = 0;
    int _retryLockstateCount = 0;
    long _rssiPublishInterval = 0;
    unsigned long _nextRetryTs = 0;
    unsigned long _nextLockStateUpdateTs = 0;
    unsigned long _nextBatteryReportTs = 0;
    unsigned long _nextConfigUpdateTs = 0;
    unsigned long _nextTimeControlUpdateTs = 0;
    unsigned long _nextKeypadUpdateTs = 0;
    unsigned long _nextRssiTs = 0;
    unsigned long _lastRssi = 0;
    unsigned long _disableBleWatchdogTs = 0;
    std::string _firmwareVersion = "";
    std::string _hardwareVersion = "";
    volatile NukiLock::LockAction _nextLockAction = (NukiLock::LockAction)0xff;
};
