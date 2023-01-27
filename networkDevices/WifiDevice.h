#pragma once

#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include "NetworkDevice.h"
#include "WiFiManager.h"

class WifiDevice : public NetworkDevice
{
public:
    WifiDevice(const String& hostname, Preferences* _preferences);

    virtual void initialize();
    virtual void reconfigure();
    virtual ReconnectStatus reconnect();
    virtual void printError();

    virtual void update();

    virtual bool isConnected();

    int8_t signalStrength() override;

    MqttClientSetup *mqttClient() override;

private:
    static void clearRtcInitVar(WiFiManager*);

    void onDisconnected();

    WiFiManager _wm;
    WiFiClient* _wifiClient = nullptr;
    WiFiClientSecure* _wifiClientSecure = nullptr;
    MqttClientSetup* _mqttClient = nullptr;
//    SpiffsCookie _cookie;
    bool _restartOnDisconnect = false;
    bool _startAp = false;
    char* _path;

    char _ca[TLS_CA_MAX_SIZE];
    char _cert[TLS_CERT_MAX_SIZE];
    char _key[TLS_KEY_MAX_SIZE];
};
