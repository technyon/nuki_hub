#pragma once

#include "NukiOpener.h"
#include "NukiNetworkOpener.h"
#include "NukiOpenerConstants.h"
#include "NukiDataTypes.h"
#include "BleScanner.h"
#include "Gpio.h"
#include "NukiDeviceId.h"

class NukiOpenerWrapper : public NukiOpener::SmartlockEventHandler
{
public:
    NukiOpenerWrapper(const std::string& deviceName, NukiDeviceId* deviceId, BleScanner::Scanner* scanner, NukiNetworkOpener* network, Gpio* gpio, Preferences* preferences, char* buffer, size_t bufferSize);
    virtual ~NukiOpenerWrapper();

    void initialize();
    void readSettings();
    void update();

    void electricStrikeActuation();
    void activateRTO();
    void activateCM();
    void deactivateRtoCm();
    void deactivateRTO();
    void deactivateCM();

    bool hasConnected();
    bool isPinValid();
    void setPin(const uint16_t pin);
    const uint16_t getPin();

    void unpair();
    void disableWatchdog();

    const NukiOpener::OpenerState& keyTurnerState();
    const bool isPaired() const;
    const bool hasKeypad() const;
    const BLEAddress getBleAddress() const;
    const uint8_t restartController() const;

    const std::string firmwareVersion() const;
    const std::string hardwareVersion() const;

    const BleScanner::Scanner* bleScanner();

    void notify(NukiOpener::EventType eventType) override;

private:
    static LockActionResult onLockActionReceivedCallback(const char* value);
    static void onConfigUpdateReceivedCallback(const char* value);
    static void onKeypadCommandReceivedCallback(const char* command, const uint& id, const String& name, const String& code, const int& enabled);
    static void onKeypadJsonCommandReceivedCallback(const char* value);
    static void onTimeControlCommandReceivedCallback(const char* value);
    static void onAuthCommandReceivedCallback(const char* value);
    static void gpioActionCallback(const GpioAction& action, const int& pin);

    void onKeypadCommandReceived(const char* command, const uint& id, const String& name, const String& code, const int& enabled);
    void onConfigUpdateReceived(const char* value);
    void onKeypadJsonCommandReceived(const char* value);
    void onTimeControlCommandReceived(const char* value);
    void onAuthCommandReceived(const char* value);

    bool updateKeyTurnerState();
    void updateBatteryState();
    void updateConfig();
    void updateAuthData(bool retrieved);
    void updateKeypad(bool retrieved);
    void updateTimeControl(bool retrieved);
    void updateAuth(bool retrieved);
    void postponeBleWatchdog();
    void updateTime();

    void updateGpioOutputs();

    void readConfig();
    void readAdvancedConfig();

    void printCommandResult(Nuki::CmdResult result);

    const NukiOpener::LockAction lockActionToEnum(const char* str) const; // char array at least 24 characters
    const Nuki::AdvertisingMode advertisingModeToEnum(const char* str) const;
    const Nuki::TimeZoneId timeZoneToEnum(const char* str) const;
    const uint8_t fobActionToInt(const char *str) const;
    const uint8_t operatingModeToInt(const char *str) const;
    const uint8_t doorbellSuppressionToInt(const char *str) const;
    const uint8_t soundToInt(const char *str) const;
    const NukiOpener::ButtonPressAction buttonPressActionToEnum(const char* str) const;
    const Nuki::BatteryType batteryTypeToEnum(const char* str) const;

    std::string _deviceName;
    NukiDeviceId* _deviceId = nullptr;
    NukiOpener::NukiOpener _nukiOpener;
    BleScanner::Scanner* _bleScanner = nullptr;
    NukiNetworkOpener* _network = nullptr;
    Gpio* _gpio = nullptr;
    Preferences* _preferences = nullptr;
    int _intervalLockstate = 0; // seconds
    int _intervalBattery = 0; // seconds
    int _intervalConfig = 60 * 60; // seconds
    int _intervalKeypad = 0; // seconds
    int _restartBeaconTimeout = 0; // seconds
    bool _publishAuthData = false;
    bool _clearAuthData = false;
    bool _disableNonJSON = false;
    bool _checkKeypadCodes = false;
    bool _pairedAsApp = false;
    int _nrOfRetries = 0;
    int _retryDelay = 0;
    int _retryConfigCount = 0;
    int _retryLockstateCount = 0;
    int64_t _nextRetryTs = 0;
    int _invalidCount = 0;
    int64_t _lastCodeCheck = 0;
    std::vector<uint16_t> _keypadCodeIds;
    std::vector<uint32_t> _keypadCodes;
    std::vector<uint8_t> _timeControlIds;
    std::vector<uint32_t> _authIds;

    NukiOpener::OpenerState _lastKeyTurnerState;
    NukiOpener::OpenerState _keyTurnerState;

    NukiOpener::BatteryReport _batteryReport;
    NukiOpener::BatteryReport _lastBatteryReport;

    NukiOpener::Config _nukiConfig = {0};
    NukiOpener::AdvancedConfig _nukiAdvancedConfig = {0};
    bool _nukiConfigValid = false;
    bool _nukiAdvancedConfigValid = false;
    bool _hassEnabled = false;
    bool _hassSetupCompleted = false;

    bool _paired = false;
    bool _statusUpdated = false;
    int _newSignal = 0;
    bool _hasKeypad = false;
    bool _forceKeypad = false;
    bool _keypadEnabled = false;
    bool _forceId = false;
    bool _hasConnected = false;
    uint _maxKeypadCodeCount = 0;
    uint _maxTimeControlEntryCount = 0;
    uint _maxAuthEntryCount = 0;
    uint8_t _restartController = 0;
    int _rssiPublishInterval = 0;
    int64_t _statusUpdatedTs = 0;
    int64_t _nextLockStateUpdateTs = 0;
    int64_t _nextBatteryReportTs = 0;
    int64_t _nextConfigUpdateTs = 0;
    int64_t _waitAuthLogUpdateTs = 0;
    int64_t _waitKeypadUpdateTs = 0;
    int64_t _waitTimeControlUpdateTs = 0;
    int64_t _waitAuthUpdateTs = 0;
    int64_t _nextTimeUpdateTs = 0;
    int64_t _nextKeypadUpdateTs = 0;
    int64_t _nextPairTs = 0;
    int64_t _nextRssiTs = 0;
    int64_t _lastRssi = 0;
    int64_t _disableBleWatchdogTs = 0;
    uint32_t _basicOpenerConfigAclPrefs[16];
    uint32_t _advancedOpenerConfigAclPrefs[21];
    std::string _firmwareVersion = "";
    std::string _hardwareVersion = "";
    NukiOpener::LockAction _nextLockAction = (NukiOpener::LockAction)0xff;

    char* _buffer;
    const size_t _bufferSize;
};
