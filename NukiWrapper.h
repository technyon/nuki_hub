#pragma once

#include "NetworkLock.h"
#include "NukiConstants.h"
#include "NukiDataTypes.h"
#include "BleScanner.h"
#include "NukiLock.h"

class NukiWrapper : public Nuki::SmartlockEventHandler
{
public:
    NukiWrapper(const std::string& deviceName, uint32_t id, BleScanner::Scanner* scanner, NetworkLock* network, Preferences* preferences);
    virtual ~NukiWrapper();

    void initialize(const bool& firstStart);
    void update();

    void lock();
    void unlock();
    void unlatch();

    bool isPinSet();
    void setPin(const uint16_t pin);
    void unpair();
    
    void disableHASS();

    const NukiLock::KeyTurnerState& keyTurnerState();
    const bool isPaired();
    const bool hasKeypad();
    const BLEAddress getBleAddress() const;

    void notify(Nuki::EventType eventType) override;

private:
    static bool onLockActionReceivedCallback(const char* value);
    static void onConfigUpdateReceivedCallback(const char* topic, const char* value);
    static void onKeypadCommandReceivedCallback(const char* command, const uint& id, const String& name, const String& code, const int& enabled);
    void onConfigUpdateReceived(const char* topic, const char* value);
    void onKeypadCommandReceived(const char* command, const uint& id, const String& name, const String& code, const int& enabled);

    void updateKeyTurnerState();
    void updateBatteryState();
    void updateConfig();
    void updateAuthData();
    void updateKeypad();
    void postponeBleWatchdog();

    void readConfig();
    void readAdvancedConfig();
    
    void setupHASS();
    bool hasDoorSensor();

    void printCommandResult(Nuki::CmdResult result);

    NukiLock::LockAction lockActionToEnum(const char* str); // char array at least 14 characters

    std::string _deviceName;
    NukiLock::NukiLock _nukiLock;
    BleScanner::Scanner* _bleScanner;
    NetworkLock* _network;
    Preferences* _preferences;
    int _intervalLockstate = 0; // seconds
    int _intervalBattery = 0; // seconds
    int _intervalConfig = 60 * 60; // seconds
    int _intervalKeypad = 0; // seconds
    int _restartBeaconTimeout = 0; // seconds
    bool _publishAuthData = false;
    bool _clearAuthData = false;
    std::vector<uint16_t> _keypadCodeIds;

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
    int _retryLockstateCount = 0;
    long _rssiPublishInterval = 0;
    unsigned long _nextRetryTs = 0;
    unsigned long _nextLockStateUpdateTs = 0;
    unsigned long _nextBatteryReportTs = 0;
    unsigned long _nextConfigUpdateTs = 0;
    unsigned long _nextKeypadUpdateTs = 0;
    unsigned long _nextRssiTs = 0;
    unsigned long _lastRssi = 0;
    unsigned long _disableBleWatchdogTs = 0;
    volatile NukiLock::LockAction _nextLockAction = (NukiLock::LockAction)0xff;
};
