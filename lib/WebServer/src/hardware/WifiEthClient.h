#pragma once

#include <WiFiClient.h>
#include "EthClient.h"

class WifiEthClient : public EthClient
{
public:
    explicit WifiEthClient(WiFiClient* wifiClient);
    virtual ~WifiEthClient();

    uint8_t connected() override;
    int available() override;
    unsigned long getTimeout(void) override;
    int setTimeout(uint32_t seconds) override;
    int read() override;
    size_t write(const char *buffer, size_t size) override;
    size_t write(Stream &stream) override;
    size_t write_P(const char *buf, size_t size) override;
    String readStringUntil(char terminator) override;
    size_t readBytes(char *buffer, size_t length) override;
    IPAddress localIP() override;
    void stop() override;
    void flush() override;

private:
    WiFiClient* _wifiClient;
};