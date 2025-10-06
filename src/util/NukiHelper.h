#pragma once

#include "NukiConstants.h"
#include "NukiLock.h"

class NukiHelper
{
public:
    static const NukiLock::LockAction lockActionToEnum(const char* str); // char array at least 14 characters
    static const Nuki::AdvertisingMode advertisingModeToEnum(const char* str);
    static const Nuki::TimeZoneId timeZoneToEnum(const char* str);
    static const uint8_t fobActionToInt(const char *str);
    static const NukiLock::ButtonPressAction buttonPressActionToEnum(const char* str);
    static const Nuki::BatteryType batteryTypeToEnum(const char* str);
    static const NukiLock::MotorSpeed motorSpeedToEnum(const char* str);

    static void buttonPressActionToString(const NukiLock::ButtonPressAction btnPressAction, char* str);
    static void motorSpeedToString(const NukiLock::MotorSpeed speed, char* str);
    static void homeKitStatusToString(const int hkstatus, char* str);
    static void fobActionToString(const int fobact, char* str);

    static void printCommandResult(Nuki::CmdResult result);
};