#include "NukiOpenerHelper.h"
#include <cstring>
#include "Logger.h"
#include "NukiOpenerUtils.h"

const NukiOpener::LockAction NukiOpenerHelper::lockActionToEnum(const char *str)
{
    if(strcmp(str, "activateRTO") == 0 || strcmp(str, "ActivateRTO") == 0)
    {
        return NukiOpener::LockAction::ActivateRTO;
    }
    else if(strcmp(str, "deactivateRTO") == 0 || strcmp(str, "DeactivateRTO") == 0)
    {
        return NukiOpener::LockAction::DeactivateRTO;
    }
    else if(strcmp(str, "electricStrikeActuation") == 0 || strcmp(str, "ElectricStrikeActuation") == 0)
    {
        return NukiOpener::LockAction::ElectricStrikeActuation;
    }
    else if(strcmp(str, "activateCM") == 0 || strcmp(str, "ActivateCM") == 0)
    {
        return NukiOpener::LockAction::ActivateCM;
    }
    else if(strcmp(str, "deactivateCM") == 0 || strcmp(str, "DeactivateCM") == 0)
    {
        return NukiOpener::LockAction::DeactivateCM;
    }
    else if(strcmp(str, "fobAction2") == 0 || strcmp(str, "FobAction2") == 0)
    {
        return NukiOpener::LockAction::FobAction2;
    }
    else if(strcmp(str, "fobAction1") == 0 || strcmp(str, "FobAction1") == 0)
    {
        return NukiOpener::LockAction::FobAction1;
    }
    else if(strcmp(str, "fobAction3") == 0 || strcmp(str, "FobAction3") == 0)
    {
        return NukiOpener::LockAction::FobAction3;
    }
    return (NukiOpener::LockAction)0xff;
}

const Nuki::AdvertisingMode NukiOpenerHelper::advertisingModeToEnum(const char *str)
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

const uint8_t NukiOpenerHelper::fobActionToInt(const char *str)
{
    if(strcmp(str, "No Action") == 0)
    {
        return 0;
    }
    else if(strcmp(str, "Toggle RTO") == 0)
    {
        return 1;
    }
    else if(strcmp(str, "Activate RTO") == 0)
    {
        return 2;
    }
    else if(strcmp(str, "Deactivate RTO") == 0)
    {
        return 3;
    }
    else if(strcmp(str, "Open") == 0)
    {
        return 7;
    }
    else if(strcmp(str, "Ring") == 0)
    {
        return 8;
    }
    return 99;
}

const uint8_t NukiOpenerHelper::operatingModeToInt(const char *str)
{
    if(strcmp(str, "Generic door opener") == 0)
    {
        return 0;
    }
    else if(strcmp(str, "Analogue intercom") == 0)
    {
        return 1;
    }
    else if(strcmp(str, "Digital intercom") == 0)
    {
        return 2;
    }
    else if(strcmp(str, "Siedle") == 0)
    {
        return 3;
    }
    else if(strcmp(str, "TCS") == 0)
    {
        return 4;
    }
    else if(strcmp(str, "Bticino") == 0)
    {
        return 5;
    }
    else if(strcmp(str, "Siedle HTS") == 0)
    {
        return 6;
    }
    else if(strcmp(str, "STR") == 0)
    {
        return 7;
    }
    else if(strcmp(str, "Ritto") == 0)
    {
        return 8;
    }
    else if(strcmp(str, "Fermax") == 0)
    {
        return 9;
    }
    else if(strcmp(str, "Comelit") == 0)
    {
        return 10;
    }
    else if(strcmp(str, "Urmet BiBus") == 0)
    {
        return 11;
    }
    else if(strcmp(str, "Urmet 2Voice") == 0)
    {
        return 12;
    }
    else if(strcmp(str, "Golmar") == 0)
    {
        return 13;
    }
    else if(strcmp(str, "SKS") == 0)
    {
        return 14;
    }
    else if(strcmp(str, "Spare") == 0)
    {
        return 15;
    }
    return 99;
}

const uint8_t NukiOpenerHelper::doorbellSuppressionToInt(const char *str)
{
    if(strcmp(str, "Off") == 0)
    {
        return 0;
    }
    else if(strcmp(str, "CM") == 0)
    {
        return 1;
    }
    else if(strcmp(str, "RTO") == 0)
    {
        return 2;
    }
    else if(strcmp(str, "CM & RTO") == 0)
    {
        return 3;
    }
    else if(strcmp(str, "Ring") == 0)
    {
        return 4;
    }
    else if(strcmp(str, "CM & Ring") == 0)
    {
        return 5;
    }
    else if(strcmp(str, "RTO & Ring") == 0)
    {
        return 6;
    }
    else if(strcmp(str, "CM & RTO & Ring") == 0)
    {
        return 7;
    }
    return 99;
}

const uint8_t NukiOpenerHelper::soundToInt(const char *str)
{
    if(strcmp(str, "No Sound") == 0)
    {
        return 0;
    }
    else if(strcmp(str, "Sound 1") == 0)
    {
        return 1;
    }
    else if(strcmp(str, "Sound 2") == 0)
    {
        return 2;
    }
    else if(strcmp(str, "Sound 3") == 0)
    {
        return 3;
    }
    return 99;
}

const NukiOpener::ButtonPressAction NukiOpenerHelper::buttonPressActionToEnum(const char* str)
{
    if(strcmp(str, "No Action") == 0)
    {
        return NukiOpener::ButtonPressAction::NoAction;
    }
    else if(strcmp(str, "Toggle RTO") == 0)
    {
        return NukiOpener::ButtonPressAction::ToggleRTO;
    }
    else if(strcmp(str, "Activate RTO") == 0)
    {
        return NukiOpener::ButtonPressAction::ActivateRTO;
    }
    else if(strcmp(str, "Deactivate RTO") == 0)
    {
        return NukiOpener::ButtonPressAction::DeactivateRTO;
    }
    else if(strcmp(str, "Toggle CM") == 0)
    {
        return NukiOpener::ButtonPressAction::ToggleCM;
    }
    else if(strcmp(str, "Activate CM") == 0)
    {
        return NukiOpener::ButtonPressAction::ActivateCM;
    }
    else if(strcmp(str, "Deactivate CM") == 0)
    {
        return NukiOpener::ButtonPressAction::DectivateCM;
    }
    else if(strcmp(str, "Open") == 0)
    {
        return NukiOpener::ButtonPressAction::Open;
    }
    return (NukiOpener::ButtonPressAction)0xff;
}

const Nuki::BatteryType NukiOpenerHelper::batteryTypeToEnum(const char* str)
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
    return (Nuki::BatteryType)0xff;
}

void NukiOpenerHelper::printCommandResult(Nuki::CmdResult result)
{
    char resultStr[15];
    NukiOpener::cmdResultToString(result, resultStr);
    Log->println(resultStr);
}
