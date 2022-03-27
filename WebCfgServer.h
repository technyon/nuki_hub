#pragma once

#include <WiFiServer.h>
#include <Preferences.h>

enum class TokenType
{
    NONE,
    MQTT_SERVER,
};

class WebCfgServer
{
public:
    WebCfgServer(Preferences* preferences);
    ~WebCfgServer() = default;

    void initialize();
    void update();


private:
    void serveHtml(WiFiClient& client);
    TokenType getParameterType(char*& token);

    WiFiServer _wifiServer;
    Preferences* _preferences;

    bool _enabled = true;
};