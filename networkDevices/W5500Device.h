#pragma once

#include "NetworkDevice.h"
#include <Ethernet.h>

class W5500Device : public NetworkDevice
{
public:
    explicit W5500Device(const String& hostname);
    ~W5500Device();

    virtual void initialize();
    virtual void reconfigure();

    virtual bool isConnected();

    virtual PubSubClient *mqttClient();

private:
    void resetDevice();

    EthernetClient* _ethClient = nullptr;
    PubSubClient* _mqttClient = nullptr;
};