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
    NukiBle _nukiBle;
    Network* _network;

    KeyTurnerState _keyTurnerState;

    bool _paired = false;
};
