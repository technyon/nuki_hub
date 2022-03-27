#pragma once

#include <WiFiServer.h>
#include <Preferences.h>
#include "Nuki.h"
#include "Network.h"

enum class TokenType
{
    None,
    MqttServer,
    MqttPort,
    QueryIntervalLockstate,
    QueryIntervalBattery,
};

class WebCfgServer
{
public:
    WebCfgServer(Nuki* nuki, Network* network, Preferences* preferences);
    ~WebCfgServer() = default;

    void initialize();
    void update();


private:
    void serveHtml(WiFiClient& client);

    TokenType getParameterType(char*& token);

    WiFiServer _wifiServer;
    Nuki* _nuki;
    Network* _network;
    Preferences* _preferences;

    bool _enabled = true;
};