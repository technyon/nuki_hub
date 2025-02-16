#pragma once

#include <Preferences.h>
#include <PsychicHttp.h>
#include "enums/NukiPinState.h"

#ifdef CONFIG_ESP_HTTPS_SERVER_ENABLE
#include <PsychicHttpsServer.h>
#endif
#include "esp_ota_ops.h"
#include "Config.h"

#ifndef NUKI_HUB_UPDATER
#include "NukiWrapper.h"
#include "NukiNetworkLock.h"
#include "NukiOpenerWrapper.h"
#include "Gpio.h"
#include "ImportExport.h"

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
#include "ImportExport.h"
#endif

extern TaskHandle_t networkTaskHandle;

class WebCfgServer
{
public:
    #ifndef NUKI_HUB_UPDATER
    WebCfgServer(NukiWrapper* nuki, NukiOpenerWrapper* nukiOpener, NukiNetwork* network, Gpio* gpio, Preferences* preferences, bool allowRestartToPortal, uint8_t partitionType, PsychicHttpServer* psychicServer, ImportExport* importExport);
    #else
    WebCfgServer(NukiNetwork* network, Preferences* preferences, bool allowRestartToPortal, uint8_t partitionType, PsychicHttpServer* psychicServer, ImportExport* importExport);
    #endif
    ~WebCfgServer() = default;

    void initialize();

private:
    #ifndef NUKI_HUB_UPDATER
    esp_err_t sendSettings(PsychicRequest *request, PsychicResponse* resp, bool adminKey = false);
    bool processArgs(PsychicRequest *request, PsychicResponse* resp, String& message);
    bool processImport(PsychicRequest *request, PsychicResponse* resp, String& message);
    void processGpioArgs(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t buildHtml(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t buildAccLvlHtml(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t buildCredHtml(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t buildImportExportHtml(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t buildNetworkConfigHtml(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t buildMqttConfigHtml(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t buildMqttSSLConfigHtml(PsychicRequest *request, PsychicResponse* resp, int type=0);
    esp_err_t buildHttpSSLConfigHtml(PsychicRequest *request, PsychicResponse* resp, int type=0);
    esp_err_t buildStatusHtml(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t buildAdvancedConfigHtml(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t buildNukiConfigHtml(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t buildGpioConfigHtml(PsychicRequest *request, PsychicResponse* resp);
    #ifndef CONFIG_IDF_TARGET_ESP32H2
    esp_err_t buildConfigureWifiHtml(PsychicRequest *request, PsychicResponse* resp);
    #endif
    esp_err_t buildInfoHtml(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t buildCustomNetworkConfigHtml(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t processUnpair(PsychicRequest *request, PsychicResponse* resp, bool opener);
    esp_err_t processUpdate(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t processFactoryReset(PsychicRequest *request, PsychicResponse* resp);
    void printTextarea(PsychicStreamResponse *response, const char *token, const char *description, const char *value, const size_t& maxLength, const bool& enabled = true, const bool& showLengthRestriction = false);
    void printDropDown(PsychicStreamResponse *response, const char *token, const char *description, const String preselectedValue, std::vector<std::pair<String, String>> options, const String className);
    void buildNavigationMenuEntry(PsychicStreamResponse *response, const char *title, const char *targetPath, const char* warningMessage = "");

    const std::vector<std::pair<String, String>> getNetworkDetectionOptions() const;
    const std::vector<std::pair<String, String>> getGpioOptions() const;
    const std::vector<std::pair<String, String>> getNetworkCustomPHYOptions() const;
    #if defined(CONFIG_IDF_TARGET_ESP32)
    const std::vector<std::pair<String, String>> getNetworkCustomCLKOptions() const;
    #endif
    #ifdef CONFIG_SOC_SPIRAM_SUPPORTED
    void createSSLCertificate();
    #endif
    const String getPreselectionForGpio(const uint8_t& pin) const;
    const String pinStateToString(const NukiPinState& value) const;

    void printParameter(PsychicStreamResponse *response, const char* description, const char* value, const char *link = "", const char *id = "");

    NukiWrapper* _nuki = nullptr;
    NukiOpenerWrapper* _nukiOpener = nullptr;
    Gpio* _gpio = nullptr;
    bool _brokerConfigured = false;
    bool _rebootRequired = false;
    #endif

    std::vector<String> _ssidList;
    std::vector<int> _rssiList;
    String generateConfirmCode();
    String _confirmCode = "----";

    void saveSessions(int type = 0);
    void loadSessions(int type = 0);
    void clearSessions();
    esp_err_t logoutSession(PsychicRequest *request, PsychicResponse* resp);
    bool isAuthenticated(PsychicRequest *request, int type = 0);
    bool processLogin(PsychicRequest *request, PsychicResponse* resp);
    bool processTOTP(PsychicRequest *request, PsychicResponse* resp);
    bool processBypass(PsychicRequest *request, PsychicResponse* resp);
    int doAuthentication(PsychicRequest *request);
    esp_err_t buildCoredumpHtml(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t buildLoginHtml(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t buildBypassHtml(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t buildTOTPHtml(PsychicRequest *request, PsychicResponse* resp, int type);
    esp_err_t buildDuoHtml(PsychicRequest *request, PsychicResponse* resp, int type);
    esp_err_t buildDuoCheckHtml(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t buildSSIDListHtml(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t buildConfirmHtml(PsychicRequest *request, PsychicResponse* resp, const String &message, uint32_t redirectDelay = 5, bool redirect = false, String redirectTo = "/");
    esp_err_t buildOtaHtml(PsychicRequest *request, PsychicResponse* resp, bool debug = false);
    esp_err_t sendCss(PsychicRequest *request, PsychicResponse* resp);
    esp_err_t sendFavicon(PsychicRequest *request, PsychicResponse* resp);
    void createSsidList();
    void buildHtmlHeader(PsychicStreamResponse *response, String additionalHeader = "");
    void waitAndProcess(const bool blocking, const uint32_t duration);
    esp_err_t handleOtaUpload(PsychicRequest *request, const String& filename, uint64_t index, uint8_t *data, size_t len, bool final);
    void printCheckBox(PsychicStreamResponse *response, const char* token, const char* description, const bool value, const char* htmlClass);
    #ifndef CONFIG_IDF_TARGET_ESP32H2
    esp_err_t buildWifiConnectHtml(PsychicRequest *request, PsychicResponse* resp);
    bool processWiFi(PsychicRequest *request, PsychicResponse* resp, String& message);

    #endif
    void printInputField(PsychicStreamResponse *response, const char* token, const char* description, const char* value, const size_t& maxLength, const char* args, const bool& isPassword = false, const bool& showLengthRestriction = false);
    void printInputField(PsychicStreamResponse *response, const char* token, const char* description, const int value, size_t maxLength, const char* args);

    PsychicHttpServer* _psychicServer = nullptr;
    NukiNetwork* _network = nullptr;
    Preferences* _preferences = nullptr;
    ImportExport* _importExport;

    char _credUser[31] = {0};
    char _credPassword[31] = {0};
    bool _allowRestartToPortal = false;
    bool _isSSL = false;
    uint8_t _partitionType = 0;
    size_t _otaContentLen = 0;
    String _hostname;
    JsonDocument _httpSessions;
    bool _duoEnabled = false;
    bool _bypassGPIO = false;
    bool _newBypass = false;
    int _bypassGPIOHigh = -1;
    int _bypassGPIOLow = -1;
};
