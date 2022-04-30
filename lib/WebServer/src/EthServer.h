#pragma once

#include <IPAddress.h>
#include <WiFiClient.h>
#include "EthClient.h"

class EthServer
{
public:
    EthServer(IPAddress address, int port) { }

    EthServer(int port) { }

    virtual EthClient* available() = 0;
    virtual void discardClient() = 0;

    virtual void close() = 0;
    virtual void begin(const int port = 80) = 0;
    virtual void setNoDelay(const bool value) = 0;
};