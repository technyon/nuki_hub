#include <WiFi.h>
#include "WifiDevice.h"
#include "../PreferencesKeys.h"
#include "../Logger.h"
#ifndef NUKI_HUB_UPDATER
#include "../MqttTopics.h"
#include "espMqttClient.h"
#endif
#include "../RestartReason.h"

RTC_NOINIT_ATTR char WiFiDevice_reconfdetect[17];

WifiDevice::WifiDevice(const String& hostname, Preferences* preferences, const IPConfiguration* ipConfiguration)
: NetworkDevice(hostname, ipConfiguration),
  _preferences(preferences),
  _wm(preferences->getString(preference_cred_user).c_str(), preferences->getString(preference_cred_password).c_str())
{
    _startAp = strcmp(WiFiDevice_reconfdetect, "reconfigure_wifi") == 0;

    #ifndef NUKI_HUB_UPDATER
    _restartOnDisconnect = preferences->getBool(preference_restart_on_disconnect);

    size_t caLength = preferences->getString(preference_mqtt_ca, _ca, TLS_CA_MAX_SIZE);
    size_t crtLength = preferences->getString(preference_mqtt_crt, _cert, TLS_CERT_MAX_SIZE);
    size_t keyLength = preferences->getString(preference_mqtt_key, _key, TLS_KEY_MAX_SIZE);

    _useEncryption = caLength > 1;  // length is 1 when empty

    if(_useEncryption)
    {
        Log->println(F("MQTT over TLS."));
        Log->println(_ca);
        _mqttClientSecure = new espMqttClientSecure(espMqttClientTypes::UseInternalTask::NO);
        _mqttClientSecure->setCACert(_ca);
        if(crtLength > 1 && keyLength > 1) // length is 1 when empty
        {
            Log->println(F("MQTT with client certificate."));
            Log->println(_cert);
            Log->println(_key);
            _mqttClientSecure->setCertificate(_cert);
            _mqttClientSecure->setPrivateKey(_key);
        }
    } else
    {
        Log->println(F("MQTT without TLS."));
        _mqttClient = new espMqttClient(espMqttClientTypes::UseInternalTask::NO);
    }

    if(preferences->getBool(preference_mqtt_log_enabled))
    {
        _path = new char[200];
        memset(_path, 0, sizeof(_path));

        String pathStr = preferences->getString(preference_mqtt_lock_path);
        pathStr.concat(mqtt_topic_log);
        strcpy(_path, pathStr.c_str());
        Log = new MqttLogger(*getMqttClient(), _path, MqttLoggerMode::MqttAndSerial);
    }
    #endif
}

const String WifiDevice::deviceName() const
{
    return "Built-in Wi-Fi";
}

void WifiDevice::initialize()
{
    std::vector<const char *> wm_menu;
    wm_menu.push_back("wifi");
    wm_menu.push_back("exit");
    _wm.setEnableConfigPortal(_startAp || !_preferences->getBool(preference_network_wifi_fallback_disabled));
    // reduced timeout if ESP is set to restart on disconnect
    _wm.setFindBestRSSI(_preferences->getBool(preference_find_best_rssi));
    _wm.setConfigPortalTimeout(_restartOnDisconnect ? 60 * 3 : 60 * 30);
    _wm.setShowInfoUpdate(false);
    _wm.setMenu(wm_menu);
    _wm.setHostname(_hostname);

    if(!_ipConfiguration->dhcpEnabled())
    {
        _wm.setSTAStaticIPConfig(_ipConfiguration->ipAddress(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet(), _ipConfiguration->dnsServer());
    }

    _wm.setAPCallback(clearRtcInitVar);

    bool res = false;

    if(_startAp)
    {
        Log->println(F("Opening Wi-Fi configuration portal."));
        res = _wm.startConfigPortal();
    }
    else
    {
        res = _wm.autoConnect(); // password protected ap
    }

    if(!res)
    {
        esp_wifi_disconnect ();
        esp_wifi_stop ();
        esp_wifi_deinit ();

        Log->println(F("Failed to connect. Wait for ESP restart."));
        delay(1000);
        restartEsp(RestartReason::WifiInitFailed);
    }
    else {
        Log->print(F("Wi-Fi connected: "));
        Log->println(WiFi.localIP().toString());
    }

    WiFi.onEvent([&](WiFiEvent_t event, WiFiEventInfo_t info)
    {
        if(event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
        {
            onDisconnected();
        }
    });
}

void WifiDevice::reconfigure()
{
    strcpy(WiFiDevice_reconfdetect, "reconfigure_wifi");
    delay(200);
    restartEsp(RestartReason::ReconfigureWifi);
}

bool WifiDevice::supportsEncryption()
{
    return true;
}

bool WifiDevice::isConnected()
{
    return WiFi.isConnected();
}

ReconnectStatus WifiDevice::reconnect()
{
    if(!isConnected())
    {
        _wm.autoConnect();
        delay(3000);
    }
    return isConnected() ? ReconnectStatus::Success : ReconnectStatus::Failure;
}

void WifiDevice::onDisconnected()
{
    if(_restartOnDisconnect && (millis() > 60000)) restartEsp(RestartReason::RestartOnDisconnectWatchdog);
    reconnect();
}

int8_t WifiDevice::signalStrength()
{
    return WiFi.RSSI();
}

String WifiDevice::localIP()
{
    return WiFi.localIP().toString();
}

String WifiDevice::BSSIDstr()
{
    return WiFi.BSSIDstr();
}

void WifiDevice::clearRtcInitVar(WiFiManager *)
{
    memset(WiFiDevice_reconfdetect, 0, sizeof WiFiDevice_reconfdetect);
}
