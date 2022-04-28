#include "EthernetServerImpl.h"

EthernetServerImpl::EthernetServerImpl(IPAddress addr, int port)
        : EthernetServer(port)
{}

EthernetServerImpl::EthernetServerImpl(int port)
        : EthernetServer(port)
{}

void EthernetServerImpl::begin(uint16_t port)
{
    EthernetServer::begin();
}

void EthernetServerImpl::begin()
{
    EthernetServer::begin();
}

void EthernetServerImpl::close()
{

}

int EthernetServerImpl::setNoDelay(bool nodelay)
{
    return 0;
}
