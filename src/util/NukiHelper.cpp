#include "NukiHelper.h"
#include <cstring>
#include "Logger.h"

const NukiLock::LockAction NukiHelper::lockActionToEnum(const char *str)
{
    if(strcmp(str, "unlock") == 0 || strcmp(str, "Unlock") == 0)
    {
        return NukiLock::LockAction::Unlock;
    }
    else if(strcmp(str, "lock") == 0 || strcmp(str, "Lock") == 0)
    {
        return NukiLock::LockAction::Lock;
    }
    else if(strcmp(str, "unlatch") == 0 || strcmp(str, "Unlatch") == 0)
    {
        return NukiLock::LockAction::Unlatch;
    }
    else if(strcmp(str, "lockNgo") == 0 || strcmp(str, "LockNgo") == 0)
    {
        return NukiLock::LockAction::LockNgo;
    }
    else if(strcmp(str, "lockNgoUnlatch") == 0 || strcmp(str, "LockNgoUnlatch") == 0)
    {
        return NukiLock::LockAction::LockNgoUnlatch;
    }
    else if(strcmp(str, "fullLock") == 0 || strcmp(str, "FullLock") == 0)
    {
        return NukiLock::LockAction::FullLock;
    }
    else if(strcmp(str, "fobAction2") == 0 || strcmp(str, "FobAction2") == 0)
    {
        return NukiLock::LockAction::FobAction2;
    }
    else if(strcmp(str, "fobAction1") == 0 || strcmp(str, "FobAction1") == 0)
    {
        return NukiLock::LockAction::FobAction1;
    }
    else if(strcmp(str, "fobAction3") == 0 || strcmp(str, "FobAction3") == 0)
    {
        return NukiLock::LockAction::FobAction3;
    }
    return (NukiLock::LockAction)0xff;
}

const Nuki::AdvertisingMode NukiHelper::advertisingModeToEnum(const char *str)
{
    if(strcmp(str, "Automatic") == 0)
    {
        return Nuki::AdvertisingMode::Automatic;
    }
    else if(strcmp(str, "Normal") == 0)
    {
        return Nuki::AdvertisingMode::Normal;
    }
    else if(strcmp(str, "Slow") == 0)
    {
        return Nuki::AdvertisingMode::Slow;
    }
    else if(strcmp(str, "Slowest") == 0)
    {
        return Nuki::AdvertisingMode::Slowest;
    }
    return (Nuki::AdvertisingMode)0xff;
}

const Nuki::TimeZoneId NukiHelper::timeZoneToEnum(const char *str)
{
    if(strcmp(str, "Africa/Cairo") == 0)
    {
        return Nuki::TimeZoneId::Africa_Cairo;
    }
    else if(strcmp(str, "Africa/Lagos") == 0)
    {
        return Nuki::TimeZoneId::Africa_Lagos;
    }
    else if(strcmp(str, "Africa/Maputo") == 0)
    {
        return Nuki::TimeZoneId::Africa_Maputo;
    }
    else if(strcmp(str, "Africa/Nairobi") == 0)
    {
        return Nuki::TimeZoneId::Africa_Nairobi;
    }
    else if(strcmp(str, "America/Anchorage") == 0)
    {
        return Nuki::TimeZoneId::America_Anchorage;
    }
    else if(strcmp(str, "America/Argentina/Buenos_Aires") == 0)
    {
        return Nuki::TimeZoneId::America_Argentina_Buenos_Aires;
    }
    else if(strcmp(str, "America/Chicago") == 0)
    {
        return Nuki::TimeZoneId::America_Chicago;
    }
    else if(strcmp(str, "America/Denver") == 0)
    {
        return Nuki::TimeZoneId::America_Denver;
    }
    else if(strcmp(str, "America/Halifax") == 0)
    {
        return Nuki::TimeZoneId::America_Halifax;
    }
    else if(strcmp(str, "America/Los_Angeles") == 0)
    {
        return Nuki::TimeZoneId::America_Los_Angeles;
    }
    else if(strcmp(str, "America/Manaus") == 0)
    {
        return Nuki::TimeZoneId::America_Manaus;
    }
    else if(strcmp(str, "America/Mexico_City") == 0)
    {
        return Nuki::TimeZoneId::America_Mexico_City;
    }
    else if(strcmp(str, "America/New_York") == 0)
    {
        return Nuki::TimeZoneId::America_New_York;
    }
    else if(strcmp(str, "America/Phoenix") == 0)
    {
        return Nuki::TimeZoneId::America_Phoenix;
    }
    else if(strcmp(str, "America/Regina") == 0)
    {
        return Nuki::TimeZoneId::America_Regina;
    }
    else if(strcmp(str, "America/Santiago") == 0)
    {
        return Nuki::TimeZoneId::America_Santiago;
    }
    else if(strcmp(str, "America/Sao_Paulo") == 0)
    {
        return Nuki::TimeZoneId::America_Sao_Paulo;
    }
    else if(strcmp(str, "America/St_Johns") == 0)
    {
        return Nuki::TimeZoneId::America_St_Johns;
    }
    else if(strcmp(str, "Asia/Bangkok") == 0)
    {
        return Nuki::TimeZoneId::Asia_Bangkok;
    }
    else if(strcmp(str, "Asia/Dubai") == 0)
    {
        return Nuki::TimeZoneId::Asia_Dubai;
    }
    else if(strcmp(str, "Asia/Hong_Kong") == 0)
    {
        return Nuki::TimeZoneId::Asia_Hong_Kong;
    }
    else if(strcmp(str, "Asia/Jerusalem") == 0)
    {
        return Nuki::TimeZoneId::Asia_Jerusalem;
    }
    else if(strcmp(str, "Asia/Karachi") == 0)
    {
        return Nuki::TimeZoneId::Asia_Karachi;
    }
    else if(strcmp(str, "Asia/Kathmandu") == 0)
    {
        return Nuki::TimeZoneId::Asia_Kathmandu;
    }
    else if(strcmp(str, "Asia/Kolkata") == 0)
    {
        return Nuki::TimeZoneId::Asia_Kolkata;
    }
    else if(strcmp(str, "Asia/Riyadh") == 0)
    {
        return Nuki::TimeZoneId::Asia_Riyadh;
    }
    else if(strcmp(str, "Asia/Seoul") == 0)
    {
        return Nuki::TimeZoneId::Asia_Seoul;
    }
    else if(strcmp(str, "Asia/Shanghai") == 0)
    {
        return Nuki::TimeZoneId::Asia_Shanghai;
    }
    else if(strcmp(str, "Asia/Tehran") == 0)
    {
        return Nuki::TimeZoneId::Asia_Tehran;
    }
    else if(strcmp(str, "Asia/Tokyo") == 0)
    {
        return Nuki::TimeZoneId::Asia_Tokyo;
    }
    else if(strcmp(str, "Asia/Yangon") == 0)
    {
        return Nuki::TimeZoneId::Asia_Yangon;
    }
    else if(strcmp(str, "Australia/Adelaide") == 0)
    {
        return Nuki::TimeZoneId::Australia_Adelaide;
    }
    else if(strcmp(str, "Australia/Brisbane") == 0)
    {
        return Nuki::TimeZoneId::Australia_Brisbane;
    }
    else if(strcmp(str, "Australia/Darwin") == 0)
    {
        return Nuki::TimeZoneId::Australia_Darwin;
    }
    else if(strcmp(str, "Australia/Hobart") == 0)
    {
        return Nuki::TimeZoneId::Australia_Hobart;
    }
    else if(strcmp(str, "Australia/Perth") == 0)
    {
        return Nuki::TimeZoneId::Australia_Perth;
    }
    else if(strcmp(str, "Australia/Sydney") == 0)
    {
        return Nuki::TimeZoneId::Australia_Sydney;
    }
    else if(strcmp(str, "Europe/Berlin") == 0)
    {
        return Nuki::TimeZoneId::Europe_Berlin;
    }
    else if(strcmp(str, "Europe/Helsinki") == 0)
    {
        return Nuki::TimeZoneId::Europe_Helsinki;
    }
    else if(strcmp(str, "Europe/Istanbul") == 0)
    {
        return Nuki::TimeZoneId::Europe_Istanbul;
    }
    else if(strcmp(str, "Europe/London") == 0)
    {
        return Nuki::TimeZoneId::Europe_London;
    }
    else if(strcmp(str, "Europe/Moscow") == 0)
    {
        return Nuki::TimeZoneId::Europe_Moscow;
    }
    else if(strcmp(str, "Pacific/Auckland") == 0)
    {
        return Nuki::TimeZoneId::Pacific_Auckland;
    }
    else if(strcmp(str, "Pacific/Guam") == 0)
    {
        return Nuki::TimeZoneId::Pacific_Guam;
    }
    else if(strcmp(str, "Pacific/Honolulu") == 0)
    {
        return Nuki::TimeZoneId::Pacific_Honolulu;
    }
    else if(strcmp(str, "Pacific/Pago_Pago") == 0)
    {
        return Nuki::TimeZoneId::Pacific_Pago_Pago;
    }
    else if(strcmp(str, "None") == 0)
    {
        return Nuki::TimeZoneId::None;
    }
    return (Nuki::TimeZoneId)0xff;
}

const uint8_t NukiHelper::fobActionToInt(const char *str)
{
    if(strcmp(str, "No Action") == 0)
    {
        return 0;
    }
    else if(strcmp(str, "Unlock") == 0)
    {
        return 1;
    }
    else if(strcmp(str, "Lock") == 0)
    {
        return 2;
    }
    else if(strcmp(str, "Lock n Go") == 0)
    {
        return 3;
    }
    else if(strcmp(str, "Intelligent") == 0)
    {
        return 4;
    }
    return 99;
}

const NukiLock::ButtonPressAction NukiHelper::buttonPressActionToEnum(const char* str)
{
    if(strcmp(str, "No Action") == 0)
    {
        return NukiLock::ButtonPressAction::NoAction;
    }
    else if(strcmp(str, "Intelligent") == 0)
    {
        return NukiLock::ButtonPressAction::Intelligent;
    }
    else if(strcmp(str, "Unlock") == 0)
    {
        return NukiLock::ButtonPressAction::Unlock;
    }
    else if(strcmp(str, "Lock") == 0)
    {
        return NukiLock::ButtonPressAction::Lock;
    }
    else if(strcmp(str, "Unlatch") == 0)
    {
        return NukiLock::ButtonPressAction::Unlatch;
    }
    else if(strcmp(str, "Lock n Go") == 0)
    {
        return NukiLock::ButtonPressAction::LockNgo;
    }
    else if(strcmp(str, "Show Status") == 0)
    {
        return NukiLock::ButtonPressAction::ShowStatus;
    }
    return (NukiLock::ButtonPressAction)0xff;
}

const Nuki::BatteryType NukiHelper::batteryTypeToEnum(const char* str)
{
    if(strcmp(str, "Alkali") == 0)
    {
        return Nuki::BatteryType::Alkali;
    }
    else if(strcmp(str, "Accumulators") == 0)
    {
        return Nuki::BatteryType::Accumulators;
    }
    else if(strcmp(str, "Lithium") == 0)
    {
        return Nuki::BatteryType::Lithium;
    }
    else if(strcmp(str, "No Warnings") == 0)
    {
        return Nuki::BatteryType::NoWarnings;
    }
    return (Nuki::BatteryType)0xff;
}

const NukiLock::MotorSpeed NukiHelper::motorSpeedToEnum(const char* str)
{
    if(strcmp(str, "Standard") == 0)
    {
        return NukiLock::MotorSpeed::Standard;
    }
    else if(strcmp(str, "Insane") == 0)
    {
        return NukiLock::MotorSpeed::Insane;
    }
    else if(strcmp(str, "Gentle") == 0)
    {
        return NukiLock::MotorSpeed::Gentle;
    }
    return (NukiLock::MotorSpeed)0xff;
}

void NukiHelper::buttonPressActionToString(const NukiLock::ButtonPressAction btnPressAction, char* str)
{
    switch (btnPressAction)
    {
    case NukiLock::ButtonPressAction::NoAction:
        strcpy(str, "No Action");
        break;
    case NukiLock::ButtonPressAction::Intelligent:
        strcpy(str, "Intelligent");
        break;
    case NukiLock::ButtonPressAction::Unlock:
        strcpy(str, "Unlock");
        break;
    case NukiLock::ButtonPressAction::Lock:
        strcpy(str, "Lock");
        break;
    case NukiLock::ButtonPressAction::Unlatch:
        strcpy(str, "Unlatch");
        break;
    case NukiLock::ButtonPressAction::LockNgo:
        strcpy(str, "Lock n Go");
        break;
    case NukiLock::ButtonPressAction::ShowStatus:
        strcpy(str, "Show Status");
        break;
    default:
        strcpy(str, "undefined");
        break;
    }
}

void NukiHelper::motorSpeedToString(const NukiLock::MotorSpeed speed, char* str)
{
    switch (speed)
    {
    case NukiLock::MotorSpeed::Standard:
        strcpy(str, "Standard");
        break;
    case NukiLock::MotorSpeed::Insane:
        strcpy(str, "Insane");
        break;
    case NukiLock::MotorSpeed::Gentle:
        strcpy(str, "Gentle");
        break;
    default:
        strcpy(str, "undefined");
        break;
    }
}

void NukiHelper::homeKitStatusToString(const int hkstatus, char* str)
{
    switch (hkstatus)
    {
    case 0:
        strcpy(str, "Not Available");
        break;
    case 1:
        strcpy(str, "Disabled");
        break;
    case 2:
        strcpy(str, "Enabled");
        break;
    case 3:
        strcpy(str, "Enabled & Paired");
        break;
    default:
        strcpy(str, "undefined");
        break;
    }
}

void NukiHelper::fobActionToString(const int fobact, char* str)
{
    switch (fobact)
    {
    case 0:
        strcpy(str, "No Action");
        break;
    case 1:
        strcpy(str, "Unlock");
        break;
    case 2:
        strcpy(str, "Lock");
        break;
    case 3:
        strcpy(str, "Lock n Go");
        break;
    case 4:
        strcpy(str, "Intelligent");
        break;
    default:
        strcpy(str, "undefined");
        break;
    }
}

void NukiHelper::weekdaysToJsonArray(int weekdaysInt, JsonArray& weekdays)
{
    if ((weekdaysInt & 0b01000000) > 0)
    {
        weekdays.add("mon");
    }
    if ((weekdaysInt & 0b00100000) > 0)
    {
        weekdays.add("tue");
    }
    if ((weekdaysInt & 0b00010000) > 0)
    {
        weekdays.add("wed");
    }
    if ((weekdaysInt & 0b00001000) > 0)
    {
        weekdays.add("thu");
    }
    if ((weekdaysInt & 0b00000100) > 0)
    {
        weekdays.add("fri");
    }
    if ((weekdaysInt & 0b00000010) > 0)
    {
        weekdays.add("sat");
    }
    if ((weekdaysInt & 0b00000001) > 0)
    {
        weekdays.add("sun");
    }
}


void NukiHelper::printCommandResult(Nuki::CmdResult result)
{
    char resultStr[15];
    NukiLock::cmdResultToString(result, resultStr);
    Log->println(resultStr);
}

