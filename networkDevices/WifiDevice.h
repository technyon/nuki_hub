#pragma once

#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include "NetworkDevice.h"
#include "../SpiffsCookie.h"

#define TLS_CA_MAX_SIZE 1800
#define TLS_CERT_MAX_SIZE 1800
#define TLS_KEY_MAX_SIZE 1800

class WifiDevice : public NetworkDevice
{
public:
    WifiDevice(const String& hostname, Preferences* _preferences);

    virtual void initialize();
    virtual void reconfigure();
    virtual bool reconnect();

    virtual void update();

    virtual bool isConnected();

    virtual PubSubClient *mqttClient();

private:
    WiFiClient* _wifiClient = nullptr;
    WiFiClientSecure* _wifiClientSecure = nullptr;
    PubSubClient* _mqttClient = nullptr;
    SpiffsCookie _cookie;
    char _ca[TLS_CA_MAX_SIZE];
    char _cert[TLS_CERT_MAX_SIZE];
    char _key[TLS_KEY_MAX_SIZE];
};
