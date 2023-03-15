#pragma once

#include <Preferences.h>
#include <WebServer.h>
#include "NukiWrapper.h"
#include "NetworkLock.h"
#include "NukiOpenerWrapper.h"
#include "Ota.h"

extern TaskHandle_t networkTaskHandle;
extern TaskHandle_t nukiTaskHandle;
extern TaskHandle_t presenceDetectionTaskHandle;

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
    void buildOtaHtml(String& response, bool errored);
    void buildOtaCompletedHtml(String& response);
    void buildMqttConfigHtml(String& response);
    void buildNukiConfigHtml(String& response);
    void buildConfirmHtml(String& response, const String &message, uint32_t redirectDelay = 5);
    void buildConfigureWifiHtml(String& response);
    void buildInfoHtml(String& response);
    void sendCss();
    void sendFavicon();
    void processUnpair(bool opener);

    void buildHtmlHeader(String& response);
    void printInputField(String& response, const char* token, const char* description, const char* value, const size_t& maxLength, const bool& isPassword = false, const bool& showLengthRestriction = false);
    void printInputField(String& response, const char* token, const char* description, const int value, size_t maxLength);
    void printCheckBox(String& response, const char* token, const char* description, const bool value);
    void printTextarea(String& response, const char *token, const char *description, const char *value, const size_t& maxLength, const bool& enabled = true, const bool& showLengthRestriction = false);
    void printDropDown(String &response, const char *token, const char *description, const String preselectedValue, std::vector<std::pair<String, String>> options);
    void buildNavigationButton(String& response, const char* caption, const char* targetPath, const char* labelText = "");

    const std::vector<std::pair<String, String>> getNetworkDetectionOptions() const;
    const std::vector<std::pair<String, String>> getNetworkGpioOptions() const;

    void printParameter(String& response, const char* description, const char* value, const char *link = "");

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
    char _credUser[31] = {0};
    char _credPassword[31] = {0};
    bool _allowRestartToPortal = false;
    bool _pinsConfigured = false;
    bool _brokerConfigured = false;
    uint32_t _transferredSize = 0;
    unsigned long _otaStartTs = 0;
    String _hostname;

    String _confirmCode = "----";

    bool _enabled = true;
};