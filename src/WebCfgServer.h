#pragma once

#include <Preferences.h>
#include <WebServer.h>
#include "Ota.h"

#ifndef NUKI_HUB_UPDATER
#include "NukiWrapper.h"
#include "NukiNetworkLock.h"
#include "NukiOpenerWrapper.h"
#include "Gpio.h"

extern TaskHandle_t nukiTaskHandle;

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

#else
#include "NukiNetwork.h"
#endif

extern TaskHandle_t networkTaskHandle;

class WebCfgServer
{
public:
    #ifndef NUKI_HUB_UPDATER
    WebCfgServer(NukiWrapper* nuki, NukiOpenerWrapper* nukiOpener, NukiNetwork* network, Gpio* gpio, EthServer* ethServer, Preferences* preferences, bool allowRestartToPortal, uint8_t partitionType);
    #else
    WebCfgServer(NukiNetwork* network, EthServer* ethServer, Preferences* preferences, bool allowRestartToPortal, uint8_t partitionType);    
    #endif
    ~WebCfgServer() = default;

    void initialize();
    void update();

private:
    #ifndef NUKI_HUB_UPDATER
    void sendSettings();
    bool processArgs(String& message);
    void processGpioArgs();
    void buildHtml(String& response);
    void buildAccLvlHtml(String& response);
    void buildCredHtml(String& response);
    void buildMqttConfigHtml(String& response);
    void buildStatusHtml(String& response);
    void buildAdvancedConfigHtml(String& response);
    void buildNukiConfigHtml(String& response);
    void buildGpioConfigHtml(String& response);
    void buildConfigureWifiHtml(String& response);
    void buildInfoHtml(String& response);
    void processUnpair(bool opener);
    void processFactoryReset();
    void printInputField(String& response, const char* token, const char* description, const char* value, const size_t& maxLength, const char* id, const bool& isPassword = false, const bool& showLengthRestriction = false);
    void printInputField(String& response, const char* token, const char* description, const int value, size_t maxLength, const char* id);
    void printCheckBox(String& response, const char* token, const char* description, const bool value, const char* htmlClass);
    void printTextarea(String& response, const char *token, const char *description, const char *value, const size_t& maxLength, const bool& enabled = true, const bool& showLengthRestriction = false);
    void printDropDown(String &response, const char *token, const char *description, const String preselectedValue, std::vector<std::pair<String, String>> options);
    void buildNavigationButton(String& response, const char* caption, const char* targetPath, const char* labelText = "");
    void buildNavigationMenuEntry(String &response, const char *title, const char *targetPath, const char* warningMessage = "");

    const std::vector<std::pair<String, String>> getNetworkDetectionOptions() const;
    const std::vector<std::pair<String, String>> getGpioOptions() const;
    String getPreselectionForGpio(const uint8_t& pin);
    String pinStateToString(uint8_t value);

    void printParameter(String& response, const char* description, const char* value, const char *link = "", const char *id = "");

    String generateConfirmCode();
    
    NukiWrapper* _nuki = nullptr;
    NukiOpenerWrapper* _nukiOpener = nullptr;
    Gpio* _gpio = nullptr;
    bool _pinsConfigured = false;
    bool _brokerConfigured = false;
    String _confirmCode = "----";
    #endif

    void buildConfirmHtml(String& response, const String &message, uint32_t redirectDelay = 5);    
    void buildOtaHtml(String& response, bool errored);
    void buildOtaCompletedHtml(String& response);
    void sendCss();
    void sendFavicon();
    void buildHtmlHeader(String& response, String additionalHeader = "");
    void waitAndProcess(const bool blocking, const uint32_t duration);
    void handleOtaUpload();

    WebServer _server;
    NukiNetwork* _network = nullptr;
    Preferences* _preferences = nullptr;
    Ota _ota;

    bool _hasCredentials = false;
    char _credUser[31] = {0};
    char _credPassword[31] = {0};
    bool _allowRestartToPortal = false;
    uint8_t _partitionType = 0;
    uint32_t _transferredSize = 0;
    unsigned long _otaStartTs = 0;
    String _hostname;
    bool _enabled = true;
};
