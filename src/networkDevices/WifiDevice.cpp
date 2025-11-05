#include "esp_task_wdt.h"
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
    ssid.trim();
    pass = _preferences->getString(preference_wifi_pass, "");
    pass.trim();
    WiFi.setHostname(_hostname.c_str());

    WiFi.onEvent([&](WiFiEvent_t event, WiFiEventInfo_t info)
    {
        onWifiEvent(event, info);
    });

    if(isWifiConfigured())
    {
        Log->println(String("Attempting to connect to saved SSID ") + String(ssid));
        _openAP = false;
        if(_preferences->getBool(preference_find_best_rssi, false))
        {
            scan(false, true);
        }
        else
        {
            WiFi.mode(WIFI_STA);
            connect();
        }
    }
    else
    {
        Log->println("No SSID or Wifi password saved, opening AP");
        _openAP = true;
        scan(false, true);
    }
    return;
}

void WifiDevice::scan(bool passive, bool async)
{
    if (!_openAP)
    {
        _wifiClientStarted = false;
        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
    }

    int loop = 0;
    while (!_wifiClientStarted && loop < 50)
    {
        if (esp_task_wdt_status(NULL) == ESP_OK)
        {
            esp_task_wdt_reset();
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
        loop++;
    }

    WiFi.scanDelete();
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

    if(async)
    {
        Log->println("Wi-Fi async scan started");
    }
    else
    {
        Log->println("Wi-Fi sync scan started");
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
        Log->println("Starting AP with SSID NukiHub and Password NukiHubESP32");
        _startAP = false;
        WiFi.mode(WIFI_AP);
        if (esp_task_wdt_status(NULL) == ESP_OK)
        {
            esp_task_wdt_reset();
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
        WiFi.softAPsetHostname(_hostname.c_str());
        if (esp_task_wdt_status(NULL) == ESP_OK)
        {
            esp_task_wdt_reset();
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
        WiFi.softAP("NukiHub", "NukiHubESP32");

        //if(MDNS.begin(_hostname.c_str())){
        //  MDNS.addService("http", "tcp", 80);
        //}
    }
}

bool WifiDevice::connect()
{
    int loop = 0;
    while (!_wifiClientStarted && loop < 50)
    {
        if (esp_task_wdt_status(NULL) == ESP_OK)
        {
            esp_task_wdt_reset();
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
        loop++;
    }

    int bestConnection = -1;

    if(_preferences->getBool(preference_find_best_rssi, false))
    {
        for (int i = 0; i < _foundNetworks; i++)
        {
            if (ssid == WiFi.SSID(i))
            {
                Log->println(String("Saved SSID ") + ssid + String(" found with RSSI: ") +
                             String(WiFi.RSSI(i)) + String(("(")) +
                             String(constrain((100.0 + WiFi.RSSI(i)) * 2, 0, 100)) +
                             String(" %) and BSSID: ") + WiFi.BSSIDstr(i) +
                             String(" and channel: ") + String(WiFi.channel(i)));
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
            Log->println(String("Trying to connect to SSID ") + ssid + String(" found with RSSI: ") +
                         String(WiFi.RSSI(bestConnection)) + String(("(")) +
                         String(constrain((100.0 + WiFi.RSSI(bestConnection)) * 2, 0, 100)) +
                         String(" %) and BSSID: ") + WiFi.BSSIDstr(bestConnection) +
                         String(" and channel: ") + String(WiFi.channel(bestConnection)));
        }
    }

    if(!_ipConfiguration->dhcpEnabled())
    {
        WiFi.config(_ipConfiguration->ipAddress(), _ipConfiguration->dnsServer(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet());
    }

    if (bestConnection == -1)
    {
        WiFi.begin(ssid, pass);
    }
    else
    {
        WiFi.begin(ssid, pass, WiFi.channel(bestConnection), WiFi.BSSID(bestConnection), 1);
    }

    Log->print("WiFi connecting");
    loop = 0;
    while(!isConnected() && loop < 600)
    {
        Log->print(".");
        if (esp_task_wdt_status(NULL) == ESP_OK)
        {
            esp_task_wdt_reset();
        }
        vTaskDelay(25 / portTICK_PERIOD_MS);
        loop++;
    }
    Log->println("");

    if (!isConnected())
    {
        Log->println("Failed to connect within 15 seconds");

        if(_preferences->getBool(preference_restart_on_disconnect, false) && (espMillis() > 60000))
        {
            Log->println("Restart on disconnect watchdog triggered, rebooting");
            if (esp_task_wdt_status(NULL) == ESP_OK)
            {
                esp_task_wdt_reset();
            }
            vTaskDelay(100 / portTICK_PERIOD_MS);
            restartEsp(RestartReason::RestartOnDisconnectWatchdog);
        }
        else
        {
            Log->println("Retrying WiFi connection");
            scan(false, true);
        }

        return false;
    }

    return true;
}

bool WifiDevice::isWifiConfigured() const
{
    return ssid.length() > 0 && pass.length() > 0;
}

void WifiDevice::reconfigure()
{
    _preferences->putString(preference_wifi_ssid, "");
    _preferences->putString(preference_wifi_pass, "");
    if (esp_task_wdt_status(NULL) == ESP_OK)
    {
        esp_task_wdt_reset();
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
    restartEsp(RestartReason::ReconfigureWifi);
}

bool WifiDevice::isConnected()
{
    return WiFi.isConnected();
}

void WifiDevice::onConnected()
{
    Log->println("Wi-Fi connected");
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
    connect();
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
    Log->printf("[WiFi-event] event: %d\n", event);

    switch (event)
    {
    case ARDUINO_EVENT_WIFI_READY:
        Log->println("WiFi interface ready");
        break;
    case ARDUINO_EVENT_WIFI_SCAN_DONE:
        Log->println("Completed scan for access points");
        _foundNetworks = WiFi.scanComplete();

        for (int i = 0; i < _foundNetworks; i++)
        {
            Log->println(String("SSID ") + WiFi.SSID(i) + String(" found with RSSI: ") +
                         String(WiFi.RSSI(i)) + String(("(")) +
                         String(constrain((100.0 + WiFi.RSSI(i)) * 2, 0, 100)) +
                         String(" %) and BSSID: ") + WiFi.BSSIDstr(i) +
                         String(" and channel: ") + String(WiFi.channel(i)));
        }

        if (_openAP)
        {
            openAP();
        }
        else if (_foundNetworks > 0 || _preferences->getBool(preference_find_best_rssi, false))
        {
            esp_wifi_scan_stop();
            connect();
        }
        else
        {
            Log->println("No networks found, restarting scan");
            scan(false, true);
        }
        break;
    case ARDUINO_EVENT_WIFI_STA_START:
        Log->println("WiFi client started");
        _wifiClientStarted = true;
        break;
    case ARDUINO_EVENT_WIFI_STA_STOP:
        Log->println("WiFi clients stopped");
        if(!_openAP)
        {
            onDisconnected();
        }
        break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Log->println("Connected to access point");
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Log->println("Disconnected from WiFi access point");
        if(!_openAP)
        {
            onDisconnected();
        }
        break;
    case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
        Log->println("Authentication mode of access point has changed");
        break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Log->print("Obtained IP address: ");
        Log->println(WiFi.localIP());
        if(!_openAP)
        {
            onConnected();
        }
        break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        Log->println("Lost IP address and IP address is reset to 0");
        if(!_openAP)
        {
            onDisconnected();
        }
        break;
    case ARDUINO_EVENT_WIFI_AP_START:
        Log->println("WiFi access point started");
        break;
    case ARDUINO_EVENT_WIFI_AP_STOP:
        Log->println("WiFi access point  stopped");
        break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
        Log->println("Client connected");
        break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
        Log->println("Client disconnected");
        break;
    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
        Log->println("Assigned IP address to client");
        break;
    case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
        Log->println("Received probe request");
        break;
    case ARDUINO_EVENT_WIFI_AP_GOT_IP6:
        Log->println("AP IPv6 is preferred");
        break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
        Log->println("STA IPv6 is preferred");
        break;
    default:
        break;
    }
}