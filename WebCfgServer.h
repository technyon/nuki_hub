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
    WebCfgServer(NukiWrapper* nuki, Network* network, EthServer* ethServer, Preferences* preferences, bool allowRestartToPortal);
    ~WebCfgServer() = default;

    void initialize();
    void update();


private:
    bool processArgs(String& message);
    void buildHtml(String& response);
    void buildCredHtml(String& response);
    void buildMqttConfigHtml(String& response);
    void buildConfirmHtml(String& response, const String &message, uint32_t redirectDelay = 5);
    void buildConfigureWifiHtml(String& response);
    void processUnpair();

    void buildHtmlHeader(String& response);
    void printInputField(String& response, const char* token, const char* description, const char* value, const size_t maxLength, const bool isPassword = false);
    void printInputField(String& response, const char* token, const char* description, const int value, size_t maxLength);
    void printCheckBox(String& response, const char* token, const char* description, const bool value);

    void printParameter(String& response, const char* description, const char* value);

    String generateConfirmCode();
    void waitAndProcess(const bool blocking, const uint32_t duration);

    WebServer _server;
    NukiWrapper* _nuki;
    Network* _network;
    Preferences* _preferences;

    bool _hasCredentials = false;
    char _credUser[20] = {0};
    char _credPassword[20] = {0};
    bool _allowRestartToPortal = false;

    String _confirmCode = "----";

    bool _enabled = true;
};