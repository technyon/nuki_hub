#pragma once

#include "EthServer.h"
#include "W5500EthClient.h"
#include <WiFiServer.h>
#include <EthernetServer.h>

class EthernetServerImpl : public EthernetServer
{
public:
    EthernetServerImpl(int address, int port);
    explicit EthernetServerImpl(int port);

    virtual void begin(uint16_t port);
};

class W5500EthServer : public EthServer
{
public:
    W5500EthServer(IPAddress address, int port);
    explicit W5500EthServer(int port);

    virtual EthClient* available();
    virtual void discardClient();

    virtual void begin(const int port = 80);
    virtual void close();

    virtual void setNoDelay(const bool value);

private:
    EthernetServerImpl _ethServer;
    EthernetClient _ethClient;
    W5500EthClient* _W5500EthClient = nullptr;
};
