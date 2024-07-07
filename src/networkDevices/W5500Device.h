#pragma once

#include "NetworkDevice.h"
#ifndef NUKI_HUB_UPDATER
#include "espMqttClient.h"
#include "espMqttClientW5500.h"
#endif
#include <Ethernet.h>
#include <Preferences.h>

enum class W5500Variant
{
    Generic = 2,
    M5StackAtomPoe = 3
};

class W5500Device : public NetworkDevice
{
public:
    explicit W5500Device(const String& hostname, Preferences* _preferences, const IPConfiguration* ipConfiguration, int variant);
    ~W5500Device();

    const String deviceName() const override;

    virtual void initialize();
    virtual ReconnectStatus reconnect();
    virtual void reconfigure();

    bool supportsEncryption() override;

    virtual void update() override;

    virtual bool isConnected();

    int8_t signalStrength() override;
    
    String localIP() override;
    String BSSIDstr() override;

private:
    void resetDevice();
    void initializeMacAddress(byte* mac);

    Preferences* _preferences = nullptr;

    int _maintainResult = 0;
    int _resetPin = -1;
    bool _hasDHCPAddress = false;
    char* _path;
    W5500Variant _variant;

    byte _mac[6];
};