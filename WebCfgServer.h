#pragma once

#include <WiFiServer.h>

enum class TokenType
{
    NONE,
    MQTT_SERVER,
};

class WebCfgServer
{
public:
    WebCfgServer();
    ~WebCfgServer() = default;

    void initialize();
    void update();


private:
    void serveHtml(WiFiClient& client);
    TokenType getParameterType(char*& token);

    WiFiServer _wifiServer;

    bool _enabled = true;
};