#include "WifiEthServer.h"


WifiEthServer::WifiEthServer(IPAddress address, int port)
: EthServer(address, port),
  _wifiServer(address, port)
{

}

WifiEthServer::WifiEthServer(int port)
: EthServer(port),
  _wifiServer(port)
{

}

void WifiEthServer::close()
{
    _wifiServer.close();
}

void WifiEthServer::begin(const int port)
{
    _wifiServer.begin(port);
}

void WifiEthServer::setNoDelay(const bool value)
{
    _wifiServer.setNoDelay(value);
}

EthClient* WifiEthServer::available()
{
    if(_wifiEthClient != nullptr)
    {
        delete _wifiEthClient;
        _wifiEthClient = nullptr;
    }

    _wifiClient = _wifiServer.accept();
    _wifiEthClient = new WifiEthClient(&_wifiClient);
    return _wifiEthClient;
}


void WifiEthServer::discardClient()
{
    if(_wifiEthClient != nullptr)
    {
        delete _wifiEthClient;
        _wifiEthClient = nullptr;
    }

    _wifiClient = WiFiClient();
}
