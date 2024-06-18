#pragma once

#include <WiFiClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include "NetworkDevice.h"
#include "WiFiManager.h"
#ifndef NUKI_HUB_UPDATER
#include "espMqttClient.h"
#endif
#include "IPConfiguration.h"

class WifiDevice : public NetworkDevice
{
public:
    WifiDevice(const String& hostname, Preferences* preferences, const IPConfiguration* ipConfiguration);

    const String deviceName() const override;

    virtual void initialize();
    virtual void reconfigure();
    virtual ReconnectStatus reconnect();
    bool supportsEncryption() override;

    virtual bool isConnected();

    int8_t signalStrength() override;
    
    String localIP() override;
    String BSSIDstr() override;

private:
    static void clearRtcInitVar(WiFiManager*);

    void onDisconnected();

    WiFiManager _wm;
    Preferences* _preferences = nullptr;

    bool _restartOnDisconnect = false;
    bool _startAp = false;
    char* _path;

    #ifndef NUKI_HUB_UPDATER
    char _ca[TLS_CA_MAX_SIZE] = {0};
    char _cert[TLS_CERT_MAX_SIZE] = {0};
    char _key[TLS_KEY_MAX_SIZE] = {0};
    #endif
};
