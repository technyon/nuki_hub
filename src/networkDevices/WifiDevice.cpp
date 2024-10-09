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
    String ssid = _preferences->getString(preference_wifi_ssid, "");
    String pass = _preferences->getString(preference_wifi_pass, "");
    WiFi.setHostname(_hostname.c_str());

    if(!_ipConfiguration->dhcpEnabled())
    {
        WiFi.config(_ipConfiguration->ipAddress(), _ipConfiguration->dnsServer(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet());
    }

    WiFi.onEvent([&](WiFiEvent_t event, WiFiEventInfo_t info)
    {
        if(event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
        {
            if(!_openAP && !_connecting && _connected)
            {
                onDisconnected();
            }
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

            if (_connectOnScanDone && _foundNetworks > 0)
            {
                connect();
            }
            else if (_connectOnScanDone)
            {
                Log->println("No networks found, restarting scan");
                scan(false, true);
            }
            else if (_openAP)
            {
                openAP();
            }
            else if(_convertOldWiFi)
            {
                _convertOldWiFi = false;
                _preferences->putBool(preference_wifi_converted, true);

                wifi_config_t wifi_cfg;
                if(esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) != ESP_OK) {
                    Log->println("Failed to get Wi-Fi configuration in RAM");
                }

                if (esp_wifi_set_storage(WIFI_STORAGE_FLASH) != ESP_OK) {
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
                        ssid = tempSSID;
                        pass = tempPass;
                        _preferences->putString(preference_wifi_ssid, ssid);
                        _preferences->putString(preference_wifi_pass, pass);
                        found = true;
                        break;
                    }
                }

                memset(wifi_cfg.sta.ssid, 0, sizeof(wifi_cfg.sta.ssid));
                memset(wifi_cfg.sta.password, 0, sizeof(wifi_cfg.sta.password));

                if (esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg) != ESP_OK) {
                    Log->println("Failed to clear NVS Wi-Fi configuration");
                }

                if(found)
                {
                    Log->println(String("Attempting to connect to saved SSID ") + String(ssid));
                    _connectOnScanDone = true;
                    _openAP = false;
                    scan(false, true);
                }
                else
                {
                    Log->println("No SSID or Wifi password saved, opening AP");
                    _connectOnScanDone = false;
                    _openAP = true;
                    scan(false, true);
                }
            }
        }
    });

    ssid.trim();
    pass.trim();

    if(ssid.length() > 0 && ssid != "~" && pass.length() > 0)
    {
        Log->println(String("Attempting to connect to saved SSID ") + String(ssid));
        _connectOnScanDone = true;
        _openAP = false;
        scan(false, true);
    }
    else
    {
        if(!_preferences->getBool(preference_wifi_converted, false))
        {
            _connectOnScanDone = false;
            _openAP = false;
            _convertOldWiFi = true;
            scan(false, true);
        }

        ssid.trim();
        pass.trim();

        if(ssid.length() > 0 && ssid != "~" && pass.length() > 0)
        {
            Log->println(String("Attempting to connect to saved SSID ") + String(ssid));
            _connectOnScanDone = true;
            _openAP = false;
            scan(false, true);
        }
        else
        {
            Log->println("No SSID or Wifi password saved, opening AP");
            _connectOnScanDone = false;
            _openAP = true;
            scan(false, true);
        }
    }
}

void WifiDevice::scan(bool passive, bool async)
{
    WiFi.scanDelete();

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

void WifiDevice::openAP()
{
    if(_startAP)
    {
        WiFi.persistent(false);
        WiFi.mode(WIFI_AP_STA);
        WiFi.persistent(false);
        WiFi.softAPsetHostname(_hostname.c_str());
        WiFi.softAP("NukiHub", "NukiHubESP32");
        WiFi.persistent(false);
        _startAP = false;
    }
}

bool WifiDevice::connect()
{
    bool ret = false;
    String ssid = _preferences->getString(preference_wifi_ssid, "");
    String pass = _preferences->getString(preference_wifi_pass, "");
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(_hostname.c_str());
    delay(500);

    int bestConnection = -1;
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
        if(_preferences->getBool(preference_restart_on_disconnect, false) && ((esp_timer_get_time() / 1000) > 60000)) restartEsp(RestartReason::RestartOnDisconnectWatchdog);
        _connectOnScanDone = true;
        _openAP = false;
        scan(false, true);
        return false;
    }
    else
    {
        _connecting = true;
        Log->println(String(F("Trying to connect to SSID ")) + ssid + String(F(" found with RSSI: ")) +
               String(WiFi.RSSI(bestConnection)) + String(F("(")) +
               String(constrain((100.0 + WiFi.RSSI(bestConnection)) * 2, 0, 100)) +
               String(F(" %) and BSSID: ")) + WiFi.BSSIDstr(bestConnection) +
               String(F(" and channel: ")) + String(WiFi.channel(bestConnection)));
        ret = WiFi.begin(ssid.c_str(), pass.c_str(), WiFi.channel(bestConnection), WiFi.BSSID(bestConnection), true);
        WiFi.persistent(false);
        _connecting = false;
    }

    if(!ret)
    {
        int loop = 0;

        while(!isConnected() && loop < 200)
        {
            loop++;
            delay(100);
        }

        if(!isConnected())
        {
            esp_wifi_disconnect();
            esp_wifi_stop();
            esp_wifi_deinit();

            Log->println(F("Failed to connect. Wait for ESP restart."));
            delay(1000);
            restartEsp(RestartReason::WifiInitFailed);
        }
    }
    else
    {
        if(!_preferences->getBool(preference_wifi_converted, false))
        {
            _preferences->putBool(preference_wifi_converted, true);
        }

        int loop = 0;

        while(!isConnected() && loop < 200)
        {
            loop++;
            delay(100);
        }

        if(!isConnected())
        {
            if(_preferences->getBool(preference_restart_on_disconnect, false) && ((esp_timer_get_time() / 1000) > 60000))
            {
                restartEsp(RestartReason::RestartOnDisconnectWatchdog);
                return false;
            }
            Log->print("Connection failed, retrying");
            _connectOnScanDone = true;
            _openAP = false;
            scan(false, true);
            return false;
        }
    }

    return ret;
}

void WifiDevice::reconfigure()
{
    bool changed = false;
    wifi_config_t wifi_cfg;
    if(esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) != ESP_OK) {
        Log->println("Failed to get Wi-Fi configuration in RAM");
    }

    if (esp_wifi_set_storage(WIFI_STORAGE_FLASH) != ESP_OK) {
        Log->println("Failed to set storage Wi-Fi");
    }

    if(sizeof(wifi_cfg.sta.ssid) > 0)
    {
        memset(wifi_cfg.sta.ssid, 0, sizeof(wifi_cfg.sta.ssid));
        changed = true;
    }
    if(sizeof(wifi_cfg.sta.password) > 0)
    {
        memset(wifi_cfg.sta.password, 0, sizeof(wifi_cfg.sta.password));
        changed = true;
    }
    if(changed)
    {
        if (esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg) != ESP_OK) {
            Log->println("Failed to clear NVS Wi-Fi configuration");
        }
    }

    _preferences->putString(preference_wifi_ssid, "");
    _preferences->putString(preference_wifi_pass, "");
    delay(200);
    restartEsp(RestartReason::ReconfigureWifi);
}

bool WifiDevice::isConnected()
{
    return (WiFi.status() == WL_CONNECTED);
}

void WifiDevice::onConnected()
{
    Log->println(F("Wi-Fi connected"));
    _connectedChannel = WiFi.channel();
    _connectedBSSID = WiFi.BSSID();
    _connected = true;
}

void WifiDevice::onDisconnected()
{
    if(_connected)
    {
        _connected = false;
        _disconnectTs = (esp_timer_get_time() / 1000);
        Log->println(F("Wi-Fi disconnected"));

        //QUICK RECONNECT
        _connecting = true;
        String ssid = _preferences->getString(preference_wifi_ssid, "");
        String pass = _preferences->getString(preference_wifi_pass, "");
        WiFi.begin(ssid.c_str(), pass.c_str(), _connectedChannel, _connectedBSSID, true);
        WiFi.persistent(false);
        
        int loop = 0;
        
        while(!isConnected() && loop < 50)
        {
            loop++;
            delay(100);
        }

        _connecting = false;
        //END QUICK RECONECT

        if(!isConnected())
        {
          if(_preferences->getBool(preference_restart_on_disconnect, false) && ((esp_timer_get_time() / 1000) > 60000)) restartEsp(RestartReason::RestartOnDisconnectWatchdog);

          WiFi.persistent(false);
          WiFi.disconnect(true);
          WiFi.mode(WIFI_STA);
          WiFi.disconnect();
          delay(500);
          
          wifi_mode_t wifiMode;
          esp_wifi_get_mode(&wifiMode);

          while (wifiMode != WIFI_MODE_STA || WiFi.status() == WL_CONNECTED)
          {
              delay(500);
              Log->println(F("Waiting for WiFi mode change or disconnection."));
              esp_wifi_get_mode(&wifiMode);
          }

          _connectOnScanDone = true;
          _openAP = false;
          scan(false, true);
        }
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