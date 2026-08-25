#include "WebSessionProvider.h"
#include "ArduinoJson.h"
#include "SPIFFS.h"
#include "PreferencesKeys.h"
#include "FS.h"

WebSessionProvider::WebSessionProvider(Preferences* preferences, ImportExport* importExport)
: _preferences(preferences),
  _importExport(importExport)
{

}

ArduinoJson::JsonDocument& WebSessionProvider::httpSessions()
{
    return _httpSessions;
}

void WebSessionProvider::saveSessions(int type)
{
    if(_preferences->getBool(preference_update_time, false))
    {
        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
        }
        else
        {
            File file;

            if (type == 0)
            {
                file = SPIFFS.open("/sessions.json", "w");
                serializeJson(_httpSessions, file);
            }
            else if (type == 1)
            {
                file = SPIFFS.open("/duosessions.json", "w");
                serializeJson(_importExport->_duoSessions, file);
            }
            else if (type == 2)
            {
                file = SPIFFS.open("/totpsessions.json", "w");
                serializeJson(_importExport->_totpSessions, file);
            }
            file.close();
        }
    }
}

void WebSessionProvider::loadSessions(int type)
{
    if(_preferences->getBool(preference_update_time, false))
    {
        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
        }
        else
        {
            File file;

            if (type == 0)
            {
                file = SPIFFS.open("/sessions.json", "r");

                if (!file || file.isDirectory())
                {
                    Log->println("sessions.json not found");
                }
                else
                {
                    deserializeJson(_httpSessions, file);
                }
            }
            else if (type == 1)
            {
                file = SPIFFS.open("/duosessions.json", "r");

                if (!file || file.isDirectory())
                {
                    Log->println("duosessions.json not found");
                }
                else
                {
                    deserializeJson(_importExport->_duoSessions, file);
                }
            }
            else if (type == 2)
            {
                file = SPIFFS.open("/totpsessions.json", "r");

                if (!file || file.isDirectory())
                {
                    Log->println("totpsessions.json not found");
                }
                else
                {
                    deserializeJson(_importExport->_totpSessions, file);
                }
            }
            file.close();
        }
    }
}

void WebSessionProvider::clearSessions()
{
    if (!SPIFFS.begin(true))
    {
        Log->println("SPIFFS Mount Failed");
    }
    else
    {
        _httpSessions.clear();
        _importExport->_duoSessions.clear();
        _importExport->_totpSessions.clear();
        File file;
        file = SPIFFS.open("/sessions.json", "w");
        serializeJson(_httpSessions, file);
        file.close();
        file = SPIFFS.open("/duosessions.json", "w");
        serializeJson(_importExport->_duoSessions, file);
        file.close();
        file = SPIFFS.open("/totpsessions.json", "w");
        serializeJson(_importExport->_totpSessions, file);
        file.close();
    }
}

