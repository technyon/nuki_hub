#include <functional>
#include <algorithm>
#include "NukiOpenerWrapper.h"
#include "PreferencesKeys.h"
#include "MqttTopics.h"
#include "Logger.h"
#include "RestartReason.h"
#include <NukiOpenerUtils.h>
#include "Config.h"
#include "enums/NukiPinState.h"
#include "enums/NukiPinState.h"
#include "hal/wdt_hal.h"
#include <time.h>
#include "esp_sntp.h"
#include "util/NukiOpenerHelper.h"
#include "util/NukiHelper.h"

void NukiOpenerWrapper::updateConfigBool(JsonDocument& jsonResult, const char* key, const char* value, uint8_t current, Nuki::CmdResult& cmdResult, std::function<Nuki::CmdResult(bool)> setter)
{
    const int keyvalue = atoi(value);

    if(keyvalue != 0 && keyvalue != 1)
    {
        jsonResult[key] = "invalidValue";
        return;
    }
    if(current == keyvalue)
    {
        jsonResult[key] = "unchanged";
        return;
    }
    cmdResult = setter(keyvalue > 0);
}

void NukiOpenerWrapper::updateConfigInt(JsonDocument& jsonResult, const char* key, const char* value, int min, int max, int current, Nuki::CmdResult& cmdResult, std::function<Nuki::CmdResult(int)> setter)
{
    const int keyvalue = atoi(value);

    if(keyvalue < min || keyvalue > max)
    {
        jsonResult[key] = "invalidValue";
        return;
    }
    if(current == keyvalue)
    {
        jsonResult[key] = "unchanged";
        return;
    }
    cmdResult = setter(keyvalue);
}

void NukiOpenerWrapper::executeConfigUpdate(const char *value)
{
    JsonDocument jsonResult;

    if(!_nukiConfigValid)
    {
        jsonResult["general"] = "configNotReady";
        serializeJson(jsonResult, _buffer, _bufferSize);
        _network->publishConfigCommandResult(_buffer);
        return;
    }

    if(!isPinValid())
    {
        jsonResult["general"] = "noValidPinSet";
        serializeJson(jsonResult, _buffer, _bufferSize);
        _network->publishConfigCommandResult(_buffer);
        return;
    }

    JsonDocument json;
    DeserializationError jsonError = deserializeJson(json, value);

    if(jsonError)
    {
        jsonResult["general"] = "invalidJson";
        serializeJson(jsonResult, _buffer, _bufferSize);
        _network->publishConfigCommandResult(_buffer);
        return;
    }

    Nuki::CmdResult cmdResult;
    const char *basicKeys[14] = {"name", "latitude", "longitude", "pairingEnabled", "buttonEnabled", "ledFlashEnabled", "timeZoneOffset", "dstMode", "fobAction1",  "fobAction2", "fobAction3", "operatingMode", "advertisingMode", "timeZone"};
    const char *advancedKeys[22] = {"intercomID", "busModeSwitch", "shortCircuitDuration", "electricStrikeDelay", "randomElectricStrikeDelay", "electricStrikeDuration", "disableRtoAfterRing", "rtoTimeout", "doorbellSuppression", "doorbellSuppressionDuration", "soundRing", "soundOpen", "soundRto", "soundCm", "soundConfirmation", "soundLevel", "singleButtonPressAction", "doubleButtonPressAction", "batteryType", "automaticBatteryTypeDetection", "rebootNuki", "recalibrateNuki"};
    bool basicUpdated = false;
    bool advancedUpdated = false;

    for(int i=0; i < 14; i++)
    {
        if(json[basicKeys[i]].is<JsonVariantConst>())
        {
            JsonVariantConst jsonKey = json[basicKeys[i]];
            char jsonValue[16] = {0};
            const char *jsonchar = jsonValue;

            if (jsonKey.is<float>())
            {
                itoa(jsonKey, jsonValue, 10);
            }
            else if (jsonKey.is<bool>())
            {
                if (jsonKey)
                {
                    itoa(1, jsonValue, 10);
                }
                else
                {
                    itoa(0, jsonValue, 10);
                }
            }
            else if (jsonKey.is<const char*>())
            {
                jsonchar = jsonKey.as<const char*>();
            }

            if(strlen(jsonchar) == 0)
            {
                jsonResult[basicKeys[i]] = "noValueSet";
                continue;
            }

            if((int)_basicOpenerConfigAclPrefs[i] == 1)
            {
                cmdResult = Nuki::CmdResult::Error;
                int retryCount = 0;

                while(retryCount < _nrOfRetries + 1)
                {
                    if(strcmp(basicKeys[i], "name") == 0)
                    {
                        if(strlen(jsonchar) <= 32)
                        {
                            if(strcmp((const char*)_nukiConfig.name, jsonchar) == 0)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setName(std::string(jsonchar));
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "valueTooLong";
                        }
                    }
                    else if(strcmp(basicKeys[i], "latitude") == 0)
                    {
                        const float keyvalue = atof(jsonchar);

                        if(keyvalue > 0)
                        {
                            if(_nukiConfig.latitude == keyvalue)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setLatitude(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "longitude") == 0)
                    {
                        const float keyvalue = atof(jsonchar);

                        if(keyvalue > 0)
                        {
                            if(_nukiConfig.longitude == keyvalue)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setLongitude(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "pairingEnabled") == 0)
                    {
                        updateConfigBool(jsonResult, basicKeys[i], jsonchar, _nukiConfig.pairingEnabled, cmdResult, [&](bool value) { return _nukiOpener.enablePairing(value); });
                    }
                    else if(strcmp(basicKeys[i], "buttonEnabled") == 0)
                    {
                        updateConfigBool(jsonResult, basicKeys[i], jsonchar, _nukiConfig.buttonEnabled, cmdResult, [&](bool value) { return _nukiOpener.enableButton(value); });
                    }
                    else if(strcmp(basicKeys[i], "ledFlashEnabled") == 0)
                    {
                        updateConfigBool(jsonResult, basicKeys[i], jsonchar, _nukiConfig.ledFlashEnabled, cmdResult, [&](bool value) { return _nukiOpener.enableLedFlash(value); });
                    }
                    else if(strcmp(basicKeys[i], "timeZoneOffset") == 0)
                    {
                        updateConfigInt(jsonResult, basicKeys[i], jsonchar, 0, 60, _nukiConfig.timeZoneOffset, cmdResult, [&](int value) { return _nukiOpener.setTimeZoneOffset(value); });
                    }
                    else if(strcmp(basicKeys[i], "dstMode") == 0)
                    {
                        updateConfigBool(jsonResult, basicKeys[i], jsonchar, _nukiConfig.dstMode, cmdResult, [&](bool value) { return _nukiOpener.enableDst(value); });
                    }
                    else if(strcmp(basicKeys[i], "fobAction1") == 0)
                    {
                        const uint8_t fobAct1 = NukiOpenerHelper::fobActionToInt(jsonchar);

                        if(fobAct1 != 99)
                        {
                            if(_nukiConfig.fobAction1 == fobAct1)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setFobAction(1, fobAct1);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "fobAction2") == 0)
                    {
                        const uint8_t fobAct2 = NukiOpenerHelper::fobActionToInt(jsonchar);

                        if(fobAct2 != 99)
                        {
                            if(_nukiConfig.fobAction2 == fobAct2)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setFobAction(2, fobAct2);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "fobAction3") == 0)
                    {
                        const uint8_t fobAct3 = NukiOpenerHelper::fobActionToInt(jsonchar);

                        if(fobAct3 != 99)
                        {
                            if(_nukiConfig.fobAction3 == fobAct3)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setFobAction(3, fobAct3);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "operatingMode") == 0)
                    {
                        const uint8_t opmode = NukiOpenerHelper::operatingModeToInt(jsonchar);

                        if(opmode != 99)
                        {
                            if(_nukiConfig.operatingMode == opmode)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setOperatingMode(opmode);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "advertisingMode") == 0)
                    {
                        Nuki::AdvertisingMode advmode = NukiOpenerHelper::advertisingModeToEnum(jsonchar);

                        if((int)advmode != 0xff)
                        {
                            if(_nukiConfig.advertisingMode == advmode)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setAdvertisingMode(advmode);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "timeZone") == 0)
                    {
                        Nuki::TimeZoneId tzid = NukiHelper::timeZoneToEnum(jsonchar);

                        if((int)tzid != 0xff)
                        {
                            if(_nukiConfig.timeZoneId == tzid)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setTimeZoneId(tzid);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }

                    if(cmdResult != Nuki::CmdResult::Success)
                    {
                        ++retryCount;
                    }
                    else
                    {
                        break;
                    }
                }

                if(cmdResult == Nuki::CmdResult::Success)
                {
                    basicUpdated = true;
                }

                if(!jsonResult[basicKeys[i]])
                {
                    char resultStr[15] = {0};
                    NukiOpener::cmdResultToString(cmdResult, resultStr);
                    jsonResult[basicKeys[i]] = resultStr;
                }
            }
            else
            {
                jsonResult[basicKeys[i]] = "accessDenied";
            }
        }
    }

    for(int j=0; j < 22; j++)
    {
        if(json[advancedKeys[j]].is<JsonVariantConst>())
        {
            JsonVariantConst jsonKey = json[advancedKeys[j]];
            char jsonValue[16] = {0};
            const char *jsonchar = jsonValue;

            if (jsonKey.is<float>())
            {
                itoa(jsonKey, jsonValue, 10);
            }
            else if (jsonKey.is<bool>())
            {
                if (jsonKey)
                {
                    itoa(1, jsonValue, 10);
                }
                else
                {
                    itoa(0, jsonValue, 10);
                }
            }
            else if (jsonKey.is<const char*>())
            {
                jsonchar = jsonKey.as<const char*>();
            }

            if(strlen(jsonchar) == 0)
            {
                jsonResult[advancedKeys[j]] = "noValueSet";
                continue;
            }

            if((int)_advancedOpenerConfigAclPrefs[j] == 1)
            {
                cmdResult = Nuki::CmdResult::Error;
                int retryCount = 0;

                while(retryCount < _nrOfRetries + 1)
                {
                    if(strcmp(advancedKeys[j], "intercomID") == 0)
                    {
                        updateConfigInt(jsonResult, advancedKeys[j], jsonchar, 0, 65535, _nukiAdvancedConfig.intercomID, cmdResult, [&](int value) { return _nukiOpener.setIntercomID(value); });
                    }
                    else if(strcmp(advancedKeys[j], "busModeSwitch") == 0)
                    {
                        updateConfigBool(jsonResult, advancedKeys[j], jsonchar, _nukiAdvancedConfig.busModeSwitch, cmdResult, [&](bool value) { return _nukiOpener.setBusModeSwitch(value); });
                    }
                    else if(strcmp(advancedKeys[j], "shortCircuitDuration") == 0)
                    {
                        updateConfigInt(jsonResult, advancedKeys[j], jsonchar, 0, 65535, _nukiAdvancedConfig.shortCircuitDuration, cmdResult, [&](int value) { return _nukiOpener.setShortCircuitDuration(value); });
                    }
                    else if(strcmp(advancedKeys[j], "electricStrikeDelay") == 0)
                    {
                        updateConfigInt(jsonResult, advancedKeys[j], jsonchar, 0, 30000, _nukiAdvancedConfig.electricStrikeDelay, cmdResult, [&](int value) { return _nukiOpener.setElectricStrikeDelay(value); });
                    }
                    else if(strcmp(advancedKeys[j], "randomElectricStrikeDelay") == 0)
                    {
                        updateConfigBool(jsonResult, advancedKeys[j], jsonchar, _nukiAdvancedConfig.randomElectricStrikeDelay, cmdResult, [&](bool value) { return _nukiOpener.enableRandomElectricStrikeDelay(value); });
                    }
                    else if(strcmp(advancedKeys[j], "electricStrikeDuration") == 0)
                    {
                        updateConfigInt(jsonResult, advancedKeys[j], jsonchar, 1000, 30000, _nukiAdvancedConfig.electricStrikeDuration, cmdResult, [&](int value) { return _nukiOpener.setElectricStrikeDuration(value); });
                    }
                    else if(strcmp(advancedKeys[j], "disableRtoAfterRing") == 0)
                    {
                        updateConfigBool(jsonResult, advancedKeys[j], jsonchar, _nukiAdvancedConfig.disableRtoAfterRing, cmdResult, [&](bool value) { return _nukiOpener.disableRtoAfterRing(value); });
                    }
                    else if(strcmp(advancedKeys[j], "rtoTimeout") == 0)
                    {
                        updateConfigInt(jsonResult, advancedKeys[j], jsonchar, 5, 60, _nukiAdvancedConfig.rtoTimeout, cmdResult, [&](int value) { return _nukiOpener.setRtoTimeout(value); });
                    }
                    else if(strcmp(advancedKeys[j], "doorbellSuppression") == 0)
                    {
                        const uint8_t dbsupr = NukiOpenerHelper::doorbellSuppressionToInt(jsonchar);

                        if(dbsupr != 99)
                        {
                            if(_nukiAdvancedConfig.doorbellSuppression == dbsupr)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setDoorbellSuppression(dbsupr);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "doorbellSuppressionDuration") == 0)
                    {
                        updateConfigInt(jsonResult, advancedKeys[j], jsonchar, 500, 10000, _nukiAdvancedConfig.doorbellSuppressionDuration, cmdResult, [&](int value) { return _nukiOpener.setDoorbellSuppressionDuration(value); });
                    }
                    else if(strcmp(advancedKeys[j], "soundRing") == 0)
                    {
                        const uint8_t sound = NukiOpenerHelper::soundToInt(jsonchar);

                        if(sound != 99)
                        {
                            if(_nukiAdvancedConfig.soundRing == sound)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setSoundRing(sound);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "soundOpen") == 0)
                    {
                        const uint8_t sound = NukiOpenerHelper::soundToInt(jsonchar);

                        if(sound != 99)
                        {
                            if(_nukiAdvancedConfig.soundOpen == sound)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setSoundOpen(sound);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "soundRto") == 0)
                    {
                        const uint8_t sound = NukiOpenerHelper::soundToInt(jsonchar);

                        if(sound != 99)
                        {
                            if(_nukiAdvancedConfig.soundRto == sound)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setSoundRto(sound);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "soundCm") == 0)
                    {
                        const uint8_t sound = NukiOpenerHelper::soundToInt(jsonchar);

                        if(sound != 99)
                        {
                            if(_nukiAdvancedConfig.soundCm == sound)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setSoundCm(sound);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "soundConfirmation") == 0)
                    {
                        updateConfigBool(jsonResult, advancedKeys[j], jsonchar, _nukiAdvancedConfig.soundConfirmation, cmdResult, [&](bool value) { return _nukiOpener.enableSoundConfirmation(value); });
                    }
                    else if(strcmp(advancedKeys[j], "soundLevel") == 0)
                    {
                        updateConfigInt(jsonResult, advancedKeys[j], jsonchar, 0, 255, _nukiAdvancedConfig.soundLevel, cmdResult, [&](int value) { return _nukiOpener.setSoundLevel(value); });
                    }
                    else if(strcmp(advancedKeys[j], "singleButtonPressAction") == 0)
                    {
                        NukiOpener::ButtonPressAction sbpa = NukiOpenerHelper::buttonPressActionToEnum(jsonchar);

                        if((int)sbpa != 0xff)
                        {
                            if(_nukiAdvancedConfig.singleButtonPressAction == sbpa)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setSingleButtonPressAction(sbpa);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "doubleButtonPressAction") == 0)
                    {
                        NukiOpener::ButtonPressAction dbpa = NukiOpenerHelper::buttonPressActionToEnum(jsonchar);

                        if((int)dbpa != 0xff)
                        {
                            if(_nukiAdvancedConfig.doubleButtonPressAction == dbpa)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setDoubleButtonPressAction(dbpa);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "batteryType") == 0)
                    {
                        Nuki::BatteryType battype = NukiOpenerHelper::batteryTypeToEnum(jsonchar);

                        if((int)battype != 0xff)
                        {
                            if(_nukiAdvancedConfig.batteryType == battype)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setBatteryType(battype);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "automaticBatteryTypeDetection") == 0)
                    {
                        updateConfigBool(jsonResult, advancedKeys[j], jsonchar, _nukiAdvancedConfig.automaticBatteryTypeDetection, cmdResult, [&](bool value) { return _nukiOpener.enableAutoBatteryTypeDetection(value); });
                    }
                    else if(strcmp(advancedKeys[j], "rebootNuki") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 1)
                        {
                            cmdResult = _nukiOpener.requestReboot();
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "recalibrateNuki") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 1)
                        {
                            cmdResult = _nukiOpener.requestCalibration();
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }

                    if(cmdResult != Nuki::CmdResult::Success)
                    {
                        ++retryCount;
                    }
                    else
                    {
                        break;
                    }
                }

                if(cmdResult == Nuki::CmdResult::Success)
                {
                    advancedUpdated = true;
                }

                if(!jsonResult[advancedKeys[j]])
                {
                    char resultStr[15] = {0};
                    NukiOpener::cmdResultToString(cmdResult, resultStr);
                    jsonResult[advancedKeys[j]] = resultStr;
                }
            }
            else
            {
                jsonResult[advancedKeys[j]] = "accessDenied";
            }
        }
    }

    if(basicUpdated || advancedUpdated)
    {
        jsonResult["general"] = "success";
    }
    else
    {
        jsonResult["general"] = "noChange";
    }

    _nextConfigUpdateTs = espMillis() + 300;

    serializeJson(jsonResult, _buffer, _bufferSize);
    _network->publishConfigCommandResult(_buffer);

    return;
}

