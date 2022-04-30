#pragma once

#include <WiFiClient.h>
#include "NetworkDevice.h"
#include "../SpiffsCookie.h"

class WifiDevice : public NetworkDevice
{
public:
    WifiDevice(const String& hostname);

    virtual void initialize();
    virtual void reconfigure();
    virtual bool reconnect();

    virtual void update();

    virtual bool isConnected();

    virtual PubSubClient *mqttClient();

private:
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;
    SpiffsCookie _cookie;
};
