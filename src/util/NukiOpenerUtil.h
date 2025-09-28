#pragma once
#include "NukiOpener.h"

class NukiOpenerUtil
{
public:
    static const NukiOpener::LockAction lockActionToEnum(const char* str); // char array at least 24 characters
    static const Nuki::AdvertisingMode advertisingModeToEnum(const char* str);
    static const uint8_t fobActionToInt(const char *str);
    static const uint8_t operatingModeToInt(const char *str);
    static const uint8_t doorbellSuppressionToInt(const char *str);
    static const uint8_t soundToInt(const char *str);
    static const NukiOpener::ButtonPressAction buttonPressActionToEnum(const char* str);
    static const Nuki::BatteryType batteryTypeToEnum(const char* str);

    static void printCommandResult(Nuki::CmdResult result);
};
