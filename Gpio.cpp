#include <esp32-hal.h>
#include "Gpio.h"
#include "Arduino.h"
#include "Pins.h"
#include "Logger.h"

Gpio* Gpio::_inst = nullptr;
NukiWrapper* Gpio::_nuki = nullptr;
unsigned long Gpio::_lockedTs = 0;
const uint Gpio::_debounceTime = 1000;

void Gpio::init(NukiWrapper* nuki)
{
    _nuki = nuki;

    pinMode(TRIGGER_LOCK_PIN, INPUT_PULLUP);
    pinMode(TRIGGER_UNLOCK_PIN, INPUT_PULLUP);
    pinMode(TRIGGER_UNLATCH_PIN, INPUT_PULLUP);

    attachInterrupt(TRIGGER_LOCK_PIN, isrLock, FALLING);
    attachInterrupt(TRIGGER_UNLOCK_PIN, isrUnlock, FALLING);
    attachInterrupt(TRIGGER_UNLATCH_PIN, isrUnlatch, FALLING);
}

void Gpio::isrLock()
{
    if(millis() < _lockedTs) return;
    _nuki->lock();
    _lockedTs = millis() + _debounceTime;
    Log->println(F("Lock via GPIO"));;
}

void Gpio::isrUnlock()
{
    if(millis() < _lockedTs) return;
    _nuki->unlock();
    _lockedTs = millis() + _debounceTime;
    Log->println(F("Unlock via GPIO"));;
}

void Gpio::isrUnlatch()
{
    if(millis() < _lockedTs) return;
    _nuki->unlatch();
    _lockedTs = millis() + _debounceTime;
    Log->println(F("Unlatch via GPIO"));;
}
