#pragma once


#include "NukiWrapper.h"

class Gpio
{
public:
    Gpio() = delete;
    static void init(NukiWrapper* nuki);

private:
    static const uint _debounceTime;

    static void IRAM_ATTR isrLock();
    static void IRAM_ATTR isrUnlock();
    static void IRAM_ATTR isrUnlatch();

    static Gpio* _inst;
    static NukiWrapper* _nuki;
    static unsigned long _lockedTs;
};
