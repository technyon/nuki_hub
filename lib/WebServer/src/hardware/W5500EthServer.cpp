#include "W5500EthServer.h"


W5500EthServer::W5500EthServer(IPAddress address, int port)
        : EthServer(address, port),
          _ethServer(address, port)
{

}

W5500EthServer::W5500EthServer(int port)
        : EthServer(port),
          _ethServer(port)
{

}

void W5500EthServer::close()
{
//    _ethServer.close();
}

void W5500EthServer::begin(const int port)
{
    _ethServer.begin(port);
}

void W5500EthServer::setNoDelay(const bool value)
{
//    _ethServer.setNoDelay(value);
}

EthClient* W5500EthServer::available()
{
    if(_W5500EthClient != nullptr)
    {
        delete _W5500EthClient;
        _W5500EthClient = nullptr;
    }

    _ethClient = _ethServer.available();
    _W5500EthClient = new W5500EthClient(&_ethClient);
    return _W5500EthClient;
}


void W5500EthServer::discardClient()
{
    if(_W5500EthClient != nullptr)
    {
        delete _W5500EthClient;
        _W5500EthClient = nullptr;
    }

    _ethClient = EthernetClient();
}


// EthernetServerImpl
void EthernetServerImpl::begin(uint16_t port)
{
    EthernetServer::begin();
}

EthernetServerImpl::EthernetServerImpl(int address, int port)
: EthernetServer(port)
{

}

EthernetServerImpl::EthernetServerImpl(int port)
: EthernetServer(port)
{

}
