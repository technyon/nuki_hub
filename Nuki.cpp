#include "Nuki.h"
#include <FreeRTOS.h>

Nuki::Nuki(const std::string& name, uint32_t id, Network* network)
: _nukiBle(name, id),
  _network(network)
{
    memset(&_lastKeyTurnerState, sizeof(KeyTurnerState), 0);
    memset(&_keyTurnerState, sizeof(KeyTurnerState), 0);
}

void Nuki::initialize()
{
    _nukiBle.initialize();
}

void Nuki::update()
{
    if (!_paired) {
        Serial.println(F("Nuki start pairing"));

        if (_nukiBle.pairNuki()) {
            Serial.println(F("Nuki paired"));
            _paired = true;
        }
        else
        {
            return;
        }
    }

    vTaskDelay( 100 / portTICK_PERIOD_MS);
    _nukiBle.requestKeyTurnerState(&_keyTurnerState);
    Serial.print(F("Nuki lock state: "));
    Serial.println((int)_keyTurnerState.lockState);

    if(_keyTurnerState.lockState != _lastKeyTurnerState.lockState)
    {
        _network->publishKeyTurnerState(_keyTurnerState);
    }

    memcpy(&_lastKeyTurnerState, &_keyTurnerState, sizeof(KeyTurnerState));

    vTaskDelay( 20000 / portTICK_PERIOD_MS);
}
