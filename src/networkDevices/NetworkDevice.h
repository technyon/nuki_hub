#pragma once
#include "IPConfiguration.h"

class NetworkDevice
{
public:
    explicit NetworkDevice(const String& hostname, const IPConfiguration* ipConfiguration)
    : _hostname(hostname),
      _ipConfiguration(ipConfiguration)
    {}

    virtual const String deviceName() const = 0;

    virtual void initialize() = 0;
    virtual void reconfigure() = 0;
    virtual void update();
    virtual void scan(bool passive = false, bool async = true) = 0;

    virtual bool isConnected() = 0;
    virtual bool isApOpen() = 0;
    virtual int8_t signalStrength() = 0;

    virtual String localIP() = 0;
    virtual String BSSIDstr() = 0;
protected:    
    const String _hostname;
    const IPConfiguration* _ipConfiguration = nullptr;
};