#pragma once

enum class PinMode
{
    Output = 0x03,
    InputPullup = 0x05,
    InputPullDown = 0x09
};

//#define INPUT             0x01
//// Changed OUTPUT from 0x02 to behave the same as Arduino pinMode(pin,OUTPUT)
//// where you can read the state of pin even when it is set as OUTPUT
//#define OUTPUT            0x03
//#define PULLUP            0x04
//#define INPUT_PULLUP      0x05
//#define PULLDOWN          0x08
//#define INPUT_PULLDOWN    0x09
//#define OPEN_DRAIN        0x10
//#define OUTPUT_OPEN_DRAIN 0x12
//#define ANALOG            0xC0