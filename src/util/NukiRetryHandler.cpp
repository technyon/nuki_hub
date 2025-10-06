#include "NukiRetryHandler.h"
#include "Logger.h"

NukiRetryHandler::NukiRetryHandler(std::string reference, Gpio* gpio, std::vector<uint8_t> pinsComm, std::vector<uint8_t> pinsCommError, int nrOfRetries, int retryDelay)
: _reference(reference),
  _gpio(gpio),
  _pinsComm(pinsComm),
  _pinsCommError(pinsCommError),
  _nrOfRetries(nrOfRetries),
  _retryDelay(retryDelay)
{
}

const Nuki::CmdResult NukiRetryHandler::retryComm(std::function<Nuki::CmdResult()> func)
{
    Nuki::CmdResult cmdResult = Nuki::CmdResult::Error;

    int retryCount = 0;

    setCommPins(HIGH);

    while(retryCount < _nrOfRetries + 1 && cmdResult != Nuki::CmdResult::Success)
    {
        if (esp_task_wdt_status(NULL) == ESP_OK)
        {
            esp_task_wdt_reset();
        }

        cmdResult = func();

        if (cmdResult != Nuki::CmdResult::Success)
        {
            setCommErrorPins(HIGH);
            ++retryCount;

            Log->print(_reference.c_str());
            Log->print(": Last command failed, retrying after ");
            Log->print(_retryDelay);
            Log->print(" milliseconds. Retry ");
            Log->print(retryCount);
            Log->print(" of ");
            Log->println(_nrOfRetries);

            vTaskDelay(_retryDelay / portTICK_PERIOD_MS);
        }
    }
    setCommPins(LOW);
    setCommErrorPins(LOW);

    return cmdResult;
}

void NukiRetryHandler::setCommPins(const uint8_t& value)
{
    for (uint8_t pin : _pinsComm)
    {
        _gpio->setPinOutput(pin, value);
    }
}

void NukiRetryHandler::setCommErrorPins(const uint8_t& value)
{
    for (uint8_t pin : _pinsCommError)
    {
        _gpio->setPinOutput(pin, value);
    }
}


