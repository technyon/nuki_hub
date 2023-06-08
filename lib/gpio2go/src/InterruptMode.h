#pragma once

enum class InterruptMode
{
    Rising = 0x01,
    Falling = 0x02,
    Change = 0x03,
    OnLow = 0x04,
    OnHigh = 0x05
};