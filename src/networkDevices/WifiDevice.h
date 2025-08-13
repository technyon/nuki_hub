#pragma once

#include <Preferences.h>
#include "NetworkDevice.h"
#include "IPConfiguration.h"
#include "esp_wifi.h"
#include <WiFi.h>

class WifiDevice : public NetworkDevice
{
public:
    WifiDevice(const String& hostname, Preferences* preferences, const IPConfiguration* ipConfiguration);

    const String deviceName() const override;

    virtual void initialize();
    virtual void reconfigure();
    virtual void scan(bool passive = false, bool async = true);

    virtual bool isConnected();
    virtual bool isApOpen();

    int8_t signalStrength() override;

    String localIP() override;
    String BSSIDstr() override;

private:
    void openAP();
    void onDisconnected();
    void onConnected();
    bool connect();
    bool isWifiConfigured() const;

    void onWifiEvent(const WiFiEvent_t& event, const WiFiEventInfo_t& info);

    Preferences* _preferences = nullptr;

    String ssid;
    String pass;

    int _foundNetworks = 0;
    bool _openAP = false;
    bool _startAP = true;
    bool _connected = false;
    bool _wifiClientStarted = false;
};