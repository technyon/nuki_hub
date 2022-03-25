#pragma once

#include "NukiBle.h"
#include "NukiConstants.h"

class Nuki
{
public:
    Nuki(const std::string& name, uint32_t id);

    void initialize();
    void update();

private:
    NukiBle _nukiBle;
    bool _paired = false;

    KeyTurnerState _keyTurnerState;

};
