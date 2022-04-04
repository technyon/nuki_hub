#include "WebCfgServer.h"
#include <WiFiClient.h>
#include "PreferencesKeys.h"

WebCfgServer::WebCfgServer(NukiWrapper* nuki, Network* network, Preferences* preferences)
: server(80),
  _nuki(nuki),
  _network(network),
  _preferences(preferences)
{
    String str = _preferences->getString(preference_cred_user);

    if(str.length() > 0)
    {
        _hasCredentials = true;
        const char *user = str.c_str();
        memcpy(&_credUser, user, str.length());

        str = _preferences->getString(preference_cred_password);
        const char *pass = str.c_str();
        memcpy(&_credPassword, pass, str.length());

//        Serial.print("##### user: "); Serial.println(_credUser);
//        Serial.print("##### pass: "); Serial.println(_credPassword);
    }
}


void WebCfgServer::initialize()
{
    server.on("/", [&]() {
        if (_hasCredentials && !server.authenticate(_credUser, _credPassword)) {
            return server.requestAuthentication();
        }
        String response = "";
        buildHtml(response);
        server.send(200, "text/html", response);
    });
    server.on("/cred", [&]() {
        if (_hasCredentials && !server.authenticate(_credUser, _credPassword)) {
            return server.requestAuthentication();
        }
        String response = "";
        buildCredHtml(response);
        server.send(200, "text/html", response);
    });
    server.on("/method=get", [&]() {
        if (_hasCredentials && !server.authenticate(_credUser, _credPassword)) {
            return server.requestAuthentication();
        }
        bool configChanged = processArgs();
        if(configChanged)
        {
            String response = "";
            buildConfirmHtml(response);
            server.send(200, "text/html", response);
            Serial.println(F("Restarting"));
            unsigned long timeout = millis() + 1000;
            while(millis() < timeout)
            {
                server.handleClient();
                delay(10);
            }
            ESP.restart();
        }
    });

    server.begin();
}

bool WebCfgServer::processArgs()
{
    bool configChanged = false;
    bool clearMqttCredentials = false;
    bool clearCredentials = false;

    int count = server.args();
    for(int index = 0; index < count; index++)
    {
        String key = server.argName(index);
        String value = server.arg(index);

//        Serial.print(key);
//        Serial.print(" = ");
//        Serial.println(value);

        if(key == "MQTTSERVER")
        {
            _preferences->putString(preference_mqtt_broker, value);
            configChanged = true;
        }
        else if(key == "MQTTPORT")
        {
            _preferences->putInt(preference_mqtt_broker_port, value.toInt());
            configChanged = true;
        }
        else if(key == "MQTTUSER")
        {
            if(value == "#")
            {
                clearMqttCredentials = true;
            }
            else
            {
                _preferences->putString(preference_mqtt_user, value);
                configChanged = true;
            }
        }
        else if(key == "MQTTPASS")
        {
            if(value != "*")
            {
                _preferences->putString(preference_mqtt_password, value);
                configChanged = true;
            }
        }
        else if(key == "MQTTPATH")
        {
            _preferences->putString(preference_mqtt_path, value);
            configChanged = true;
        }
        else if(key == "LSTINT")
        {
            _preferences->putInt(preference_query_interval_lockstate, value.toInt());
            configChanged = true;
        }
        else if(key == "BATINT")
        {
            _preferences->putInt(preference_query_interval_battery, value.toInt());
            configChanged = true;
        }
        else if(key == "CREDUSER")
        {
            if(value == "#")
            {
                clearCredentials = true;
            }
            else
            {
                _preferences->putString(preference_cred_user, value);
                configChanged = true;
            }
        }
        else if(key == "CREDPASS")
        {
            _preferences->putString(preference_cred_password, value);
            configChanged = true;
        }
    }

    if(clearMqttCredentials)
    {
        _preferences->putString(preference_mqtt_user, "");
        _preferences->putString(preference_mqtt_password, "");
        configChanged = true;
    }

    if(clearCredentials)
    {
        _preferences->putString(preference_cred_user, "");
        _preferences->putString(preference_cred_password, "");
        configChanged = true;
    }

    if(configChanged)
    {
        _enabled = false;
        _preferences->end();
    }

    return configChanged;
}

void WebCfgServer::update()
{
    if(!_enabled) return;

    server.handleClient();
    vTaskDelay(200 / portTICK_PERIOD_MS);
}

void WebCfgServer::buildHtml(String& response)
{
    response.concat("<HTML>\n");
    response.concat("<HEAD>\n");
    response.concat("<TITLE>NUKI Hub</TITLE>\n");
    response.concat("</HEAD>\n");
    response.concat("<BODY>\n");
    response.concat("<br><br><h3>Info</h3>\n");


    response.concat("<table>");
    printParameter(response, "Paired", _nuki->isPaired() ? "&nbsp;Yes" : "&nbsp;No");
    printParameter(response, "MQTT Connected", _network->isMqttConnected() ? "&nbsp;Yes" : "&nbsp;No");
    response.concat("</table><br><br>");

    response.concat("<FORM ACTION=method=get >");

    response.concat("<h3>Configuration</h3>");
    response.concat("<table>");
    printInputField(response, "MQTTSERVER", "MQTT Broker", _preferences->getString(preference_mqtt_broker).c_str(), 100);
    printInputField(response, "MQTTPORT", "MQTT Broker port", _preferences->getInt(preference_mqtt_broker_port), 5);
    printInputField(response, "MQTTUSER", "MQTT User (# to clear)", _preferences->getString(preference_mqtt_user).c_str(), 30);
    printInputField(response, "MQTTPASS", "MQTT Password", "*", 30);
    printInputField(response, "MQTTPATH", "MQTT Path", _preferences->getString(preference_mqtt_path).c_str(), 180);
    printInputField(response, "LSTINT", "Query interval lock state (seconds)", _preferences->getInt(preference_query_interval_lockstate), 10);
    printInputField(response, "BATINT", "Query interval battery (seconds)", _preferences->getInt(preference_query_interval_battery), 10);
    response.concat("</table>");

    response.concat("<br><INPUT TYPE=SUBMIT NAME=\"submit\" VALUE=\"Save\">");

    response.concat("</FORM><BR><BR>");

    response.concat("<h3>Credentials</h3>");


    response.concat("<form method=\"get\" action=\"/cred\">");
    response.concat("<button type=\"submit\">Edit</button>");
    response.concat("</form>");



            //
    response.concat("</BODY>\n");
    response.concat("</HTML>\n");
}


void WebCfgServer::buildCredHtml(String &response)
{
    response.concat("<HTML>\n");
    response.concat("<HEAD>\n");
    response.concat("<TITLE>NUKI Hub</TITLE>\n");
    response.concat("</HEAD>\n");
    response.concat("<BODY>\n");

    response.concat("<FORM ACTION=method=get >");

    response.concat("<h3>Credentials</h3>");
    response.concat("<table>");
    printInputField(response, "CREDUSER", "User (# to clear)", _preferences->getString(preference_cred_user).c_str(), 20);
    printInputField(response, "CREDPASS", "Password", "*", 30);
    response.concat("</table>");

    response.concat("<br><INPUT TYPE=SUBMIT NAME=\"submit\" VALUE=\"Save\">");

    response.concat("</FORM>");

    response.concat("<BR>");
//
    response.concat("</BODY>\n");
    response.concat("</HTML>\n");
}

void WebCfgServer::buildConfirmHtml(String &response)
{
    response.concat("<HTML>\n");
    response.concat("<HEAD>\n");
    response.concat("<TITLE>NUKI Hub</TITLE>\n");
    response.concat("<meta http-equiv=\"Refresh\" content=\"5; url=/\" />");
    response.concat("\n</HEAD>\n");
    response.concat("<BODY>\n");

    response.concat("Configuration saved ... restarting.\n");

    response.concat("</BODY>\n");
    response.concat("</HTML>\n");
}

void WebCfgServer::printInputField(String& response,
                                   const char *token,
                                   const char *description,
                                   const char *value,
                                   size_t maxLength)
{
    char maxLengthStr[20];

    itoa(maxLength, maxLengthStr, 10);

    response.concat("<tr>");
    response.concat("<td>");
    response.concat(description);
    response.concat("</td>");
    response.concat("<td>");
    response.concat(" <INPUT TYPE=TEXT VALUE=\"");
    response.concat(value);
    response.concat("\" NAME=\"");
    response.concat(token);
    response.concat("\" SIZE=\"25\" MAXLENGTH=\"");
    response.concat(maxLengthStr);
    response.concat("\">");
    response.concat("</td>");
    response.concat("</tr>");
}

void WebCfgServer::printInputField(String& response,
                                   const char *token,
                                   const char *description,
                                   const int value,
                                   size_t maxLength)
{
    char valueStr[20];
    itoa(value, valueStr, 10);
    printInputField(response, token, description, valueStr, maxLength);
}

void WebCfgServer::printParameter(String& response, const char *description, const char *value)
{
    response.concat("<tr>");
    response.concat("<td>");
    response.concat(description);
    response.concat("</td>");
    response.concat("<td>");
    response.concat(value);
    response.concat("</td>");
    response.concat("</tr>");

}
