#include "WebCfgServer.h"
#include <WiFiClient.h>
#include "PreferencesKeys.h"

WebCfgServer::WebCfgServer(NukiWrapper* nuki, Network* network, Preferences* preferences)
: _wifiServer(80),
  _nuki(nuki),
  _network(network),
  _preferences(preferences)
{}


void WebCfgServer::initialize()
{
    _wifiServer.begin();
}

void WebCfgServer::update()
{
    if(!_enabled) return;

    bool configChanged = false;

    // Create a client connections
    WiFiClient client = _wifiServer.available();

    if (client)
    {
        int index = 0;
        char message[500];


        while (client.connected())
        {
            if (client.available())
            {
                char c = client.read();

                //read char by char HTTP request
                if (index < sizeof(message) - 1)
                {
                    message[index] = c;
                    index++;
                }
                message[index] = 0;

                //if HTTP request has ended
                if (c == '\n')
                {
                    serveHtml(client);
                    vTaskDelay( 5 / portTICK_PERIOD_MS);
                    //stopping client
                    client.stop();
                }
            }
        }

        char *token = strtok(message, "?=&");
        char *lastToken = NULL;

        bool configChanged = false;
        bool clearCredentials = false;
        while (token != NULL)
        {
            if(lastToken != NULL)
            {
                TokenType lastTokenType = getParameterType(lastToken);
                TokenType tokenType = getParameterType(token);

                if(lastTokenType == TokenType::MqttServer && tokenType == TokenType::None)
                {
                    _preferences->putString(preference_mqtt_broker, token);
                    configChanged = true;
                }
                else if(lastTokenType == TokenType::MqttPort && tokenType == TokenType::None)
                {
                    _preferences->putInt(preference_mqtt_broker_port, String(token).toInt());
                    configChanged = true;
                }
                else if(lastTokenType == TokenType::MqttUser && tokenType == TokenType::None)
                {
                    char* c = "%23";
                    if(strcmp(token, c) == 0)
                    {
                        clearCredentials = true;
                    }
                    else
                    {
                        _preferences->putString(preference_mqtt_user, token);
                        configChanged = true;
                    }
                }
                else if(lastTokenType == TokenType::MqttPass && tokenType == TokenType::None)
                {
                    char* c = "*";
                    if(strcmp(token, c) != 0)
                    {
                        _preferences->putString(preference_mqtt_password, token);
                        configChanged = true;
                    }
                }
                else if(lastTokenType == TokenType::MqttPath && tokenType == TokenType::None)
                {
                    _preferences->putString(preference_mqtt_path, token);
                    configChanged = true;
                }
                else if(lastTokenType == TokenType::QueryIntervalLockstate && tokenType == TokenType::None)
                {
                    _preferences->putInt(preference_query_interval_lockstate, String(token).toInt());
                    configChanged = true;
                }
                else if(lastTokenType == TokenType::QueryIntervalBattery && tokenType == TokenType::None)
                {
                    _preferences->putInt(preference_query_interval_battery, String(token).toInt());
                    configChanged = true;
                }
            }
            lastToken = token;
            token = strtok(NULL, "?=&");
        }

        if(clearCredentials)
        {
            _preferences->putString(preference_mqtt_user, "");
            _preferences->putString(preference_mqtt_password, "");
            configChanged = true;
        }

        if(configChanged)
        {
            _enabled = false;
            _preferences->end();
            Serial.println(F("Restarting"));
            vTaskDelay( 200 / portTICK_PERIOD_MS);
            ESP.restart();
        }
    }

    vTaskDelay(200 / portTICK_PERIOD_MS);

}

void WebCfgServer::serveHtml(WiFiClient &client)
{
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println();

    client.println("<HTML>");
    client.println("<HEAD>");
    client.println("<TITLE>NUKI Hub</TITLE>");
    client.println("</HEAD>");
    client.println("<BODY>");


    client.println("<br><br>");
    client.println("<h3>Info</h3>");
    client.println("<table>");
    printParameter(client, "Paired", _nuki->isPaired() ? "&nbsp;Yes" : "&nbsp;No");
    printParameter(client, "MQTT Connected", _network->isMqttConnected() ? "&nbsp;Yes" : "&nbsp;No");
    client.println("</table><br><br>");

    client.println("<FORM ACTION=method=get >");

    client.println("<h3>Configuration</h3>");
    client.println("<table>");
    printInputField(client, "MQTTSERVER", "MQTT Broker", _preferences->getString(preference_mqtt_broker).c_str(), 100);
    printInputField(client, "MQTTPORT", "MQTT Broker port", _preferences->getInt(preference_mqtt_broker_port), 5);
    printInputField(client, "MQTTUSER", "MQTT User (# to clear)", _preferences->getString(preference_mqtt_user).c_str(), 30);
    printInputField(client, "MQTTPASS", "MQTT Password", "*", 30);
    printInputField(client, "MQTTPATH", "MQTT Path", _preferences->getString(preference_mqtt_path).c_str(), 180);
    printInputField(client, "LSTINT", "Query interval lock state (seconds)", _preferences->getInt(preference_query_interval_lockstate), 10);
    printInputField(client, "BATINT", "Query interval battery (seconds)", _preferences->getInt(preference_query_interval_battery), 10);
    client.println("</table>");

    client.println("<br><INPUT TYPE=SUBMIT NAME=\"submit\" VALUE=\"Save\">");

    client.println("</FORM>");

    client.println("<BR>");

    client.println("</BODY>");
    client.println("</HTML>");
}

TokenType WebCfgServer::getParameterType(char *&token)
{
    if (strcmp(token, "MQTTSERVER") == 0)
    {
        return TokenType::MqttServer;
    }
    if (strcmp(token, "LSTINT") == 0)
    {
        return TokenType::QueryIntervalLockstate;
    }
    if (strcmp(token, "BATINT") == 0)
    {
        return TokenType::QueryIntervalBattery;
    }
    if (strcmp(token, "MQTTPORT") == 0)
    {
        return TokenType::MqttPort;
    }
    if (strcmp(token, "MQTTUSER") == 0)
    {
        return TokenType::MqttUser;
    }
    if (strcmp(token, "MQTTPASS") == 0)
    {
        return TokenType::MqttPass;
    }
    if (strcmp(token, "MQTTPATH") == 0)
    {
        return TokenType::MqttPath;
    }

    return TokenType::None;
}

void WebCfgServer::printInputField(WiFiClient &client,
                                   const char *token,
                                   const char *description,
                                   const char *value,
                                   size_t maxLength)
{
    char maxLengthStr[20];

    itoa(maxLength, maxLengthStr, 10);

    client.println("<tr>");
    client.print("<td>");
    client.print(description);
    client.print("</td>");
    client.print("<td>");
    client.print(" <INPUT TYPE=TEXT VALUE=\"");
    client.print(value);
    client.print("\" NAME=\"");
    client.print(token);
    client.print("\" SIZE=\"25\" MAXLENGTH=\"");
    client.print(maxLengthStr);
    client.println("\">");
    client.print("</td>");
    client.println("</tr>");
}

void WebCfgServer::printInputField(WiFiClient &client,
                                   const char *token,
                                   const char *description,
                                   const int value,
                                   size_t maxLength)
{
    char valueStr[20];
    itoa(value, valueStr, 10);
    printInputField(client, token, description, valueStr, maxLength);
}

void WebCfgServer::printParameter(WiFiClient &client, const char *description, const char *value)
{
    client.println("<tr>");
    client.print("<td>");
    client.print(description);
    client.print("</td>");
    client.print("<td>");
    client.print(value);
    client.print("</td>");
    client.println("</tr>");

}
