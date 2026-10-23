#pragma once

#include <Preferences.h>
#include <PsychicHttp.h>
#include <ArduinoJson.h>
#include "NukiWrapper.h"
#include "NukiOpenerWrapper.h"
#include "NukiNetwork.h"
#include "NukiNetworkOpener.h"

class BridgeApi
{
public:
    BridgeApi(NukiWrapper* nuki, NukiOpenerWrapper* nukiOpener, NukiNetwork* network, NukiNetworkOpener* networkOpener, Preferences* preferences);

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

    NukiWrapper* _nuki = nullptr;
    NukiOpenerWrapper* _nukiOpener = nullptr;
    NukiNetwork* _network = nullptr;
    NukiNetworkOpener* _networkOpener = nullptr;
    Preferences* _preferences = nullptr;

    String _lastLockCallback = "";
    String _lastOpenerCallback = "";
    int64_t _nextCallbackCheckTs = 0;
};
