#pragma once

#include "NukiConstants.h"
#include "NukiLock.h"

class NukiUtil
{
public:
    static const NukiLock::LockAction lockActionToEnum(const char* str); // char array at least 14 characters
    static const Nuki::AdvertisingMode advertisingModeToEnum(const char* str);
    static const Nuki::TimeZoneId timeZoneToEnum(const char* str);
    static const uint8_t fobActionToInt(const char *str);
    static const NukiLock::ButtonPressAction buttonPressActionToEnum(const char* str);
    static const Nuki::BatteryType batteryTypeToEnum(const char* str);
    static const NukiLock::MotorSpeed motorSpeedToEnum(const char* str);

    static void printCommandResult(Nuki::CmdResult result);
};