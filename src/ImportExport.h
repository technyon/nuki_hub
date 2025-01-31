#pragma once

#include <Preferences.h>
#include "ArduinoJson.h"
#include <PsychicHttp.h>

class ImportExport
{
public:
    explicit ImportExport(Preferences* preferences);
    void exportHttpsJson(JsonDocument &json);
    void exportMqttsJson(JsonDocument &json);
    void exportNukiHubJson(JsonDocument &json, bool redacted = false, bool pairing = false, bool nuki = false, bool nukiOpener = false);
    JsonDocument importJson(JsonDocument &doc);
    int checkDuoAuth(PsychicRequest *request);
    int checkDuoApprove();
    bool startDuoAuth(char* pushType = (char*)"");
    bool getDuoEnabled();
    bool getBypassGPIOEnabled();
    int getBypassGPIOHigh();
    int getBypassGPIOLow();
    void readSettings();
    void setDuoCheckIP(String duoCheckIP);
    void setDuoCheckId(String duoCheckId);
    JsonDocument _duoSessions;
    JsonDocument _sessionsOpts;
private:
    void saveSessions();
    Preferences* _preferences;
    struct tm timeinfo;
    bool _duoActiveRequest;
    bool _duoEnabled = false;
    bool _bypassGPIO = false;
    int _bypassGPIOHigh = -1;
    int _bypassGPIOLow = -1;
    int64_t _duoRequestTS = 0;
    String _duoTransactionId;
    String _duoHost;
    String _duoSkey;
    String _duoIkey;
    String _duoUser;
    String _duoCheckId;
    String _duoCheckIP;
};

