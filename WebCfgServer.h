#pragma once

#include <Preferences.h>
#include <WebServer.h>
#include "NukiWrapper.h"
#include "Network.h"
#include "NukiOpenerWrapper.h"
#include "Ota.h"

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
    WebCfgServer(NukiWrapper* nuki, NukiOpenerWrapper* nukiOpener, Network* network, EthServer* ethServer, Preferences* preferences, bool allowRestartToPortal);
    ~WebCfgServer() = default;

    void initialize();
    void update();


private:
    bool processArgs(String& message);
    void buildHtml(String& response);
    void buildCredHtml(String& response);
    void buildOtaHtml(String& response);
    void buildMqttConfigHtml(String& response);
    void buildNukiConfigHtml(String& response);
    void buildConfirmHtml(String& response, const String &message, uint32_t redirectDelay = 5);
    void buildConfigureWifiHtml(String& response);
    void processUnpair(bool opener);

    void buildHtmlHeader(String& response);
    void printInputField(String& response, const char* token, const char* description, const char* value, const size_t maxLength, const bool isPassword = false);
    void printInputField(String& response, const char* token, const char* description, const int value, size_t maxLength);
    void printCheckBox(String& response, const char* token, const char* description, const bool value);
    void printTextarea(String& response, const char *token, const char *description, const char *value, const size_t maxLength);

    void printParameter(String& response, const char* description, const char* value);

    String generateConfirmCode();
    void waitAndProcess(const bool blocking, const uint32_t duration);
    void handleOtaUpload();

    WebServer _server;
    NukiWrapper* _nuki;
    NukiOpenerWrapper* _nukiOpener;
    Network* _network;
    Preferences* _preferences;
    Ota _ota;

    bool _hasCredentials = false;
    char _credUser[20] = {0};
    char _credPassword[20] = {0};
    bool _allowRestartToPortal = false;
    uint32_t _transferredSize = 0;
    bool _otaStart = true;

    String _confirmCode = "----";

    bool _enabled = true;
};