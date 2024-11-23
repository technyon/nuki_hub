#include "WifiDevice.h"
#include "../PreferencesKeys.h"
#include "../Logger.h"
#include "../RestartReason.h"
#include "../EspMillis.h"

WifiDevice::WifiDevice(const String& hostname, Preferences* preferences, const IPConfiguration* ipConfiguration)
    : NetworkDevice(hostname, preferences, ipConfiguration),
      _preferences(preferences)
{
#ifndef NUKI_HUB_UPDATER
    NetworkDevice::init();
#endif
}

const String WifiDevice::deviceName() const
{
    return "Built-in Wi-Fi";
}

void WifiDevice::initialize()
{
    ssid = _preferences->getString(preference_wifi_ssid, "");
    pass = _preferences->getString(preference_wifi_pass, "");
    WiFi.setHostname(_hostname.c_str());

    WiFi.onEvent([&](WiFiEvent_t event, WiFiEventInfo_t info)
    {
        onWifiEvent(event, info);
    });

    ssid.trim();
    pass.trim();

    if(isWifiConfigured())
    {
        Log->println(String("Attempting to connect to saved SSID ") + String(ssid));
        _connectOnScanDone = true;
        _openAP = false;
        scan(false, true);
        return;
    }
    else if(!_preferences->getBool(preference_wifi_converted, false))
    {
        _connectOnScanDone = false;
        _openAP = false;
        _convertOldWiFi = true;
        scan(false, true);
        return;
    }
    else
    {
        Log->println("No SSID or Wifi password saved, opening AP");
        _connectOnScanDone = false;
        _openAP = true;
        scan(false, true);
        return;
    }
}

void WifiDevice::scan(bool passive, bool async)
{
    if(!_connecting)
    {
        WiFi.scanDelete();
        WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
        WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

        if(async)
        {
            Log->println(F("Wi-Fi async scan started"));
        }
        else
        {
            Log->println(F("Wi-Fi sync scan started"));
        }
        if(passive)
        {
            WiFi.scanNetworks(async,false,true,75U);
        }
        else
        {
            WiFi.scanNetworks(async);
        }
    }
}

void WifiDevice::openAP()
{
    if(_startAP)
    {
        _startAP = false;
        WiFi.mode(WIFI_AP);
        delay(500);
        WiFi.softAPsetHostname(_hostname.c_str());
        delay(500);
        WiFi.softAP("NukiHub", "NukiHubESP32");

        //if(MDNS.begin(_hostname.c_str())){
        //  MDNS.addService("http", "tcp", 80);
        //}
    }
}

bool WifiDevice::connect()
{
    bool ret = false;
    ssid = _preferences->getString(preference_wifi_ssid, "");
    pass = _preferences->getString(preference_wifi_pass, "");
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(_hostname.c_str());
    delay(500);

    int bestConnection = -1;

    if(_preferences->getBool(preference_find_best_rssi, false))
    {
        for (int i = 0; i < _foundNetworks; i++)
        {
            if (ssid == WiFi.SSID(i))
            {
                Log->println(String(F("Saved SSID ")) + ssid + String(F(" found with RSSI: ")) +
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
        }
    }

    _connecting = true;
    esp_wifi_scan_stop();

    if(!_ipConfiguration->dhcpEnabled())
    {
        WiFi.config(_ipConfiguration->ipAddress(), _ipConfiguration->dnsServer(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet());
    }

    WiFi.begin(ssid, pass);
    auto status = WiFi.waitForConnectResult(10000);

    switch (status)
    {
    case WL_CONNECTED:
        Log->println("WiFi connected");
        break;
    case WL_NO_SSID_AVAIL:
        Log->println("WiFi SSID not available");
        break;
    case WL_CONNECT_FAILED:
        Log->println("WiFi connection failed");
        break;
    case WL_IDLE_STATUS:
        Log->println("WiFi changing status");
        break;
    case WL_DISCONNECTED:
        Log->println("WiFi disconnected");
        break;
    default:
        Log->println("WiFi timeout");
        break;
    }

    if (status != WL_CONNECTED)
    {
        Log->println("Retrying");
        _connectOnScanDone = true;
        _openAP = false;
        scan(false, true);
        _connecting = false;
        return false;
    }
    else
    {
        if(!_preferences->getBool(preference_wifi_converted, false))
        {
            _preferences->putBool(preference_wifi_converted, true);
        }
        _connecting = false;
        return true;
    }

    if(_preferences->getBool(preference_restart_on_disconnect, false) && (espMillis() > 60000))
    {
        restartEsp(RestartReason::RestartOnDisconnectWatchdog);
        _connecting = false;
        return false;
    }

    return false;
}

bool WifiDevice::isWifiConfigured() const
{
    return ssid.length() > 0 && pass.length() > 0;
}

void WifiDevice::reconfigure()
{
    _preferences->putString(preference_wifi_ssid, "");
    _preferences->putString(preference_wifi_pass, "");
    delay(200);
    restartEsp(RestartReason::ReconfigureWifi);
}

bool WifiDevice::isConnected()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }
    if (!_hasIP)
    {
        return false;
    }

    return true;
}

void WifiDevice::onConnected()
{
    Log->println(F("Wi-Fi connected"));
    _connected = true;
}

void WifiDevice::onDisconnected()
{
    if (!_connected)
    {
        return;
    }
    _connected = false;

    Log->println("Wi-Fi disconnected");

    //QUICK RECONNECT
    _connecting = true;
    ssid = _preferences->getString(preference_wifi_ssid, "");
    pass = _preferences->getString(preference_wifi_pass, "");

    if(!_ipConfiguration->dhcpEnabled())
    {
        WiFi.config(_ipConfiguration->ipAddress(), _ipConfiguration->dnsServer(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet());
    }

    WiFi.begin(ssid, pass);

    int loop = 0;

    while(!isConnected() && loop < 200)
    {
        loop++;
        delay(100);
    }

    _connecting = false;
    //END QUICK RECONNECT

    if(!isConnected())
    {
        if(_preferences->getBool(preference_restart_on_disconnect, false) && (espMillis() > 60000))
        {
            Log->println("Restart on disconnect watchdog triggered, rebooting");
            delay(100);
            restartEsp(RestartReason::RestartOnDisconnectWatchdog);
        }

        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(500);

        wifi_mode_t wifiMode;
        esp_wifi_get_mode(&wifiMode);

        while (wifiMode != WIFI_MODE_STA || WiFi.status() == WL_CONNECTED)
        {
            delay(500);
            Log->println("Waiting for WiFi mode change or disconnection.");
            esp_wifi_get_mode(&wifiMode);
        }

        _connectOnScanDone = true;
        _openAP = false;
        scan(false, true);
    }
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

void WifiDevice::onWifiEvent(const WiFiEvent_t &event, const WiFiEventInfo_t &info)
{
    if(event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED || event == ARDUINO_EVENT_WIFI_STA_STOP)
    {
        if(!_openAP && !_connecting && _connected)
        {
            onDisconnected();
            _hasIP = false;
        }
    }
    else if(event == ARDUINO_EVENT_WIFI_STA_GOT_IP)
    {
        _hasIP = true;
    }
    else if(event == ARDUINO_EVENT_WIFI_STA_LOST_IP)
    {
        _hasIP = false;
    }
    else if(event == ARDUINO_EVENT_WIFI_STA_CONNECTED)
    {
        onConnected();
    }
    else if(event == ARDUINO_EVENT_WIFI_SCAN_DONE)
    {
        Log->println(F("Wi-Fi scan done"));
        _foundNetworks = WiFi.scanComplete();

        for (int i = 0; i < _foundNetworks; i++)
        {
            Log->println(String(F("SSID ")) + WiFi.SSID(i) + String(F(" found with RSSI: ")) +
                         String(WiFi.RSSI(i)) + String(F("(")) +
                         String(constrain((100.0 + WiFi.RSSI(i)) * 2, 0, 100)) +
                         String(F(" %) and BSSID: ")) + WiFi.BSSIDstr(i) +
                         String(F(" and channel: ")) + String(WiFi.channel(i)));
        }

        if (_openAP)
        {
            openAP();
        }
        else if(_convertOldWiFi)
        {
            Log->println("Trying to convert old WiFi settings");
            _convertOldWiFi = false;
            _preferences->putBool(preference_wifi_converted, true);

            wifi_config_t wifi_cfg;
            if(esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) != ESP_OK)
            {
                Log->println("Failed to get Wi-Fi configuration in RAM");
            }

            if (esp_wifi_set_storage(WIFI_STORAGE_FLASH) != ESP_OK)
            {
                Log->println("Failed to set storage Wi-Fi");
            }

            String tempSSID = String(reinterpret_cast<const char*>(wifi_cfg.sta.ssid));
            String tempPass = String(reinterpret_cast<const char*>(wifi_cfg.sta.password));
            tempSSID.trim();
            tempPass.trim();
            bool found = false;

            for (int i = 0; i < _foundNetworks; i++)
            {
                if(tempSSID.length() > 0 && tempSSID == WiFi.SSID(i) && tempPass.length() > 0)
                {
                    _preferences->putString(preference_wifi_ssid, tempSSID);
                    _preferences->putString(preference_wifi_pass, tempPass);
                    Log->println("Succesfully converted old WiFi settings");
                    found = true;
                    break;
                }
            }

            WiFi.disconnect(true, true);

            if(found)
            {
                Log->println(String("Attempting to connect to saved SSID ") + String(ssid));
                _connectOnScanDone = true;
                _openAP = false;
                scan(false, true);
                return;
            }
            else
            {
                restartEsp(RestartReason::ReconfigureWifi);
                return;
            }
        }
        else if ((_connectOnScanDone && _foundNetworks > 0) || _preferences->getBool(preference_find_best_rssi, false))
        {
            connect();
        }
        else if (_connectOnScanDone)
        {
            Log->println("No networks found, restarting scan");
            scan(false, true);
        }
    }
}
