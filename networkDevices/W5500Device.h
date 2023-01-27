#pragma once

#include "NetworkDevice.h"
#include "espMqttClient.h"
#include <Ethernet.h>
#include <Preferences.h>

class W5500Device : public NetworkDevice
{
public:
    explicit W5500Device(const String& hostname, Preferences* _preferences);
    ~W5500Device();

    virtual void initialize();
    virtual ReconnectStatus reconnect();
    virtual void reconfigure();
    virtual void printError();

    virtual void update();

    virtual bool isConnected();

    int8_t signalStrength() override;

    virtual MqttClientSetup* mqttClient();

private:
    void resetDevice();
    void initializeMacAddress(byte* mac);

    EthernetClient* _ethClient = nullptr;
    MqttClientSetup* _mqttClient = nullptr;
    Preferences* _preferences = nullptr;

    int _maintainResult = 0;
    bool _hasDHCPAddress = false;
    char* _path;

    byte _mac[6];
};