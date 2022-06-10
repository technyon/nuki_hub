#include <WiFi.h>
#include "WifiDevice.h"
#include "WiFiManager.h"
#include "../PreferencesKeys.h"

WifiDevice::WifiDevice(const String& hostname, Preferences* _preferences)
: NetworkDevice(hostname)
{
    String MQTT_CA = _preferences->getString(preference_mqtt_ca);
    String MQTT_CRT = _preferences->getString(preference_mqtt_crt);
    String MQTT_KEY = _preferences->getString(preference_mqtt_key);
    
    if(MQTT_CA.length() > 0)
    {
        _wifiClientSecure = new WiFiClientSecure();
        _wifiClientSecure->setCACert(MQTT_CA.c_str());
        if(MQTT_CRT.length() > 0 && MQTT_KEY.length() > 0)
        {
            _wifiClientSecure->setCertificate(MQTT_CRT.c_str());
            _wifiClientSecure->setPrivateKey(MQTT_KEY.c_str());
        }
        _mqttClient = new PubSubClient(*_wifiClientSecure);
    } else
    {
        _wifiClient = new WiFiClient();
        _mqttClient = new PubSubClient(*_wifiClient);
    }
}

PubSubClient *WifiDevice::mqttClient()
{
    return _mqttClient;
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

    _mqttClient.setBufferSize(_mqttMaxBufferSize);
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
