#include <WiFi.h>
#include "WifiDevice.h"
#include "WiFiManager.h"

WifiDevice::WifiDevice(const String& hostname)
: NetworkDevice(hostname),
  _mqttClient(_wifiClient)
{}

PubSubClient *WifiDevice::mqttClient()
{
    return &_mqttClient;
}

void WifiDevice::initialize()
{
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

    WiFiManager wm;

    std::vector<const char *> wm_menu;
    wm_menu.push_back("wifi");
    wm_menu.push_back("exit");
    wm.setShowInfoUpdate(false);
    wm.setMenu(wm_menu);
    wm.setHostname(_hostname);

    bool res = false;

    if(_cookie.isSet())
    {
        Serial.println(F("Opening WiFi configuration portal."));
        _cookie.clear();
        res = wm.startConfigPortal();
    }
    else
    {
        res = wm.autoConnect(); // password protected ap
    }

    if(!res) {
        Serial.println(F("Failed to connect. Wait for ESP restart."));
        delay(10000);
        ESP.restart();
    }
    else {
        Serial.print(F("WiFi connected: "));
        Serial.println(WiFi.localIP().toString());
    }
}

void WifiDevice::reconfigure()
{
    _cookie.set();
    delay(200);
    ESP.restart();
}

bool WifiDevice::isConnected()
{
    return WiFi.isConnected();
}

bool WifiDevice::reconnect()
{
    return isConnected();
}

void WifiDevice::update()
{

}
