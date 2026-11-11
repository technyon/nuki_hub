#pragma once
#include <functional>
#include "NukiDataTypes.h"
#include "NukiPublisher.h"

class NukiRetryHandler
{
public:
    NukiRetryHandler(std::string reference, Gpio* gpio, std::vector<uint8_t> pinsComm, std::vector<uint8_t> pinsCommError, int nrOfRetries, int retryDelay);

    const Nuki::CmdResult retryComm(std::function<Nuki::CmdResult ()> func);

    uint32_t successCount() const;
    uint32_t failedCount() const;
    uint32_t totalRetries() const;
    Nuki::CmdResult lastResult() const;


private:
    void setCommPins(const uint8_t& value);
    void setCommErrorPins(const uint8_t& value);

    std::string _reference;
    Gpio* _gpio = nullptr;
    int _nrOfRetries = 0;
    int _retryDelay = 0;
    std::vector<uint8_t> _pinsComm;
    std::vector<uint8_t> _pinsCommError;
    uint32_t _successCount = 0;
    uint32_t _failedCount = 0;
    uint32_t _totalRetries = 0;
    Nuki::CmdResult _lastResult = Nuki::CmdResult::Success;
};
