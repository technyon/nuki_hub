#include "WifiEthClient.h"

WifiEthClient::WifiEthClient(WiFiClient *wifiClient)
: _wifiClient(wifiClient)
{

}

WifiEthClient::~WifiEthClient()
{
    _wifiClient = nullptr;
}

uint8_t WifiEthClient::connected()
{
    return _wifiClient->connected();
}

int WifiEthClient::setTimeout(uint32_t seconds)
{
    _wifiClient->setTimeout(seconds);
    return seconds;
}

size_t WifiEthClient::write(const char *buffer, size_t size)
{
    return _wifiClient->write(buffer, size);
}

IPAddress WifiEthClient::localIP()
{
    return _wifiClient->localIP();
}

void WifiEthClient::stop()
{
    _wifiClient->stop();
}

size_t WifiEthClient::write_P(const char *buf, size_t size)
{
    return _wifiClient->write_P(buf, size);
}

int WifiEthClient::available()
{
    return _wifiClient->available();
}

String WifiEthClient::readStringUntil(char terminator)
{
    return _wifiClient->readStringUntil(terminator);
}

size_t WifiEthClient::readBytes(char *buffer, size_t length)
{
    return _wifiClient->readBytes(buffer, length);
}

void WifiEthClient::flush()
{
    _wifiClient->flush();
}

int WifiEthClient::read()
{
    return _wifiClient->read();
}

unsigned long WifiEthClient::getTimeout(void)
{
    return _wifiClient->getTimeout();
}

size_t WifiEthClient::write(Stream &stream)
{
    return _wifiClient->write(stream);
}
