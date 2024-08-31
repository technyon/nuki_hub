#pragma once

#include <cstdint>
#include <vector>
#include "../lib/nuki_ble/src/NukiLockConstants.h"

class NukiOfficial
{
public:
public:
    void (*_officialUpdateReceivedCallback)(const char* path, const char* value) = nullptr;
    NukiLock::LockAction _offCommand = (NukiLock::LockAction)0xff;

    std::vector<char*> _offTopics;
    char _offMqttPath[181] = {0};
    int64_t _offCommandExecutedTs = 0;

    //uint8_t _offMode = 0;
    uint8_t _offState = 0;
    bool _offCritical = false;
    uint8_t _offChargeState = 100;
    bool _offCharging = false;
    bool _offKeypadCritical = false;
    uint8_t _offDoorsensorState = 0;
    bool _offDoorsensorCritical = false;
    bool _offConnected = false;
    uint8_t _offCommandResponse = 0;
    char* _offLockActionEvent;
    uint8_t _offLockAction = 0;
    uint8_t _offTrigger = 0;
    uint32_t _offAuthId = 0;
    uint32_t _offCodeId = 0;
    uint8_t _offContext = 0;
    bool _offEnabled = false;
};

