#pragma once
#include <functional>
#include "NukiDataTypes.h"
#include "NukiPublisher.h"

class NukiRetryHandler
{
public:
    NukiRetryHandler(std::string reference, Gpio* gpio, std::vector<uint8_t> pinsCommError, int nrOfRetries, int retryDelay);

    const Nuki::CmdResult retryComm(std::function<Nuki::CmdResult ()> func);


private:
    void setCommErrorPins(const uint8_t& value);

    std::string _reference;
    Gpio* _gpio = nullptr;
    int _nrOfRetries = 0;
    int _retryDelay = 0;
    std::vector<uint8_t> _pinsCommError;
};
