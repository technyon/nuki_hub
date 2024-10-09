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

    virtual bool isConnected();
    virtual bool isApOpen();

    int8_t signalStrength() override;
    
    String localIP() override;
    String BSSIDstr() override;
private:
    void scan();
    void openAP();
    void onDisconnected();
    void onConnected();
    bool connect();
  
    String savedSSID() const;
    String savedPass() const;

    Preferences* _preferences = nullptr;

    int _foundNetworks = 0;
    bool _connectOnScanDone = false;
    bool _openAP = false;
    int64_t _disconnectTs = 0;
};
