#pragma once
#include "IPConfiguration.h"

enum class ReconnectStatus
{
    Failure = 0,
    Success = 1,
    CriticalFailure = 2
};

class NetworkDevice
{
public:
    explicit NetworkDevice(const String& hostname, const IPConfiguration* ipConfiguration)
    : _hostname(hostname),
      _ipConfiguration(ipConfiguration)
    {}

    virtual const String deviceName() const = 0;

    virtual void initialize() = 0;
    virtual ReconnectStatus reconnect(bool force = false) = 0;
    virtual void reconfigure() = 0;
    virtual void update();

    virtual bool isConnected() = 0;
    virtual int8_t signalStrength() = 0;

    virtual String localIP() = 0;
    virtual String BSSIDstr() = 0;
protected:    
    const String _hostname;
    const IPConfiguration* _ipConfiguration = nullptr;
};