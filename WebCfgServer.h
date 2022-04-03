#pragma once

#include <WiFiServer.h>
#include <Preferences.h>
#include "NukiWrapper.h"
#include "Network.h"

enum class TokenType
{
    None,
    MqttServer,
    MqttPort,
    MqttPath,
    QueryIntervalLockstate,
    QueryIntervalBattery,
};

class WebCfgServer
{
public:
    WebCfgServer(NukiWrapper* nuki, Network* network, Preferences* preferences);
    ~WebCfgServer() = default;

    void initialize();
    void update();


private:
    void serveHtml(WiFiClient& client);
    void printInputField(WiFiClient& client, const char* token, const char* description, const char* value, size_t maxLength);
    void printInputField(WiFiClient& client, const char* token, const char* description, const int value, size_t maxLength);

    TokenType getParameterType(char*& token);

    WiFiServer _wifiServer;
    NukiWrapper* _nuki;
    Network* _network;
    Preferences* _preferences;

    bool _enabled = true;
};