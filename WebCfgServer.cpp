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
    bool configChanged = false;

    // Create a client connections
    WiFiClient client = _wifiServer.available();

    if (client)
    {
        int index = 0;
        char message[200];

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


    client.println("<br><br><h3>");
    client.print("Paired: ");
    client.println(_nuki->isPaired() ? "Yes" : "No");
    client.println("</h3>");
    client.println("<h3>");
    client.print("MQTT Connected: ");
    client.println(_network->isMqttConnected() ? "Yes" : "No");
    client.println("</h3><br><br>");

    client.println("<FORM ACTION=method=get >");

    client.print("MQTT Broker: <INPUT TYPE=TEXT VALUE=\"");
    client.print(_preferences->getString(preference_mqtt_broker));
    client.println("\" NAME=\"MQTTSERVER\" SIZE=\"25\" MAXLENGTH=\"40\"><BR>");

    client.print("MQTT Broker port: <INPUT TYPE=TEXT VALUE=\"");
    client.print(_preferences->getInt(preference_mqtt_broker_port));
    client.println("\" NAME=\"MQTTPORT\" SIZE=\"25\" MAXLENGTH=\"40\"><BR>");

    client.print("MQTT Path: <INPUT TYPE=TEXT VALUE=\"");
    client.print(_preferences->getString(preference_mqtt_path));
    client.println("\" NAME=\"MQTTPATH\" SIZE=\"25\" MAXLENGTH=\"40\"><BR>");

    client.print("Query interval lock state (seconds): <INPUT TYPE=TEXT VALUE=\"");
    client.print(_preferences->getInt(preference_query_interval_lockstate));
    client.println("\" NAME=\"LSTINT\" SIZE=\"25\" MAXLENGTH=\"16\"><BR>");

    client.print("Query interval battery (seconds): <INPUT TYPE=TEXT VALUE=\"");
    client.print(_preferences->getInt(preference_query_interval_battery));
    client.println("\" NAME=\"BATINT\" SIZE=\"25\" MAXLENGTH=\"16\"><BR>");

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
    if (strcmp(token, "MQTTPATH") == 0)
    {
        return TokenType::MqttPath;
    }

    return TokenType::None;
}
