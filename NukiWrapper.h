#pragma once

#include "NukiBle.h"
#include "Network.h"
#include "NukiConstants.h"
#include "NukiDataTypes.h"
#include "BleScanner.h"

class NukiWrapper : public Nuki::SmartlockEventHandler
{
public:
    NukiWrapper(const std::string& deviceName, uint32_t id, Network* network, Preferences* preferences);
    virtual ~NukiWrapper();

    void initialize();
    void update();

    void setPin(const uint16_t pin);

    const Nuki::KeyTurnerState& keyTurnerState();
    const bool isPaired();

    BleScanner::Scanner* bleScanner();

    void notify(Nuki::EventType eventType) override;

private:
    static bool onLockActionReceivedCallback(const char* value);
    static void onConfigUpdateReceivedCallback(const char* topic, const char* value);
    void onConfigUpdateReceived(const char* topic, const char* value);

    void updateKeyTurnerState();
    void updateBatteryState();
    void updateConfig();
    void updateAuthData();

    void readConfig();
    void readAdvancedConfig();

    Nuki::LockAction lockActionToEnum(const char* str); // char array at least 14 characters

    std::string _deviceName;
    Nuki::NukiBle _nukiBle;
    BleScanner::Scanner* _bleScanner;
    Network* _network;
    Preferences* _preferences;
    int _intervalLockstate = 0; // seconds
    int _intervalBattery = 0; // seconds
    int _intervalConfig = 60 * 60; // seconds
    bool _publishAuthData = false;
    bool _clearAuthData = false;

    Nuki::KeyTurnerState _lastKeyTurnerState;
    Nuki::KeyTurnerState _keyTurnerState;

    uint32_t _lastAuthId = 0xffff;

    Nuki::BatteryReport _batteryReport;
    Nuki::BatteryReport _lastBatteryReport;

    Nuki::Config _nukiConfig = {0};
    Nuki::AdvancedConfig _nukiAdvancedConfig = {0};
    bool _nukiConfigValid = false;
    bool _nukiAdvancedConfigValid = false;

    bool _paired = false;
    bool _statusUpdated = false;
    unsigned long _nextLockStateUpdateTs = 0;
    unsigned long _nextBatteryReportTs = 0;
    unsigned long _nextConfigUpdateTs = 0;
    unsigned long _nextPairTs = 0;
    Nuki::LockAction _nextLockAction = (Nuki::LockAction)0xff;
};
