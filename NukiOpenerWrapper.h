#pragma once

#include "NukiOpener.h"
#include "NetworkOpener.h"
#include "NukiOpenerConstants.h"
#include "NukiDataTypes.h"
#include "BleScanner.h"
#include "Gpio.h"
#include "NukiDeviceId.h"

class NukiOpenerWrapper : public NukiOpener::SmartlockEventHandler
{
public:
    NukiOpenerWrapper(const std::string& deviceName, NukiDeviceId* deviceId, BleScanner::Scanner* scanner, NetworkOpener* network, Gpio* gpio, Preferences* preferences);
    virtual ~NukiOpenerWrapper();

    void initialize();
    void update();

    void electricStrikeActuation();
    void activateRTO();
    void activateCM();
    void deactivateRtoCm();
    void deactivateRTO();
    void deactivateCM();

    bool isPinSet();
    void setPin(const uint16_t pin);

    void unpair();

    void disableHASS();

    void disableWatchdog();

    const NukiOpener::OpenerState& keyTurnerState();
    const bool isPaired() const;
    const bool hasKeypad() const;
    const BLEAddress getBleAddress() const;

    std::string firmwareVersion() const;
    std::string hardwareVersion() const;

    BleScanner::Scanner* bleScanner();

    void notify(NukiOpener::EventType eventType) override;

private:
    static LockActionResult onLockActionReceivedCallback(const char* value);
    static void onConfigUpdateReceivedCallback(const char* topic, const char* value);
    static void onKeypadCommandReceivedCallback(const char* command, const uint& id, const String& name, const String& code, const int& enabled);
    static void gpioActionCallback(const GpioAction& action, const int& pin);
    void onConfigUpdateReceived(const char* topic, const char* value);
    void onKeypadCommandReceived(const char* command, const uint& id, const String& name, const String& code, const int& enabled);

    void updateKeyTurnerState();
    void updateBatteryState();
    void updateConfig();
    void updateAuthData();
    void updateKeypad();
    void postponeBleWatchdog();

    void updateGpioOutputs();

    void readConfig();
    void readAdvancedConfig();

    void setupHASS();

    void printCommandResult(Nuki::CmdResult result);

    NukiOpener::LockAction lockActionToEnum(const char* str); // char array at least 14 charactersz

    std::string _deviceName;
    NukiDeviceId* _deviceId = nullptr;
    NukiOpener::NukiOpener _nukiOpener;
    BleScanner::Scanner* _bleScanner = nullptr;
    NetworkOpener* _network = nullptr;
    Gpio* _gpio = nullptr;
    Preferences* _preferences = nullptr;
    int _intervalLockstate = 0; // seconds
    int _intervalBattery = 0; // seconds
    int _intervalConfig = 60 * 60; // seconds
    int _intervalKeypad = 0; // seconds
    int _restartBeaconTimeout = 0; // seconds
    bool _publishAuthData = false;
    bool _clearAuthData = false;
    int _nrOfRetries = 0;
    int _retryDelay = 0;
    int _retryCount = 0;
    int _retryLockstateCount = 0;
    unsigned long _nextRetryTs = 0;
    std::vector<uint16_t> _keypadCodeIds;

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
    bool _hasKeypad = false;
    bool _keypadEnabled = false;
    uint _maxKeypadCodeCount = 0;
    bool _configRead = false;
    long _rssiPublishInterval = 0;
    unsigned long _nextLockStateUpdateTs = 0;
    unsigned long _nextBatteryReportTs = 0;
    unsigned long _nextConfigUpdateTs = 0;
    unsigned long _nextKeypadUpdateTs = 0;
    unsigned long _nextPairTs = 0;
    long _nextRssiTs = 0;
    unsigned long _lastRssi = 0;
    unsigned long _disableBleWatchdogTs = 0;
    std::string _firmwareVersion = "";
    std::string _hardwareVersion = "";
    NukiOpener::LockAction _nextLockAction = (NukiOpener::LockAction)0xff;
};
