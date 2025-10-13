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

void NukiOpenerHelper::buttonPressActionToString(const NukiOpener::ButtonPressAction btnPressAction, char* str)
{
    switch (btnPressAction)
    {
    case NukiOpener::ButtonPressAction::NoAction:
        strcpy(str, "No Action");
        break;
    case NukiOpener::ButtonPressAction::ToggleRTO:
        strcpy(str, "Toggle RTO");
        break;
    case NukiOpener::ButtonPressAction::ActivateRTO:
        strcpy(str, "Activate RTO");
        break;
    case NukiOpener::ButtonPressAction::DeactivateRTO:
        strcpy(str, "Deactivate RTO");
        break;
    case NukiOpener::ButtonPressAction::ToggleCM:
        strcpy(str, "Toggle CM");
        break;
    case NukiOpener::ButtonPressAction::ActivateCM:
        strcpy(str, "Activate CM");
        break;
    case NukiOpener::ButtonPressAction::DectivateCM:
        strcpy(str, "Deactivate CM");
        break;
    case NukiOpener::ButtonPressAction::Open:
        strcpy(str, "Open");
        break;
    default:
        strcpy(str, "undefined");
        break;
    }
}

void NukiOpenerHelper::fobActionToString(const int fobact, char* str)
{
    switch (fobact)
    {
    case 0:
        strcpy(str, "No Action");
        break;
    case 1:
        strcpy(str, "Toggle RTO");
        break;
    case 2:
        strcpy(str, "Activate RTO");
        break;
    case 3:
        strcpy(str, "Deactivate RTO");
        break;
    case 7:
        strcpy(str, "Open");
        break;
    case 8:
        strcpy(str, "Ring");
        break;
    default:
        strcpy(str, "undefined");
        break;
    }
}

void NukiOpenerHelper::capabilitiesToString(const int capabilities, char* str)
{
    switch (capabilities)
    {
    case 0:
        strcpy(str, "Door opener");
        break;
    case 1:
        strcpy(str, "Both");
        break;
    case 2:
        strcpy(str, "RTO");
        break;
    default:
        strcpy(str, "undefined");
        break;
    }
}

void NukiOpenerHelper::operatingModeToString(const int opmode, char* str)
{
    switch (opmode)
    {
    case 0:
        strcpy(str, "Generic door opener");
        break;
    case 1:
        strcpy(str, "Analogue intercom");
        break;
    case 2:
        strcpy(str, "Digital intercom");
        break;
    case 3:
        strcpy(str, "Siedle");
        break;
    case 4:
        strcpy(str, "TCS");
        break;
    case 5:
        strcpy(str, "Bticino");
        break;
    case 6:
        strcpy(str, "Siedle HTS");
        break;
    case 7:
        strcpy(str, "STR");
        break;
    case 8:
        strcpy(str, "Ritto");
        break;
    case 9:
        strcpy(str, "Fermax");
        break;
    case 10:
        strcpy(str, "Comelit");
        break;
    case 11:
        strcpy(str, "Urmet BiBus");
        break;
    case 12:
        strcpy(str, "Urmet 2Voice");
        break;
    case 13:
        strcpy(str, "Golmar");
        break;
    case 14:
        strcpy(str, "SKS");
        break;
    case 15:
        strcpy(str, "Spare");
        break;
    default:
        strcpy(str, "undefined");
        break;
    }
}

void NukiOpenerHelper::doorbellSuppressionToString(const int dbsupr, char* str)
{
    switch (dbsupr)
    {
    case 0:
        strcpy(str, "Off");
        break;
    case 1:
        strcpy(str, "CM");
        break;
    case 2:
        strcpy(str, "RTO");
        break;
    case 3:
        strcpy(str, "CM & RTO");
        break;
    case 4:
        strcpy(str, "Ring");
        break;
    case 5:
        strcpy(str, "CM & Ring");
        break;
    case 6:
        strcpy(str, "RTO & Ring");
        break;
    case 7:
        strcpy(str, "CM & RTO & Ring");
        break;
    default:
        strcpy(str, "undefined");
        break;
    }
}

void NukiOpenerHelper::soundToString(const int sound, char* str)
{
    switch (sound)
    {
    case 0:
        strcpy(str, "No Sound");
        break;
    case 1:
        strcpy(str, "Sound 1");
        break;
    case 2:
        strcpy(str, "Sound 2");
        break;
    case 3:
        strcpy(str, "Sound 3");
        break;
    default:
        strcpy(str, "undefined");
        break;
    }
}


void NukiOpenerHelper::printCommandResult(Nuki::CmdResult result)
{
    char resultStr[15];
    NukiOpener::cmdResultToString(result, resultStr);
    Log->println(resultStr);
}
