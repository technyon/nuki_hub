#pragma once

#include "NukiNetworkLock.h"
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
    NukiWrapper(const std::string& deviceName, NukiDeviceId* deviceId, BleScanner::Scanner* scanner, NukiNetworkLock* network, Gpio* gpio, Preferences* preferences);
    virtual ~NukiWrapper();

    void initialize(const bool& firstStart);
    void readSettings();
    void update();

    void lock();
    void unlock();
    void unlatch();
    void lockngo();
    void lockngounlatch();

    bool isPinSet();
    bool isPinValid();
    void setPin(const uint16_t pin);
    uint16_t getPin();
    void unpair();

    void disableHASS();

    void disableWatchdog();

    const NukiLock::KeyTurnerState& keyTurnerState();
    const bool isPaired() const;
    const bool hasKeypad() const;
    bool hasDoorSensor() const;
    bool offConnected();
    const BLEAddress getBleAddress() const;

    std::string firmwareVersion() const;
    std::string hardwareVersion() const;

    void notify(Nuki::EventType eventType) override;

private:
    static LockActionResult onLockActionReceivedCallback(const char* value);
    static void onOfficialUpdateReceivedCallback(const char* topic, const char* value);
    static void onConfigUpdateReceivedCallback(const char* value);
    static void onKeypadCommandReceivedCallback(const char* command, const uint& id, const String& name, const String& code, const int& enabled);
    static void onKeypadJsonCommandReceivedCallback(const char* value);
    static void onTimeControlCommandReceivedCallback(const char* value);
    static void onAuthCommandReceivedCallback(const char* value);
    static void gpioActionCallback(const GpioAction& action, const int& pin);
    void onKeypadCommandReceived(const char* command, const uint& id, const String& name, const String& code, const int& enabled);
    void onOfficialUpdateReceived(const char* topic, const char* value);
    void onConfigUpdateReceived(const char* value);
    void onKeypadJsonCommandReceived(const char* value);
    void onTimeControlCommandReceived(const char* value);
    void onAuthCommandReceived(const char* value);

    void updateKeyTurnerState();
    void updateBatteryState();
    void updateConfig();
    void updateAuthData(bool retrieved);
    void updateKeypad(bool retrieved);
    void updateTimeControl(bool retrieved);
    void updateAuth(bool retrieved);
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
    NukiNetworkLock* _network = nullptr;
    Gpio* _gpio = nullptr;
    Preferences* _preferences;
    int _intervalLockstate = 0; // seconds
    int _intervalHybridLockstate = 0; // seconds
    int _intervalBattery = 0; // seconds
    int _intervalConfig = 60 * 60; // seconds
    int _intervalKeypad = 0; // seconds
    int _restartBeaconTimeout = 0; // seconds
    bool _publishAuthData = false;
    bool _clearAuthData = false;
    std::vector<uint16_t> _keypadCodeIds;
    std::vector<uint8_t> _timeControlIds;
    std::vector<uint32_t> _authIds;

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
    bool _offEnabled = false;
    bool _disableNonJSON = false;
    bool _paired = false;
    bool _statusUpdated = false;
    bool _hasKeypad = false;
    bool _keypadEnabled = false;
    uint _maxKeypadCodeCount = 0;
    uint _maxTimeControlEntryCount = 0;
    uint _maxAuthEntryCount = 0;
    int _nrOfRetries = 0;
    int _retryDelay = 0;
    int _retryConfigCount = 0;
    int _retryLockstateCount = 0;
    int _rssiPublishInterval = 0;
    int64_t _statusUpdatedTs = 0;
    int64_t _nextRetryTs = 0;
    int64_t _nextLockStateUpdateTs = 0;
    int64_t _nextHybridLockStateUpdateTs = 0;
    int64_t _nextBatteryReportTs = 0;
    int64_t _nextConfigUpdateTs = 0;
    int64_t _waitAuthLogUpdateTs = 0;
    int64_t _waitKeypadUpdateTs = 0;
    int64_t _waitTimeControlUpdateTs = 0;
    int64_t _waitAuthUpdateTs = 0;
    int64_t _nextKeypadUpdateTs = 0;
    int64_t _nextRssiTs = 0;
    int64_t _lastRssi = 0;
    int64_t _disableBleWatchdogTs = 0;
    uint32_t _basicLockConfigaclPrefs[16];
    uint32_t _advancedLockConfigaclPrefs[22];
    std::string _firmwareVersion = "";
    std::string _hardwareVersion = "";
    volatile NukiLock::LockAction _nextLockAction = (NukiLock::LockAction)0xff;
};
