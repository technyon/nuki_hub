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
            vTaskDelay( 200 / portTICK_PERIOD_MS);
            return;
        }
    }

    vTaskDelay( 100 / portTICK_PERIOD_MS);
    _nukiBle.requestKeyTurnerState(&_keyTurnerState);

    char str[20];
    stateToString(_keyTurnerState.lockState, str);
    Serial.print(F("Nuki lock state: "));
    Serial.println(str);

    if(_keyTurnerState.lockState != _lastKeyTurnerState.lockState)
    {
        _network->publishKeyTurnerState(str);
    }

    memcpy(&_lastKeyTurnerState, &_keyTurnerState, sizeof(KeyTurnerState));

    vTaskDelay( 20000 / portTICK_PERIOD_MS);
}

void Nuki::stateToString(LockState state, char* str)
{
    switch(state)
    {
        case LockState::uncalibrated:
            strcpy(str, "uncalibrated");
            break;
        case LockState::locked:
            strcpy(str, "locked");
            break;
        case LockState::locking:
            strcpy(str, "locking");
            break;
        case LockState::unlocked:
            strcpy(str, "unlocked");
            break;
        case LockState::unlatched:
            strcpy(str, "unlatched");
            break;
        case LockState::unlockedLnga:
            strcpy(str, "unlockedLnga");
            break;
        case LockState::unlatching:
            strcpy(str, "unlatching");
            break;
        case LockState::calibration:
            strcpy(str, "calibration");
            break;
        case LockState::bootRun:
            strcpy(str, "bootRun");
            break;
        case LockState::motorBlocked:
            strcpy(str, "motorBlocked");
            break;
        default:
            strcpy(str, "undefined");
            break;
    }
}
