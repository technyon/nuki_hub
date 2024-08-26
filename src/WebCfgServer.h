#pragma once

#include <Preferences.h>
#include <PsychicHttp.h>
#include <PsychicHttpsServer.h>
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
    WebCfgServer(NukiWrapper* nuki, NukiOpenerWrapper* nukiOpener, NukiNetwork* network, Gpio* gpio, Preferences* preferences, bool allowRestartToPortal, uint8_t partitionType, PsychicHttpServer* psychicServer);
    #else
    WebCfgServer(NukiNetwork* network, Preferences* preferences, bool allowRestartToPortal, uint8_t partitionType, PsychicHttpServer* psychicServer);
    #endif
    ~WebCfgServer() = default;

    void initialize();

private:
    #ifndef NUKI_HUB_UPDATER
    esp_err_t sendSettings(PsychicRequest *request);
    bool processArgs(PsychicRequest *request, String& message);
    bool processImport(PsychicRequest *request, String& message);
    void processGpioArgs(PsychicRequest *request);
    esp_err_t buildHtml(PsychicRequest *request);
    esp_err_t buildAccLvlHtml(PsychicRequest *request);
    esp_err_t buildCredHtml(PsychicRequest *request);
    esp_err_t buildImportExportHtml(PsychicRequest *request);
    esp_err_t buildMqttConfigHtml(PsychicRequest *request);
    esp_err_t buildStatusHtml(PsychicRequest *request);
    esp_err_t buildAdvancedConfigHtml(PsychicRequest *request);
    esp_err_t buildNukiConfigHtml(PsychicRequest *request);
    esp_err_t buildGpioConfigHtml(PsychicRequest *request);
    #ifndef CONFIG_IDF_TARGET_ESP32H2
    esp_err_t buildConfigureWifiHtml(PsychicRequest *request);
    #endif
    esp_err_t buildInfoHtml(PsychicRequest *request);
    esp_err_t buildCustomNetworkConfigHtml(PsychicRequest *request);
    esp_err_t processUnpair(PsychicRequest *request, bool opener);
    esp_err_t processUpdate(PsychicRequest *request);
    esp_err_t processFactoryReset(PsychicRequest *request);
    void printInputField(PsychicStreamResponse *response, const char* token, const char* description, const char* value, const size_t& maxLength, const char* args, const bool& isPassword = false, const bool& showLengthRestriction = false);
    void printInputField(PsychicStreamResponse *response, const char* token, const char* description, const int value, size_t maxLength, const char* args);
    void printCheckBox(PsychicStreamResponse *response, const char* token, const char* description, const bool value, const char* htmlClass);
    void printTextarea(PsychicStreamResponse *response, const char *token, const char *description, const char *value, const size_t& maxLength, const bool& enabled = true, const bool& showLengthRestriction = false);
    void printDropDown(PsychicStreamResponse *response, const char *token, const char *description, const String preselectedValue, std::vector<std::pair<String, String>> options, const String className);
    void buildNavigationButton(PsychicStreamResponse *response, const char* caption, const char* targetPath, const char* labelText = "");
    void buildNavigationMenuEntry(PsychicStreamResponse *response, const char *title, const char *targetPath, const char* warningMessage = "");

    const std::vector<std::pair<String, String>> getNetworkDetectionOptions() const;
    const std::vector<std::pair<String, String>> getGpioOptions() const;
    const std::vector<std::pair<String, String>> getNetworkCustomPHYOptions() const;
    #if defined(CONFIG_IDF_TARGET_ESP32)
    const std::vector<std::pair<String, String>> getNetworkCustomCLKOptions() const;
    #endif

    String getPreselectionForGpio(const uint8_t& pin);
    String pinStateToString(uint8_t value);

    void printParameter(PsychicStreamResponse *response, const char* description, const char* value, const char *link = "", const char *id = "");

    NukiWrapper* _nuki = nullptr;
    NukiOpenerWrapper* _nukiOpener = nullptr;
    Gpio* _gpio = nullptr;
    bool _pinsConfigured = false;
    bool _brokerConfigured = false;
    bool _rebootRequired = false;
    #endif

    String generateConfirmCode();
    String _confirmCode = "----";
    esp_err_t buildConfirmHtml(PsychicRequest *request, const String &message, uint32_t redirectDelay = 5, bool redirect = false);
    esp_err_t buildOtaHtml(PsychicRequest *request, bool debug = false);
    esp_err_t buildOtaCompletedHtml(PsychicRequest *request);
    esp_err_t sendCss(PsychicRequest *request);
    esp_err_t sendFavicon(PsychicRequest *request);
    void buildHtmlHeader(PsychicStreamResponse *response, String additionalHeader = "");
    void waitAndProcess(const bool blocking, const uint32_t duration);
    void handleOtaUpload(PsychicRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    void printProgress(size_t prg, size_t sz);

    PsychicHttpServer* _psychicServer = nullptr;
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
