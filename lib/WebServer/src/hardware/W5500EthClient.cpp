#include "W5500EthClient.h"

W5500EthClient::W5500EthClient(EthernetClient *wifiClient)
        : _ethClient(wifiClient)
{

}

W5500EthClient::~W5500EthClient()
{
    _ethClient = nullptr;
}

uint8_t W5500EthClient::connected()
{
    return _ethClient->connected();
}

int W5500EthClient::setTimeout(uint32_t seconds)
{
//    return _ethClient->setTimeout(seconds);
}

size_t W5500EthClient::write(const char *buffer, size_t size)
{
    if(size == 0)
    {
        return 0;
    }

    const size_t chunkSize = 2048; // W5100.SSIZE in socket.cpp

    uint32_t index = 0;
    uint32_t bytesLeft = 0;
    size_t written = 0;

    do
    {
        bytesLeft = size - index;
        if(bytesLeft >= chunkSize)
        {
            _ethClient->write(&buffer[index], chunkSize);
            index = index + chunkSize;
            written = written + chunkSize;
        }
        else
        {
            _ethClient->write(&buffer[index], bytesLeft);
            index = index + bytesLeft;
            written = written + bytesLeft;
        }
    } while (bytesLeft > 0);

    return written;
//    return _ethClient->write(buffer, size);
}

IPAddress W5500EthClient::localIP()
{
    return Ethernet.localIP();
}

void W5500EthClient::stop()
{
    _ethClient->stop();
}

size_t W5500EthClient::write_P(const char *buf, size_t size)
{
    return _ethClient->write(buf, size);
}

int W5500EthClient::available()
{
    return _ethClient->available();
}

String W5500EthClient::readStringUntil(char terminator)
{
    return _ethClient->readStringUntil(terminator);
}

size_t W5500EthClient::readBytes(char *buffer, size_t length)
{
    return _ethClient->readBytes(buffer, length);
}

void W5500EthClient::flush()
{
    _ethClient->flush();
}

int W5500EthClient::read()
{
    return _ethClient->read();
}

unsigned long W5500EthClient::getTimeout(void)
{
    return _ethClient->getTimeout();
}

size_t W5500EthClient::write(Stream &stream)
{
    uint8_t * buf = (uint8_t *)malloc(1360);
    if(!buf){
        return 0;
    }
    size_t toRead = 0, toWrite = 0, written = 0;
    size_t available = stream.available();
    while(available){
        toRead = (available > 1360)?1360:available;
        toWrite = stream.readBytes(buf, toRead);
        written += _ethClient->write(buf, toWrite);
        available = stream.available();
    }
    free(buf);
    return written;
}
