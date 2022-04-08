#pragma once

#include <Preferences.h>
#include <WebServer.h>
#include "NukiWrapper.h"
#include "Network.h"

enum class TokenType
{
    None,
    MqttServer,
    MqttPort,
    MqttUser,
    MqttPass,
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
    bool processArgs();
    void buildHtml(String& response);
    void buildCredHtml(String& response);
    void buildConfirmHtml(String& response);
    void printInputField(String& response, const char* token, const char* description, const char* value, const size_t maxLength, const bool isPassword = false);
    void printInputField(String& response, const char* token, const char* description, const int value, size_t maxLength);

    void printParameter(String& response, const char* description, const char* value);

    WebServer server;
    NukiWrapper* _nuki;
    Network* _network;
    Preferences* _preferences;

    bool _hasCredentials = false;
    char _credUser[20] = {0};
    char _credPassword[20] = {0};

    bool _enabled = true;
};