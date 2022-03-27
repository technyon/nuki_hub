#include "WebCfgServer.h"
#include <WiFiClient.h>

WebCfgServer::WebCfgServer()
: _wifiServer(80)
{}


void WebCfgServer::initialize()
{
    _wifiServer.begin();
}

void WebCfgServer::update()
{
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

                if(lastTokenType == TokenType::MQTT_SERVER && tokenType == TokenType::NONE)
                {
                    configChanged = true;
                    Serial.print("### ");
                    Serial.println(token);
//                    strcpy(_configuration->mqttServerAddress, token);
                }
            }
            lastToken = token;
            token = strtok(NULL, "?=&");
        }
//
//        if(configChanged)
//        {
//            _configuration->writeEeprom();
//            _enabled = false;
//        }
    }

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

    client.println("<FORM ACTION=method=get >");

    client.print("MQTT Server: <INPUT TYPE=TEXT VALUE=\"");
    client.print("");
    client.println("\" NAME=\"MQTTSERVER\" SIZE=\"25\" MAXLENGTH=\"40\"><BR>");

//    client.print("DNS Server: <INPUT TYPE=TEXT VALUE=\"");
//    client.print(_configuration->dnsServerAddress);
//    client.println("\" NAME=\"DNSSERVER\" SIZE=\"25\" MAXLENGTH=\"16\"><BR>");
//
//    client.print("Gateway: <INPUT TYPE=TEXT VALUE=\"");
//    client.print(_configuration->gatewayAddress);
//    client.println("\" NAME=\"GATEWAY\" SIZE=\"25\" MAXLENGTH=\"16\"><BR>");
//
//    client.print("IP Address: <INPUT TYPE=TEXT VALUE=\"");
//    client.print(_configuration->ipAddress);
//    client.println("\" NAME=\"IPADDRESS\" SIZE=\"25\" MAXLENGTH=\"16\"><BR>");
//
//    client.print("Subnet mask: <INPUT TYPE=TEXT VALUE=\"");
//    client.print(_configuration->subnetMask);
//    client.println("\" NAME=\"SUBNET\" SIZE=\"25\" MAXLENGTH=\"16\"><BR>");
//
//    client.print("MQTT publish interval (ms): <INPUT TYPE=TEXT VALUE=\"");
//    client.print(_configuration->mqttPublishInterval);
//    client.println("\" NAME=\"INTERVAL\" SIZE=\"25\" MAXLENGTH=\"6\"><BR>");

    client.println("<INPUT TYPE=SUBMIT NAME=\"submit\" VALUE=\"Save\">");

    client.println("</FORM>");

    client.println("<BR>");

    client.println("</BODY>");
    client.println("</HTML>");



}

TokenType WebCfgServer::getParameterType(char *&token)
{
    if (strcmp(token, "MQTTSERVER") == 0)
    {
        return TokenType::MQTT_SERVER;
    }

    return TokenType::NONE;
}
