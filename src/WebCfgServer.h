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
    bool processImport(String& message);
    void processGpioArgs();
    void buildHtml();
    void buildAccLvlHtml();
    void buildCredHtml();
    void buildImportExportHtml();
    void buildMqttConfigHtml();
    void buildStatusHtml();
    void buildAdvancedConfigHtml();
    void buildNukiConfigHtml();
    void buildGpioConfigHtml();
    void buildConfigureWifiHtml();
    void buildInfoHtml();
    void processUnpair(bool opener);
    void processUpdate();
    void processFactoryReset();
    void printInputField(const char* token, const char* description, const char* value, const size_t& maxLength, const char* id, const bool& isPassword = false, const bool& showLengthRestriction = false);
    void printInputField(const char* token, const char* description, const int value, size_t maxLength, const char* id);
    void printCheckBox(const char* token, const char* description, const bool value, const char* htmlClass);
    void printTextarea(const char *token, const char *description, const char *value, const size_t& maxLength, const bool& enabled = true, const bool& showLengthRestriction = false);
    void printDropDown(const char *token, const char *description, const String preselectedValue, std::vector<std::pair<String, String>> options);
    void buildNavigationButton(const char* caption, const char* targetPath, const char* labelText = "");
    void buildNavigationMenuEntry(const char *title, const char *targetPath, const char* warningMessage = "");

    const std::vector<std::pair<String, String>> getNetworkDetectionOptions() const;
    const std::vector<std::pair<String, String>> getGpioOptions() const;
    String getPreselectionForGpio(const uint8_t& pin);
    String pinStateToString(uint8_t value);

    void printParameter(const char* description, const char* value, const char *link = "", const char *id = "");
    
    NukiWrapper* _nuki = nullptr;
    NukiOpenerWrapper* _nukiOpener = nullptr;
    Gpio* _gpio = nullptr;
    bool _pinsConfigured = false;
    bool _brokerConfigured = false;
    #endif

    String generateConfirmCode();
    String _confirmCode = "----";
    void buildConfirmHtml(const String &message, uint32_t redirectDelay = 5, bool redirect = false);    
    void buildOtaHtml(bool errored, bool debug = false);
    void buildOtaCompletedHtml();
    void sendCss();
    void sendFavicon();
    void buildHtmlHeader(String additionalHeader = "");
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
    int64_t _otaStartTs = 0;
    String _hostname;
    String _response;
    bool _enabled = true;
};
