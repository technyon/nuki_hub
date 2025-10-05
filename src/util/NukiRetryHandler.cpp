#include "NukiRetryHandler.h"
#include "Logger.h"

NukiRetryHandler::NukiRetryHandler(Gpio* gpio, std::vector<uint8_t> pinsCommError, int nrOfRetries, int retryDelay)
: _gpio(gpio),
  _pinsCommError(pinsCommError),
  _nrOfRetries(nrOfRetries),
  _retryDelay(retryDelay)
{
}

const Nuki::CmdResult NukiRetryHandler::retryComm(std::function<Nuki::CmdResult()> func)
{
    Nuki::CmdResult cmdResult = Nuki::CmdResult::Error;

    int retryCount = 0;

    while(retryCount < _nrOfRetries + 1 && cmdResult != Nuki::CmdResult::Success)
    {
        cmdResult = func();

        if (cmdResult != Nuki::CmdResult::Success)
        {
            setCommErrorPins(HIGH);
            ++retryCount;

            Log->print("Lock: Last command failed, retrying after ");
            Log->print(_retryDelay);
            Log->print(" milliseconds. Retry ");
            Log->print(retryCount);
            Log->print(" of ");
            Log->println(_nrOfRetries);

            vTaskDelay(_retryDelay / portTICK_PERIOD_MS);
        }
    }
    setCommErrorPins(LOW);

    return cmdResult;
}

void NukiRetryHandler::setCommErrorPins(const uint8_t& value)
{
    for (uint8_t pin : _pinsCommError)
    {
        _gpio->setPinOutput(pin, value);
    }
}


