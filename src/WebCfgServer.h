#pragma once

#include <Preferences.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include "esp_ota_ops.h"
#include "Config.h"

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
    WebCfgServer(NukiWrapper* nuki, NukiOpenerWrapper* nukiOpener, NukiNetwork* network, Gpio* gpio, Preferences* preferences, bool allowRestartToPortal, uint8_t partitionType, AsyncWebServer* asyncServer);
    #else
    WebCfgServer(NukiNetwork* network, Preferences* preferences, bool allowRestartToPortal, uint8_t partitionType, AsyncWebServer* asyncServer);    
    #endif
    ~WebCfgServer() = default;

    void initialize();

private:
    #ifndef NUKI_HUB_UPDATER
    void sendSettings(AsyncWebServerRequest *request);
    bool processArgs(AsyncWebServerRequest *request, String& message);
    bool processImport(AsyncWebServerRequest *request, String& message);
    void processGpioArgs(AsyncWebServerRequest *request);
    void buildHtml(AsyncWebServerRequest *request);
    void buildAccLvlHtml(AsyncWebServerRequest *request);
    void buildCredHtml(AsyncWebServerRequest *request);
    void buildImportExportHtml(AsyncWebServerRequest *request);
    void buildMqttConfigHtml(AsyncWebServerRequest *request);
    void buildStatusHtml(AsyncWebServerRequest *request);
    void buildAdvancedConfigHtml(AsyncWebServerRequest *request);
    void buildNukiConfigHtml(AsyncWebServerRequest *request);
    void buildGpioConfigHtml(AsyncWebServerRequest *request);
    #ifndef CONFIG_IDF_TARGET_ESP32H2
    void buildConfigureWifiHtml(AsyncWebServerRequest *request);
    #endif
    void buildInfoHtml(AsyncWebServerRequest *request);
    void buildCustomNetworkConfigHtml(AsyncWebServerRequest *request);
    void processUnpair(AsyncWebServerRequest *request, bool opener);
    void processUpdate(AsyncWebServerRequest *request);
    void processFactoryReset(AsyncWebServerRequest *request);
    void printInputField(const char* token, const char* description, const char* value, const size_t& maxLength, const char* args, const bool& isPassword = false, const bool& showLengthRestriction = false);
    void printInputField(const char* token, const char* description, const int value, size_t maxLength, const char* args);
    void printCheckBox(const char* token, const char* description, const bool value, const char* htmlClass);
    void printTextarea(const char *token, const char *description, const char *value, const size_t& maxLength, const bool& enabled = true, const bool& showLengthRestriction = false);
    void printDropDown(const char *token, const char *description, const String preselectedValue, std::vector<std::pair<String, String>> options, const String className);
    void buildNavigationButton(const char* caption, const char* targetPath, const char* labelText = "");
    void buildNavigationMenuEntry(const char *title, const char *targetPath, const char* warningMessage = "");

    const std::vector<std::pair<String, String>> getNetworkDetectionOptions() const;
    const std::vector<std::pair<String, String>> getGpioOptions() const;
    const std::vector<std::pair<String, String>> getNetworkCustomPHYOptions() const;
    #if defined(CONFIG_IDF_TARGET_ESP32)
    const std::vector<std::pair<String, String>> getNetworkCustomCLKOptions() const;
    #endif

    String getPreselectionForGpio(const uint8_t& pin);
    String pinStateToString(uint8_t value);

    void printParameter(const char* description, const char* value, const char *link = "", const char *id = "");
    
    NukiWrapper* _nuki = nullptr;
    NukiOpenerWrapper* _nukiOpener = nullptr;
    Gpio* _gpio = nullptr;
    bool _pinsConfigured = false;
    bool _brokerConfigured = false;
    bool _rebootRequired = false;
    #endif

    String _response;
    String generateConfirmCode();
    String _confirmCode = "----";
    void buildConfirmHtml(AsyncWebServerRequest *request, const String &message, uint32_t redirectDelay = 5, bool redirect = false);    
    void buildOtaHtml(AsyncWebServerRequest *request, bool debug = false);
    void buildOtaCompletedHtml(AsyncWebServerRequest *request);
    void sendCss(AsyncWebServerRequest *request);
    void sendFavicon(AsyncWebServerRequest *request);
    void buildHtmlHeader(String additionalHeader = "");
    void waitAndProcess(const bool blocking, const uint32_t duration);
    void handleOtaUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    void printProgress(size_t prg, size_t sz);
    void sendResponse(AsyncWebServerRequest *request);
    
    AsyncWebServer* _asyncServer = nullptr;
    NukiNetwork* _network = nullptr;
    Preferences* _preferences = nullptr;

    bool _hasCredentials = false;
    char _credUser[31] = {0};
    char _credPassword[31] = {0};
    bool _allowRestartToPortal = false;
    uint8_t _partitionType = 0;
    uint32_t _transferredSize = 0;
    int64_t _otaStartTs = 0;
    size_t _otaContentLen = 0;
    String _hostname;
    bool _enabled = true;
};
