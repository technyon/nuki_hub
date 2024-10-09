#include "esp_wifi.h"
#include <WiFi.h>
#include "WifiDevice.h"
#include "../PreferencesKeys.h"
#include "../Logger.h"
#include "../RestartReason.h"

WifiDevice::WifiDevice(const String& hostname, Preferences* preferences, const IPConfiguration* ipConfiguration)
: NetworkDevice(hostname, ipConfiguration),
  _preferences(preferences)
{
}

const String WifiDevice::deviceName() const
{
    return "Built-in Wi-Fi";
}

void WifiDevice::initialize()
{
    String ssid = savedSSID();
    String pass = savedPass();
    WiFi.setHostname(_hostname.c_str());

    if(!_ipConfiguration->dhcpEnabled())
    {
        WiFi.config(_ipConfiguration->ipAddress(), _ipConfiguration->dnsServer(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet());
    }

    WiFi.onEvent([&](WiFiEvent_t event, WiFiEventInfo_t info)
    {
        if(event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
        {
            onDisconnected();
        }
        else if(event == ARDUINO_EVENT_WIFI_STA_GOT_IP)
        {
            Log->println(WiFi.localIP().toString());
            onConnected();
        }
        else if(event == ARDUINO_EVENT_WIFI_SCAN_DONE)
        {
            Log->println(F("Wi-Fi async scan done"));
            _foundNetworks = WiFi.scanComplete();

            for (int i = 0; i < _foundNetworks; i++)
            {
                Log->println(String(F("SSID ")) + WiFi.SSID(i) + String(F(" found with RSSI: ")) +
                         String(WiFi.RSSI(i)) + String(F("(")) +
                         String(constrain((100.0 + WiFi.RSSI(i)) * 2, 0, 100)) +
                         String(F(" %) and BSSID: ")) + WiFi.BSSIDstr(i) +
                         String(F(" and channel: ")) + String(WiFi.channel(i)));
            }

            if (_connectOnScanDone)
            {
                connect();
            }
            else if (_openAP)
            {
                openAP();
            }
        }
    });

    if(ssid.length() > 0 && pass.length() > 0)
    {
        Log->println(String(F("SSID ")) + ssid + " length " + String(ssid.length()) + " " +
                     String("Password length ") + String(pass.length()));
        _connectOnScanDone = true;
        _openAP = false;
        scan();
    }
    else
    {
        Log->println("No SSID or Wifi password saved, opening AP");
        _connectOnScanDone = false;
        _openAP = true;
        scan();
    }
}

void WifiDevice::scan()
{
    WiFi.scanDelete();
    Log->println(F("Wi-Fi async scan started"));
    WiFi.scanNetworks(true);
}

void WifiDevice::openAP()
{
    WiFi.softAPsetHostname(_hostname.c_str());
    WiFi.softAP("NukiHub", "NukiHubESP32");
}

bool WifiDevice::connect()
{
    bool ret = false;
    String ssid = savedSSID();
    String pass = savedPass();
    WiFi.persistent(true);
    WiFi.enableSTA(true);
    WiFi.persistent(false);
    delay(500);

    int bestConnection = -1;
    for (int i = 0; i < _foundNetworks; i++)
    {
        if (ssid == WiFi.SSID(i))
        {
            Log->println(String(F("SSID ")) + ssid + String(F(" found with RSSI: ")) +
                     String(WiFi.RSSI(i)) + String(F("(")) +
                     String(constrain((100.0 + WiFi.RSSI(i)) * 2, 0, 100)) +
                     String(F(" %) and BSSID: ")) + WiFi.BSSIDstr(i) +
                     String(F(" and channel: ")) + String(WiFi.channel(i)));
            if (bestConnection == -1)
            {
                bestConnection = i;
            }
            else
            {
                if (WiFi.RSSI(i) > WiFi.RSSI(bestConnection))
                {
                    bestConnection = i;
                }
            }
        }
    }

    if (bestConnection == -1)
    {
        Log->print("No network found with SSID: ");
        Log->println(ssid);
    }
    else
    {
        Log->println(String(F("Trying to connect to SSID ")) + ssid + String(F(" found with RSSI: ")) +
               String(WiFi.RSSI(bestConnection)) + String(F("(")) +
               String(constrain((100.0 + WiFi.RSSI(bestConnection)) * 2, 0, 100)) +
               String(F(" %) and BSSID: ")) + WiFi.BSSIDstr(bestConnection) +
               String(F(" and channel: ")) + String(WiFi.channel(bestConnection)));
        ret = WiFi.begin(ssid.c_str(), pass.c_str(), 0, WiFi.BSSID(bestConnection), true);
    }

    if(!ret)
    {
        esp_wifi_disconnect();
        esp_wifi_stop();
        esp_wifi_deinit();

        Log->println(F("Failed to connect. Wait for ESP restart."));
        delay(1000);
        restartEsp(RestartReason::WifiInitFailed);
    }
    else
    {
        Log->println(F("Wi-Fi connected"));
    }

    return ret;
}

void WifiDevice::reconfigure()
{
    delay(200);
    restartEsp(RestartReason::ReconfigureWifi);
}

bool WifiDevice::isConnected()
{
    return WiFi.isConnected();
}

void WifiDevice::onConnected()
{

}

void WifiDevice::onDisconnected()
{
    _disconnectTs = (esp_timer_get_time() / 1000);
    if(_preferences->getBool(preference_restart_on_disconnect, false) && ((esp_timer_get_time() / 1000) > 60000)) restartEsp(RestartReason::RestartOnDisconnectWatchdog);
    _connectOnScanDone = true;
    _openAP = false;
    scan();
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

bool WifiDevice::isApOpen()
{
    return _openAP;
}

String WifiDevice::savedSSID() const
{
    wifi_config_t conf;
    esp_wifi_get_config(WIFI_IF_STA, &conf);
    return String(reinterpret_cast<const char*>(conf.sta.ssid));
}

String WifiDevice::savedPass() const {
    wifi_config_t conf;
    esp_wifi_get_config(WIFI_IF_STA, &conf);
    return String(reinterpret_cast<char*>(conf.sta.password));
}