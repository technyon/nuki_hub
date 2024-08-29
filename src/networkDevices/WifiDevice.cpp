#include <WiFi.h>
#include "WifiDevice.h"
#include "../PreferencesKeys.h"
#include "../Logger.h"
#include "../RestartReason.h"

RTC_NOINIT_ATTR char WiFiDevice_reconfdetect[17];

WifiDevice::WifiDevice(const String& hostname, Preferences* preferences, const IPConfiguration* ipConfiguration)
: NetworkDevice(hostname, ipConfiguration),
  _preferences(preferences),
  _wm(preferences->getString(preference_cred_user, "").c_str(), preferences->getString(preference_cred_password, "").c_str())
{
    _startAp = strcmp(WiFiDevice_reconfdetect, "reconfigure_wifi") == 0;
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
    _wm.setEnableConfigPortal(_startAp || !_preferences->getBool(preference_network_wifi_fallback_disabled, false));
    // reduced timeout if ESP is set to restart on disconnect
    _wm.setFindBestRSSI(_preferences->getBool(preference_find_best_rssi));
    _wm.setConnectTimeout(20);
    _wm.setConfigPortalTimeout(_preferences->getBool(preference_restart_on_disconnect, false) ? 60 * 3 : 60 * 30);
    _wm.setShowInfoUpdate(false);
    _wm.setMenu(wm_menu);
    _wm.setHostname(_hostname);

    if(!_ipConfiguration->dhcpEnabled())
    {
        _wm.setSTAStaticIPConfig(_ipConfiguration->ipAddress(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet(), _ipConfiguration->dnsServer());
    }

    _wm.setAPCallback(clearRtcInitVar);

    bool res = false;
    bool connectedFromPortal = false;

    if(_startAp)
    {
        Log->println(F("Opening Wi-Fi configuration portal."));
        res = _wm.startConfigPortal();
        connectedFromPortal = true;
    }
    else
    {
        res = _wm.autoConnect(); // password protected ap
    }

    if(!res)
    {
        esp_wifi_disconnect();
        esp_wifi_stop();
        esp_wifi_deinit();

        Log->println(F("Failed to connect. Wait for ESP restart."));
        delay(1000);
        restartEsp(RestartReason::WifiInitFailed);
    }
    else {
        Log->print(F("Wi-Fi connected: "));
        Log->println(WiFi.localIP().toString());

        if(connectedFromPortal)
        {
            Log->println(F("Connected using WifiManager portal. Wait for ESP restart."));
            delay(1000);
            restartEsp(RestartReason::ConfigurationUpdated);
        }
    }

    WiFi.onEvent([&](WiFiEvent_t event, WiFiEventInfo_t info)
    {
        if(event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
        {
            onDisconnected();
        }
        else if(event == ARDUINO_EVENT_WIFI_STA_GOT_IP)
        {
            onConnected();
        }
    });
}

void WifiDevice::reconfigure()
{
    strcpy(WiFiDevice_reconfdetect, "reconfigure_wifi");
    delay(200);
    restartEsp(RestartReason::ReconfigureWifi);
}

bool WifiDevice::isConnected()
{
    return WiFi.isConnected();
}

ReconnectStatus WifiDevice::reconnect(bool force)
{
    _wm.setFindBestRSSI(_preferences->getBool(preference_find_best_rssi));

    if((!isConnected() || force) && !_isReconnecting)
    {
        _isReconnecting = true;
        WiFi.disconnect();
        int loop = 0;

        while(isConnected() && loop <20)
        {
          delay(100);
          loop++;
        }

        _wm.resetScan();
        _wm.autoConnect();
        _isReconnecting = false;
    }

    if(!isConnected() && _disconnectTs > (esp_timer_get_time() / 1000) - 120000) _wm.setEnableConfigPortal(_startAp || !_preferences->getBool(preference_network_wifi_fallback_disabled, false));
    return isConnected() ? ReconnectStatus::Success : ReconnectStatus::Failure;
}

void WifiDevice::onConnected()
{
    _isReconnecting = false;
    _wm.setEnableConfigPortal(_startAp || !_preferences->getBool(preference_network_wifi_fallback_disabled, false));
}

void WifiDevice::onDisconnected()
{
    _disconnectTs = (esp_timer_get_time() / 1000);
    if(_preferences->getBool(preference_restart_on_disconnect, false) && ((esp_timer_get_time() / 1000) > 60000)) restartEsp(RestartReason::RestartOnDisconnectWatchdog);
    _wm.setEnableConfigPortal(false);
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