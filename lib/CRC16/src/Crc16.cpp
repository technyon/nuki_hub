#include "Crc16.h"

//---------------------------------------------------
// Initialize crc calculation
//---------------------------------------------------
void Crc16::clearCrc()
{
    _crc = _xorIn;
}

//---------------------------------------------------
// Update crc with new data
//---------------------------------------------------
void Crc16::updateCrc(uint8_t data)
{
    if (_reflectIn != 0)
        data = (uint8_t)reflect(data);

    int j = 0x80;

    while (j > 0)
    {
        uint16_t bit = (uint16_t)(_crc & _msbMask);

        _crc <<= 1;

        if ((data & j) != 0)
        {
            bit = (uint16_t)(bit ^ _msbMask);
        }

        if (bit != 0)
        {
            _crc ^= _polynomial;
        }

        j >>= 1;
    }
}

//---------------------------------------------------
// Get final crc value
//---------------------------------------------------
uint16_t Crc16::getCrc()
{
    if (_reflectOut != 0)
        _crc = (unsigned int)((reflect(_crc) ^ _xorOut) & _mask);

    return _crc;
}

//---------------------------------------------------
// Calculate generic crc code on data array
// Examples of crc 16:
// Kermit: 		width=16 poly=0x1021 init=0x0000 refin=true  refout=true  xorout=0x0000 check=0x2189
// Modbus: 		width=16 poly=0x8005 init=0xffff refin=true  refout=true  xorout=0x0000 check=0x4b37
// XModem: 		width=16 poly=0x1021 init=0x0000 refin=false refout=false xorout=0x0000 check=0x31c3
// CCITT-False:	width=16 poly=0x1021 init=0xffff refin=false refout=false xorout=0x0000 check=0x29b1
//---------------------------------------------------
unsigned int Crc16::fastCrc(uint8_t data[], uint8_t start, uint16_t length, uint8_t reflectIn, uint8_t reflectOut, uint16_t polynomial, uint16_t xorIn, uint16_t xorOut, uint16_t msbMask, uint16_t mask)
{
    uint16_t crc = xorIn;

    int j;
    uint8_t c;
    unsigned int bit;

    if (length == 0)
        return crc;

    for (int i = start; i < (start + length); i++)
    {
        c = data[i];

        if (reflectIn != 0)
            c = (uint8_t)reflect(c);

        j = 0x80;

        while (j > 0)
        {
            bit = (unsigned int)(crc & msbMask);
            crc <<= 1;

            if ((c & j) != 0)
            {
                bit = (unsigned int)(bit ^ msbMask);
            }

            if (bit != 0)
            {
                crc ^= polynomial;
            }

            j >>= 1;
        }
    }

    if (reflectOut != 0)
        crc = (unsigned int)((reflect((uint16_t)crc) ^ xorOut) & mask);

    return crc;
}

//-------------------------------------------------------
// Reflects bit in a uint8_t
//-------------------------------------------------------
uint8_t Crc16::reflect(uint8_t data)
{
    const uint8_t bits = 8;
    unsigned long reflection = 0x00000000;
    // Reflect the data about the center bit.
    for (uint8_t bit = 0; bit < bits; bit++)
    {
        // If the LSB bit is set, set the reflection of it.
        if ((data & 0x01) != 0)
        {
            reflection |= (unsigned long)(1 << ((bits - 1) - bit));
        }

        data = (uint8_t)(data >> 1);
    }

    return reflection;
}

//-------------------------------------------------------
// Reflects bit in a uint16_t
//-------------------------------------------------------
uint16_t Crc16::reflect(uint16_t data)
{
    const uint8_t bits = 16;
    unsigned long reflection = 0x00000000;
    // Reflect the data about the center bit.
    for (uint8_t bit = 0; bit < bits; bit++)
    {
        // If the LSB bit is set, set the reflection of it.
        if ((data & 0x01) != 0)
        {
            reflection |= (unsigned long)(1 << ((bits - 1) - bit));
        }

        data = (uint16_t)(data >> 1);
    }

    return reflection;
}

unsigned int Crc16::XModemCrc(uint8_t data[], uint8_t start, uint16_t length)
{
    //  XModem parameters: poly=0x1021 init=0x0000 refin=false refout=false xorout=0x0000
    return fastCrc(data, start, length, false, false, 0x1021, 0x0000, 0x0000, 0x8000, 0xffff);
}

unsigned int Crc16::Mcrf4XX(uint8_t data[], uint8_t start, uint16_t length)
{
    return fastCrc(data, start, length, true, true, 0x1021, 0xffff, 0x0000, 0x8000, 0xffff);
}

unsigned int Crc16::Modbus(uint8_t data[], uint8_t start, uint16_t length)
{
    return fastCrc(data, start, length, true, true, 0x8005, 0xffff, 0x0000, 0x8000, 0xffff);
}