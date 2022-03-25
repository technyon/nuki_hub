#pragma once

#include "NukiBle.h"
#include "NukiConstants.h"
#include "Network.h"

class Nuki
{
public:
    Nuki(const std::string& name, uint32_t id, Network* network);

    void initialize();
    void update();

private:
    void stateToString(LockState state, char* str); // char array at least 14 characters

    NukiBle _nukiBle;
    Network* _network;

    KeyTurnerState _lastKeyTurnerState;
    KeyTurnerState _keyTurnerState;

    bool _paired = false;
};
