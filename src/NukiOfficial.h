#pragma once

#include <cstdint>
#include <vector>
#include "../lib/nuki_ble/src/NukiLockConstants.h"

class NukiOfficial
{
public:
    void setUid(const uint32_t& uid);

    const char* GetMqttPath();

    void buildMqttPath(const char* path, char* outPath);
    bool comparePrefixedPath(const char *fullPath, const char *subPath);

    NukiLock::LockAction _offCommand = (NukiLock::LockAction)0xff;

    std::vector<char*> offTopics;
    int64_t offCommandExecutedTs = 0;

    //uint8_t _offMode = 0;
    uint8_t offState = 0;
    bool offCritical = false;
    uint8_t offChargeState = 100;
    bool offCharging = false;
    bool offKeypadCritical = false;
    uint8_t offDoorsensorState = 0;
    bool offDoorsensorCritical = false;
    bool offConnected = false;
    uint8_t offCommandResponse = 0;
    char* offLockActionEvent;
    uint8_t offLockAction = 0;
    uint8_t offTrigger = 0;
    uint32_t offAuthId = 0;
    uint32_t offCodeId = 0;
    uint8_t offContext = 0;
    bool offEnabled = false;

private:
    char mqttPath[181] = {0};
};

