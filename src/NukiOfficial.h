#pragma once

#include <cstdint>
#include <vector>
#include "../lib/nuki_ble/src/NukiLockConstants.h"
#include "NukiPublisher.h"

class NukiOfficial
{
public:
    explicit NukiOfficial(Preferences* preferences);

    void setUid(const uint32_t& uid);

    const char* getMqttPath() const;
    const bool getStatusUpdated();

    const bool hasOffStateToPublish();
    const NukiLock::LockState getOffStateToPublish() const;

    const uint32_t getAuthId() const;
    const bool hasAuthId() const;
    void clearAuthId();

    void buildMqttPath(const char* path, char* outPath);
    bool comparePrefixedPath(const char *fullPath, const char *subPath);

    void onOfficialUpdateReceived(const char* topic, const char* value);

    const bool getOffConnected() const;
    const bool getOffEnabled() const;
    const uint8_t getOffDoorsensorState() const;
    const uint8_t getOffState() const;
    const uint8_t getOffLockAction() const;
    const uint8_t getOffTrigger() const;

    const int64_t getOffCommandExecutedTs() const;
    void setOffCommandExecutedTs(const int64_t& value);
    void clearOffCommandExecutedTs();

    std::vector<char*> offTopics;

private:
    char mqttPath[181] = {0};
    NukiPublisher* _publisher = nullptr;
    bool _statusUpdated = false;
    bool _hasOffStateToPublish = false;
    NukiLock::LockState _offStateToPublish = (NukiLock::LockState)0;
    uint32_t _authId = 0;
    bool _hasAuthId = false;
    bool _disableNonJSON = false;

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
//    uint8_t offContext = 0;
    bool offEnabled = false;
};

