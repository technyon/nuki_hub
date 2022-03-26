#pragma once

#include "NukiBle.h"
#include "NukiConstants.h"
#include "Network.h"

class Nuki
{
public:
    Nuki(const std::string& name, uint32_t id, Network* network);

    void initialize();
    void update();

private:
    static void onLockActionReceived(const char* value);

    void updateKeyTurnerState();
    void updateBatteryState();

    void lockstateToString(const LockState state, char* str); // char array at least 14 characters
    LockAction lockActionToEnum(const char* str); // char array at least 14 characters

    NukiBle _nukiBle;
    Network* _network;

    KeyTurnerState _lastKeyTurnerState;
    KeyTurnerState _keyTurnerState;

    BatteryReport _batteryReport;
    BatteryReport _lastBatteryReport;

    bool _paired = false;
    unsigned long _nextLockStateUpdateTs = 0;
    unsigned long _nextBatteryReportTs = 0;
    LockAction _nextLockAction = (LockAction)0xff;
};
