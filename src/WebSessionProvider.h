#pragma once
#include <Preferences.h>
#include "ImportExport.h"

#include "ArduinoJson/Document/JsonDocument.hpp"

class WebSessionProvider
{
public:
    WebSessionProvider(Preferences* preferences, ImportExport* importExport);

    ArduinoJson::JsonDocument& httpSessions();

    void saveSessions(int type = 0);
    void loadSessions(int type = 0);
    void clearSessions();

private:
    Preferences* _preferences;
    ImportExport* _importExport;
    ArduinoJson::JsonDocument _httpSessions;
};
