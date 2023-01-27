#pragma once

#include "MqttClient.h"
#include "MqttClientSetup.h"

enum class ReconnectStatus
{
    Failure = 0,
    Success = 1,
    CriticalFailure = 2
};

class NetworkDevice
{
public:
    explicit NetworkDevice(const String& hostname)
    : _hostname(hostname)
    {}

    virtual MqttClientSetup* mqttClient() = 0;

    virtual void initialize() = 0;
    virtual ReconnectStatus reconnect() = 0;
    virtual void reconfigure() = 0;
    virtual void printError() = 0;

    virtual void update() = 0;

    virtual bool isConnected() = 0;
    virtual int8_t signalStrength() = 0;

protected:
    const uint16_t _mqttMaxBufferSize = 6144;
    const String _hostname;
};