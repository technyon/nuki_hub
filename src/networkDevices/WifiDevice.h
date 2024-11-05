#pragma once

#include <WiFiClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include "NetworkDevice.h"
#include "IPConfiguration.h"

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
    char* _path;

    Preferences* _preferences = nullptr;

    int _foundNetworks = 0;
    int _disconnectCount = 0;
    bool _connectOnScanDone = false;
    bool _connecting = false;
    bool _openAP = false;
    bool _startAP = true;
    bool _convertOldWiFi = false;
    bool _connected = false;
    bool _hasIP = false;
    uint8_t _connectedChannel = 0;
    uint8_t* _connectedBSSID;
    int64_t _disconnectTs = 0;
};
