#pragma once

#include <stdint.h>
#include <cstddef>

class EthClient
{
public:
    virtual uint8_t connected() = 0;
    virtual int available() = 0;
    virtual unsigned long getTimeout(void) = 0;
    virtual int setTimeout(uint32_t seconds) = 0;
    virtual int read() = 0;
    virtual size_t write(const char *buffer, size_t size) = 0;
    virtual size_t write_P(PGM_P buf, size_t size) = 0;
    virtual size_t write(Stream &stream) = 0;
    virtual String readStringUntil(char terminator) = 0;
    virtual size_t readBytes(char *buffer, size_t length) = 0;
    virtual IPAddress localIP() = 0;
    virtual void stop() = 0;
    virtual void flush() = 0;
};
