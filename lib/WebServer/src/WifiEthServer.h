#pragma once

#include "EthServer.h"
#include "WifiEthClient.h"
#include <WiFiServer.h>

class WifiEthServer : public EthServer
{
public:
    WifiEthServer(IPAddress address, int port);
    explicit WifiEthServer(int port);

    virtual EthClient* available();
    virtual void discardClient();

    virtual void begin(const int port = 80);
    virtual void close();

    virtual void setNoDelay(const bool value);

private:
    WiFiServer _wifiServer;
    WiFiClient _wifiClient;
    WifiEthClient* _wifiEthClient = nullptr;
};
