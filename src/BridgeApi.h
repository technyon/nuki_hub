#pragma once

#include <Preferences.h>
#include <PsychicHttp.h>
#include <ArduinoJson.h>
#include "NukiWrapper.h"
#include "NukiOpenerWrapper.h"
#include "NukiNetwork.h"
#include "NukiNetworkLock.h"
#include "NukiNetworkOpener.h"
#include "ImportExport.h"

class BridgeApi
{
public:
    BridgeApi(NukiWrapper* nuki, NukiOpenerWrapper* nukiOpener, NukiNetwork* network, NukiNetworkLock* networkLock, NukiNetworkOpener* networkOpener, Preferences* preferences, ImportExport* importExport);

    void initialize(PsychicHttpServer* server);
    void update();

private:
    bool checkToken(PsychicRequest* request);
    esp_err_t sendJson(PsychicResponse* resp, JsonDocument& json, int code);
    esp_err_t sendUnauthorized(PsychicResponse* resp);
    int deviceType(PsychicRequest* request);
    uint8_t deviceStateBattery(int deviceType);
    void deviceState(int deviceType, JsonObject json);
    LockActionResult lockAction(int deviceType, int action);
    void loadCallbacks(JsonDocument& json);
    void saveCallbacks(JsonDocument& json);
    void sendCallback(const char* url, const String& payload);
    String timestamp(time_t time);
    void initializeRestApi(PsychicHttpServer* server);
    esp_err_t sendEntries(PsychicRequest* request, PsychicResponse* resp, int kind);
    esp_err_t runCommand(PsychicRequest* request, PsychicResponse* resp, int kind, const char* action);
    void commandJson(PsychicRequest* request, int kind, const char* action, String& out);
    uint32_t resultCounter(int deviceType);
    const String& lastResult(int deviceType);
    void dispatchCommand(int deviceType, int kind, const char* json);

    NukiWrapper* _nuki = nullptr;
    NukiOpenerWrapper* _nukiOpener = nullptr;
    NukiNetwork* _network = nullptr;
    NukiNetworkLock* _networkLock = nullptr;
    NukiNetworkOpener* _networkOpener = nullptr;
    Preferences* _preferences = nullptr;
    ImportExport* _importExport = nullptr;

    String _lastLockCallback = "";
    String _lastOpenerCallback = "";
    int64_t _nextCallbackCheckTs = 0;
};
