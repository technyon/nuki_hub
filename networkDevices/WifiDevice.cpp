#include <WiFi.h>
#include "WifiDevice.h"
#include "WiFiManager.h"
#include "../PreferencesKeys.h"

WifiDevice::WifiDevice(const String& hostname, Preferences* _preferences)
: NetworkDevice(hostname)
{
    size_t caLength = _preferences->getString(preference_mqtt_ca,_ca,TLS_CA_MAX_SIZE);
    size_t crtLength = _preferences->getString(preference_mqtt_crt,_cert,TLS_CERT_MAX_SIZE);
    size_t keyLength = _preferences->getString(preference_mqtt_key,_key,TLS_KEY_MAX_SIZE);

    if(caLength > 1) // length is 1 when empty
    {
        Serial.println(F("MQTT over TLS."));
        Serial.print(_ca);
        _wifiClientSecure = new WiFiClientSecure();
        _wifiClientSecure->setCACert(_ca);
        if(crtLength > 1 && keyLength > 1) // length is 1 when empty
        {
            Serial.println(F("MQTT with client certificate."));
            Serial.print(_cert);
            Serial.print(_key);
            _wifiClientSecure->setCertificate(_cert);
            _wifiClientSecure->setPrivateKey(_key);
        }
        _mqttClient = new PubSubClient(*_wifiClientSecure);
    } else
    {
        Serial.println(F("MQTT without TLS."));
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
