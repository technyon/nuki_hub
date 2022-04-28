#pragma once

#include <EthernetServer.h>

class EthernetServerImpl : public EthernetServer
{
public:
    EthernetServerImpl(IPAddress addr, int port);
    EthernetServerImpl(int port);

    void begin();
    void begin(uint16_t port);
    void close();
    int setNoDelay(bool nodelay);
};