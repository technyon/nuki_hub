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

void NukiOpenerWrapper::onConfigUpdateReceived(const char *value)
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
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiConfig.pairingEnabled == keyvalue)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.enablePairing((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "buttonEnabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiConfig.buttonEnabled == keyvalue)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.enableButton((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "ledFlashEnabled") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiConfig.ledFlashEnabled == keyvalue)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.enableLedFlash((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "timeZoneOffset") == 0)
                    {
                        const int16_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= 0 && keyvalue <= 60)
                        {
                            if(_nukiConfig.timeZoneOffset == keyvalue)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setTimeZoneOffset(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "dstMode") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiConfig.dstMode == keyvalue)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.enableDst((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
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
                        const uint16_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= 0)
                        {
                            if(_nukiAdvancedConfig.intercomID == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setIntercomID(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "busModeSwitch") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.busModeSwitch == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setBusModeSwitch((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "shortCircuitDuration") == 0)
                    {
                        const uint16_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= 0)
                        {
                            if(_nukiAdvancedConfig.shortCircuitDuration == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setShortCircuitDuration(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "electricStrikeDelay") == 0)
                    {
                        const uint16_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= 0 && keyvalue <= 30000)
                        {
                            if(_nukiAdvancedConfig.electricStrikeDelay == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setElectricStrikeDelay(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "randomElectricStrikeDelay") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.randomElectricStrikeDelay == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.enableRandomElectricStrikeDelay((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "electricStrikeDuration") == 0)
                    {
                        const uint16_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= 1000 && keyvalue <= 30000)
                        {
                            if(_nukiAdvancedConfig.electricStrikeDuration == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setElectricStrikeDuration(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "disableRtoAfterRing") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.disableRtoAfterRing == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.disableRtoAfterRing((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "rtoTimeout") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= 5 && keyvalue <= 60)
                        {
                            if(_nukiAdvancedConfig.rtoTimeout == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setRtoTimeout(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
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
                        const uint16_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= 500 && keyvalue <= 10000)
                        {
                            if(_nukiAdvancedConfig.doorbellSuppressionDuration == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setDoorbellSuppressionDuration(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
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
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.soundConfirmation == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.enableSoundConfirmation((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "soundLevel") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue >= 0 && keyvalue <= 255)
                        {
                            if(_nukiAdvancedConfig.soundLevel == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.setSoundLevel(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
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
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 0 || keyvalue == 1)
                        {
                            if(_nukiAdvancedConfig.automaticBatteryTypeDetection == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiOpener.enableAutoBatteryTypeDetection((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
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

void NukiOpenerWrapper::executeKeypadJsonCommand(const char *value)
{
    if(!isPinValid())
    {
        _network->publishKeypadJsonCommandResult("noValidPinSet");
        return;
    }

    if(!_preferences->getBool(preference_keypad_control_enabled, false))
    {
        _network->publishKeypadJsonCommandResult("keypadControlDisabled");
        return;
    }

    if(!hasKeypad())
    {
        if(_nukiConfigValid)
        {
            _network->publishKeypadJsonCommandResult("keypadNotAvailable");
            return;
        }

        _network->publishKeypadJsonCommandResult("configNotReady");
        return;
    }

    if(!_keypadEnabled)
    {
        _network->publishKeypadJsonCommandResult("keypadDisabled");
        return;
    }

    JsonDocument json;
    DeserializationError jsonError = deserializeJson(json, value);

    if(jsonError)
    {
        _network->publishKeypadJsonCommandResult("invalidJson");
        return;
    }

    char oldName[21];
    const char *action = json["action"].as<const char*>();
    uint16_t codeId = json["codeId"].as<unsigned int>();
    uint32_t code;
    uint8_t enabled;
    uint8_t timeLimited;
    String name;
    String allowedFrom;
    String allowedUntil;
    String allowedWeekdays;
    String allowedFromTime;
    String allowedUntilTime;

    if(json["code"].is<JsonVariant>())
    {
        code = json["code"].as<unsigned int>();
    }
    else
    {
        code = 12;
    }

    if(json["enabled"].is<JsonVariant>())
    {
        enabled = json["enabled"].as<unsigned int>();
    }
    else
    {
        enabled = 2;
    }

    if(json["timeLimited"].is<JsonVariant>())
    {
        timeLimited = json["timeLimited"].as<unsigned int>();
    }
    else
    {
        timeLimited = 2;
    }

    if(json["name"].is<JsonVariant>())
    {
        name = json["name"].as<String>();
    }
    if(json["allowedFrom"].is<JsonVariant>())
    {
        allowedFrom = json["allowedFrom"].as<String>();
    }
    if(json["allowedUntil"].is<JsonVariant>())
    {
        allowedUntil = json["allowedUntil"].as<String>();
    }
    if(json["allowedWeekdays"].is<JsonVariant>())
    {
        allowedWeekdays = json["allowedWeekdays"].as<String>();
    }
    if(json["allowedFromTime"].is<JsonVariant>())
    {
        allowedFromTime = json["allowedFromTime"].as<String>();
    }
    if(json["allowedUntilTime"].is<JsonVariant>())
    {
        allowedUntilTime = json["allowedUntilTime"].as<String>();
    }

    if(action)
    {
        bool idExists = false;

        if(codeId)
        {
            idExists = std::find(_keypadCodeIds.begin(), _keypadCodeIds.end(), codeId) != _keypadCodeIds.end();
        }

        if(strcmp(action, "check") == 0)
        {
            if(!_preferences->getBool(preference_keypad_check_code_enabled, false))
            {
                _network->publishKeypadJsonCommandResult("checkingKeypadCodesDisabled");
                return;
            }

            if((pow(_invalidCount, 5) + _lastCodeCheck) > espMillis())
            {
                _network->publishKeypadJsonCommandResult("checkingCodesBlockedTooManyInvalid");
                _lastCodeCheck = espMillis();
                return;
            }

            _lastCodeCheck = espMillis();

            if(idExists)
            {
                auto it1 = std::find(_keypadCodeIds.begin(), _keypadCodeIds.end(), codeId);
                int index = it1 - _keypadCodeIds.begin();
                Log->print("Check keypad code: ");

                if(code == _keypadCodes[index])
                {
                    _invalidCount = 0;
                    _network->publishKeypadJsonCommandResult("codeValid");
                    Log->println("Valid");
                    return;
                }
                else
                {
                    _invalidCount++;
                    _network->publishKeypadJsonCommandResult("codeInvalid");
                    Log->print("Invalid\nInvalid count: ");
                    Log->println(_invalidCount);
                    return;
                }
            }
            else
            {
                _invalidCount++;
                _network->publishKeypadJsonCommandResult("noExistingCodeIdSet");
                Log->print("Invalid count: ");
                Log->println(_invalidCount);
                return;
            }
        }
        else
        {
            Nuki::CmdResult result = (Nuki::CmdResult)-1;
            int retryCount = 0;

            while(retryCount < _nrOfRetries + 1)
            {
                if(strcmp(action, "delete") == 0)
                {
                    if(idExists)
                    {
                        result = _nukiRetryHandler->retryComm([&]()
                        {
                            return _nukiOpener.deleteKeypadEntry(codeId);
                        });
                        if(result == Nuki::CmdResult::Success)
                        {
                            _keypadDeleted.push_back(std::make_pair(codeId, espMillis()));
                            auto it = _keypadEntriesCache.begin();
                            while(it != _keypadEntriesCache.end())
                            {
                                if(it->codeId == codeId)
                                {
                                    it = _keypadEntriesCache.erase(it);
                                }
                                else
                                {
                                    ++it;
                                }
                            }
                            _keypadCacheDirty = true;
                        }
                        Log->print("Delete keypad code: ");
                        NukiOpenerHelper::printCommandResult(result);
                    }
                    else
                    {
                        _network->publishKeypadJsonCommandResult("noExistingCodeIdSet");
                        return;
                    }
                }
                else if(strcmp(action, "add") == 0 || strcmp(action, "update") == 0)
                {
                    if(name.length() < 1)
                    {
                        if (strcmp(action, "update") != 0)
                        {
                            _network->publishKeypadJsonCommandResult("noNameSet");
                            return;
                        }
                    }

                    if(code != 12)
                    {
                        String codeStr = json["code"].as<String>();
                        bool codeValid = code > 100000 && code < 1000000 && (codeStr.indexOf('0') == -1);

                        if (!codeValid)
                        {
                            _network->publishKeypadJsonCommandResult("noValidCodeSet");
                            return;
                        }
                    }
                    else if (strcmp(action, "update") != 0)
                    {
                        _network->publishKeypadJsonCommandResult("noCodeSet");
                        return;
                    }

                    unsigned int allowedFromAr[6];
                    unsigned int allowedUntilAr[6];
                    unsigned int allowedFromTimeAr[2];
                    unsigned int allowedUntilTimeAr[2];
                    uint8_t allowedWeekdaysInt = 0;

                    if(timeLimited == 1)
                    {
                        if(allowedFrom.length() > 0)
                        {
                            if(allowedFrom.length() == 19)
                            {
                                allowedFromAr[0] = (uint16_t)allowedFrom.substring(0, 4).toInt();
                                allowedFromAr[1] = (uint8_t)allowedFrom.substring(5, 7).toInt();
                                allowedFromAr[2] = (uint8_t)allowedFrom.substring(8, 10).toInt();
                                allowedFromAr[3] = (uint8_t)allowedFrom.substring(11, 13).toInt();
                                allowedFromAr[4] = (uint8_t)allowedFrom.substring(14, 16).toInt();
                                allowedFromAr[5] = (uint8_t)allowedFrom.substring(17, 19).toInt();

                                if(allowedFromAr[0] < 2000 || allowedFromAr[0] > 3000 || allowedFromAr[1] < 1 || allowedFromAr[1] > 12 || allowedFromAr[2] < 1 || allowedFromAr[2] > 31 || allowedFromAr[3] < 0 || allowedFromAr[3] > 23 || allowedFromAr[4] < 0 || allowedFromAr[4] > 59 || allowedFromAr[5] < 0 || allowedFromAr[5] > 59)
                                {
                                    _network->publishKeypadJsonCommandResult("invalidAllowedFrom");
                                    return;
                                }
                            }
                            else
                            {
                                _network->publishKeypadJsonCommandResult("invalidAllowedFrom");
                                return;
                            }
                        }

                        if(allowedUntil.length() > 0)
                        {
                            if(allowedUntil.length() == 19)
                            {
                                allowedUntilAr[0] = (uint16_t)allowedUntil.substring(0, 4).toInt();
                                allowedUntilAr[1] = (uint8_t)allowedUntil.substring(5, 7).toInt();
                                allowedUntilAr[2] = (uint8_t)allowedUntil.substring(8, 10).toInt();
                                allowedUntilAr[3] = (uint8_t)allowedUntil.substring(11, 13).toInt();
                                allowedUntilAr[4] = (uint8_t)allowedUntil.substring(14, 16).toInt();
                                allowedUntilAr[5] = (uint8_t)allowedUntil.substring(17, 19).toInt();

                                if(allowedUntilAr[0] < 2000 || allowedUntilAr[0] > 3000 || allowedUntilAr[1] < 1 || allowedUntilAr[1] > 12 || allowedUntilAr[2] < 1 || allowedUntilAr[2] > 31 || allowedUntilAr[3] < 0 || allowedUntilAr[3] > 23 || allowedUntilAr[4] < 0 || allowedUntilAr[4] > 59 || allowedUntilAr[5] < 0 || allowedUntilAr[5] > 59)
                                {
                                    _network->publishKeypadJsonCommandResult("invalidAllowedUntil");
                                    return;
                                }
                            }
                            else
                            {
                                _network->publishKeypadJsonCommandResult("invalidAllowedUntil");
                                return;
                            }
                        }

                        if(allowedFromTime.length() > 0)
                        {
                            if(allowedFromTime.length() == 5)
                            {
                                allowedFromTimeAr[0] = (uint8_t)allowedFromTime.substring(0, 2).toInt();
                                allowedFromTimeAr[1] = (uint8_t)allowedFromTime.substring(3, 5).toInt();

                                if(allowedFromTimeAr[0] < 0 || allowedFromTimeAr[0] > 23 || allowedFromTimeAr[1] < 0 || allowedFromTimeAr[1] > 59)
                                {
                                    _network->publishKeypadJsonCommandResult("invalidAllowedFromTime");
                                    return;
                                }
                            }
                            else
                            {
                                _network->publishKeypadJsonCommandResult("invalidAllowedFromTime");
                                return;
                            }
                        }

                        if(allowedUntilTime.length() > 0)
                        {
                            if(allowedUntilTime.length() == 5)
                            {
                                allowedUntilTimeAr[0] = (uint8_t)allowedUntilTime.substring(0, 2).toInt();
                                allowedUntilTimeAr[1] = (uint8_t)allowedUntilTime.substring(3, 5).toInt();

                                if(allowedUntilTimeAr[0] < 0 || allowedUntilTimeAr[0] > 23 || allowedUntilTimeAr[1] < 0 || allowedUntilTimeAr[1] > 59)
                                {
                                    _network->publishKeypadJsonCommandResult("invalidAllowedUntilTime");
                                    return;
                                }
                            }
                            else
                            {
                                _network->publishKeypadJsonCommandResult("invalidAllowedUntilTime");
                                return;
                            }
                        }

                        if(allowedWeekdays.indexOf("mon") >= 0)
                        {
                            allowedWeekdaysInt += 64;
                        }
                        if(allowedWeekdays.indexOf("tue") >= 0)
                        {
                            allowedWeekdaysInt += 32;
                        }
                        if(allowedWeekdays.indexOf("wed") >= 0)
                        {
                            allowedWeekdaysInt += 16;
                        }
                        if(allowedWeekdays.indexOf("thu") >= 0)
                        {
                            allowedWeekdaysInt += 8;
                        }
                        if(allowedWeekdays.indexOf("fri") >= 0)
                        {
                            allowedWeekdaysInt += 4;
                        }
                        if(allowedWeekdays.indexOf("sat") >= 0)
                        {
                            allowedWeekdaysInt += 2;
                        }
                        if(allowedWeekdays.indexOf("sun") >= 0)
                        {
                            allowedWeekdaysInt += 1;
                        }
                    }

                    if(strcmp(action, "add") == 0)
                    {
                        NukiOpener::NewKeypadEntry entry;
                        memset(&entry, 0, sizeof(entry));
                        size_t nameLen = name.length();
                        memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
                        entry.code = code;
                        entry.timeLimited = timeLimited == 1 ? 1 : 0;

                        if(allowedFrom.length() > 0)
                        {
                            entry.allowedFromYear = allowedFromAr[0];
                            entry.allowedFromMonth = allowedFromAr[1];
                            entry.allowedFromDay = allowedFromAr[2];
                            entry.allowedFromHour = allowedFromAr[3];
                            entry.allowedFromMin = allowedFromAr[4];
                            entry.allowedFromSec = allowedFromAr[5];
                        }

                        if(allowedUntil.length() > 0)
                        {
                            entry.allowedUntilYear = allowedUntilAr[0];
                            entry.allowedUntilMonth = allowedUntilAr[1];
                            entry.allowedUntilDay = allowedUntilAr[2];
                            entry.allowedUntilHour = allowedUntilAr[3];
                            entry.allowedUntilMin = allowedUntilAr[4];
                            entry.allowedUntilSec = allowedUntilAr[5];
                        }

                        entry.allowedWeekdays = allowedWeekdaysInt;

                        if(allowedFromTime.length() > 0)
                        {
                            entry.allowedFromTimeHour = allowedFromTimeAr[0];
                            entry.allowedFromTimeMin = allowedFromTimeAr[1];
                        }

                        if(allowedUntilTime.length() > 0)
                        {
                            entry.allowedUntilTimeHour = allowedUntilTimeAr[0];
                            entry.allowedUntilTimeMin = allowedUntilTimeAr[1];
                        }

                        result = _nukiRetryHandler->retryComm([&]()
                        {
                            return _nukiOpener.addKeypadEntry(entry);
                        });
                        Log->print("Add keypad code: ");
                        NukiOpenerHelper::printCommandResult(result);
                    }
                    else if (strcmp(action, "update") == 0)
                    {
                        if(!codeId)
                        {
                            _network->publishKeypadJsonCommandResult("noCodeIdSet");
                            return;
                        }

                        if(!idExists)
                        {
                            _network->publishKeypadJsonCommandResult("noExistingCodeIdSet");
                            return;
                        }

                        std::list<NukiOpener::KeypadEntry> entries;
                        _nukiOpener.getKeypadEntries(&entries);
                        bool cachedEntry = false;
                        for(const auto& entry : entries)
                        {
                            if(entry.codeId == codeId)
                            {
                                cachedEntry = true;
                                break;
                            }
                        }
                        Nuki::CmdResult resultKp = Nuki::CmdResult::Success;
                        if(!cachedEntry)
                        {
                            resultKp = _nukiRetryHandler->retryComm([&]()
                            {
                                return _nukiOpener.retrieveKeypadEntries(0, _preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD));
                            });
                        }
                        bool foundExisting = false;

                        if(resultKp == Nuki::CmdResult::Success)
                        {
                            if(!cachedEntry)
                            {
                                _nukiOpener.getKeypadEntries(&entries);
                            }

                            for(const auto& entry : entries)
                            {
                                if (codeId != entry.codeId)
                                {
                                    continue;
                                }
                                else
                                {
                                    foundExisting = true;
                                }

                                if(name.length() < 1)
                                {
                                    memset(oldName, 0, sizeof(oldName));
                                    memcpy(oldName, entry.name, sizeof(entry.name));
                                }
                                if(code == 12)
                                {
                                    code = entry.code;
                                }
                                if(enabled == 2)
                                {
                                    enabled = entry.enabled;
                                }
                                if(timeLimited == 2)
                                {
                                    timeLimited = entry.timeLimited;
                                }
                                if(allowedFrom.length() < 1)
                                {
                                    allowedFrom = "old";
                                    allowedFromAr[0] = entry.allowedFromYear;
                                    allowedFromAr[1] = entry.allowedFromMonth;
                                    allowedFromAr[2] = entry.allowedFromDay;
                                    allowedFromAr[3] = entry.allowedFromHour;
                                    allowedFromAr[4] = entry.allowedFromMin;
                                    allowedFromAr[5] = entry.allowedFromSec;
                                }
                                if(allowedUntil.length() < 1)
                                {
                                    allowedUntil = "old";
                                    allowedUntilAr[0] = entry.allowedUntilYear;
                                    allowedUntilAr[1] = entry.allowedUntilMonth;
                                    allowedUntilAr[2] = entry.allowedUntilDay;
                                    allowedUntilAr[3] = entry.allowedUntilHour;
                                    allowedUntilAr[4] = entry.allowedUntilMin;
                                    allowedUntilAr[5] = entry.allowedUntilSec;
                                }
                                if(allowedWeekdays.length() < 1)
                                {
                                    allowedWeekdaysInt = entry.allowedWeekdays;
                                }
                                if(allowedFromTime.length() < 1)
                                {
                                    allowedFromTime = "old";
                                    allowedFromTimeAr[0] = entry.allowedFromTimeHour;
                                    allowedFromTimeAr[1] = entry.allowedFromTimeMin;
                                }

                                if(allowedUntilTime.length() < 1)
                                {
                                    allowedUntilTime = "old";
                                    allowedUntilTimeAr[0] = entry.allowedUntilTimeHour;
                                    allowedUntilTimeAr[1] = entry.allowedUntilTimeMin;
                                }

                            }

                            if(!foundExisting)
                            {
                                _network->publishKeypadJsonCommandResult("failedToRetrieveExistingKeypadEntry");
                                return;
                            }
                        }
                        else
                        {
                            _network->publishKeypadJsonCommandResult("failedToRetrieveExistingKeypadEntry");
                            return;
                        }

                        NukiOpener::UpdatedKeypadEntry entry;

                        memset(&entry, 0, sizeof(entry));
                        entry.codeId = codeId;
                        entry.code = code;

                        if(name.length() < 1)
                        {
                            size_t nameLen = strlen(oldName);
                            memcpy(&entry.name, oldName, nameLen > 20 ? 20 : nameLen);
                        }
                        else
                        {
                            size_t nameLen = name.length();
                            memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
                        }
                        entry.enabled = enabled;
                        entry.timeLimited = timeLimited;

                        if(timeLimited == 1)
                        {
                            if(allowedFrom.length() > 0)
                            {
                                entry.allowedFromYear = allowedFromAr[0];
                                entry.allowedFromMonth = allowedFromAr[1];
                                entry.allowedFromDay = allowedFromAr[2];
                                entry.allowedFromHour = allowedFromAr[3];
                                entry.allowedFromMin = allowedFromAr[4];
                                entry.allowedFromSec = allowedFromAr[5];
                            }

                            if(allowedUntil.length() > 0)
                            {
                                entry.allowedUntilYear = allowedUntilAr[0];
                                entry.allowedUntilMonth = allowedUntilAr[1];
                                entry.allowedUntilDay = allowedUntilAr[2];
                                entry.allowedUntilHour = allowedUntilAr[3];
                                entry.allowedUntilMin = allowedUntilAr[4];
                                entry.allowedUntilSec = allowedUntilAr[5];
                            }

                            entry.allowedWeekdays = allowedWeekdaysInt;

                            if(allowedFromTime.length() > 0)
                            {
                                entry.allowedFromTimeHour = allowedFromTimeAr[0];
                                entry.allowedFromTimeMin = allowedFromTimeAr[1];
                            }

                            if(allowedUntilTime.length() > 0)
                            {
                                entry.allowedUntilTimeHour = allowedUntilTimeAr[0];
                                entry.allowedUntilTimeMin = allowedUntilTimeAr[1];
                            }
                        }

                        result = _nukiRetryHandler->retryComm([&]()
                        {
                            return _nukiOpener.updateKeypadEntry(entry);
                        });
                        if(result == Nuki::CmdResult::Success)
                        {
                            for(auto& e : _keypadEntriesCache)
                            {
                                if(e.codeId != entry.codeId)
                                {
                                    continue;
                                }
                                e.code = entry.code;
                                e.enabled = entry.enabled;
                                e.timeLimited = entry.timeLimited;
                                e.allowedFromYear = entry.allowedFromYear;
                                e.allowedFromMonth = entry.allowedFromMonth;
                                e.allowedFromDay = entry.allowedFromDay;
                                e.allowedFromHour = entry.allowedFromHour;
                                e.allowedFromMin = entry.allowedFromMin;
                                e.allowedFromSec = entry.allowedFromSec;
                                e.allowedUntilYear = entry.allowedUntilYear;
                                e.allowedUntilMonth = entry.allowedUntilMonth;
                                e.allowedUntilDay = entry.allowedUntilDay;
                                e.allowedUntilHour = entry.allowedUntilHour;
                                e.allowedUntilMin = entry.allowedUntilMin;
                                e.allowedUntilSec = entry.allowedUntilSec;
                                e.allowedWeekdays = entry.allowedWeekdays;
                                e.allowedFromTimeHour = entry.allowedFromTimeHour;
                                e.allowedFromTimeMin = entry.allowedFromTimeMin;
                                e.allowedUntilTimeHour = entry.allowedUntilTimeHour;
                                e.allowedUntilTimeMin = entry.allowedUntilTimeMin;
                                memcpy(e.name, entry.name, sizeof(e.name));
                                _keypadCacheDirty = true;
                            }
                        }
                        Log->print("Update keypad code: ");
                        NukiOpenerHelper::printCommandResult(result);
                    }
                }
                else
                {
                    _network->publishKeypadJsonCommandResult("invalidAction");
                    return;
                }

                if(result == Nuki::CmdResult::Success || result == Nuki::CmdResult::Failed || result == Nuki::CmdResult::NotPaired)
                {
                    break;
                }
                ++retryCount;
                if(retryCount <= _nrOfRetries)
                {
                    espDelayAck(_retryDelay);
                }
            }

            if((int)result != -1)
            {
                char resultStr[15];
                memset(&resultStr, 0, sizeof(resultStr));
                NukiOpener::cmdResultToString(result, resultStr);
                _network->publishKeypadJsonCommandResult(resultStr);
            }
            _nextKeypadUpdateTs = espMillis() + 3000;
        }
    }
    else
    {
        _network->publishKeypadJsonCommandResult("noActionSet");
        return;
    }
}

void NukiOpenerWrapper::onAuthCommandReceived(const char *value)
{
    if(!_nukiConfigValid)
    {
        _network->publishAuthCommandResult("configNotReady");
        return;
    }

    if(!isPinValid())
    {
        _network->publishAuthCommandResult("noValidPinSet");
        return;
    }

    if(!_preferences->getBool(preference_auth_control_enabled))
    {
        _network->publishAuthCommandResult("keypadControlDisabled");
        return;
    }

    JsonDocument json;
    DeserializationError jsonError = deserializeJson(json, value);

    if(jsonError)
    {
        _network->publishAuthCommandResult("invalidJson");
        return;
    }

    char oldName[33];
    const char *action = json["action"].as<const char*>();
    uint32_t authId = json["authId"].as<unsigned int>();
    //uint8_t idType = json["idType"].as<unsigned int>();
    //unsigned char secretKeyK[32] = {0x00};
    uint8_t remoteAllowed;
    uint8_t enabled;
    uint8_t timeLimited;
    String name;
    //String sharedKey;
    String allowedFrom;
    String allowedUntil;
    String allowedWeekdays;
    String allowedFromTime;
    String allowedUntilTime;

    if(json["remoteAllowed"].is<JsonVariant>())
    {
        remoteAllowed = json["remoteAllowed"].as<unsigned int>();
    }
    else
    {
        remoteAllowed = 2;
    }

    if(json["enabled"].is<JsonVariant>())
    {
        enabled = json["enabled"].as<unsigned int>();
    }
    else
    {
        enabled = 2;
    }

    if(json["timeLimited"].is<JsonVariant>())
    {
        timeLimited = json["timeLimited"].as<unsigned int>();
    }
    else
    {
        timeLimited = 2;
    }

    if(json["name"].is<JsonVariant>())
    {
        name = json["name"].as<String>();
    }
    //if(json["sharedKey"].is<JsonVariant>()) sharedKey = json["sharedKey"].as<String>();
    if(json["allowedFrom"].is<JsonVariant>())
    {
        allowedFrom = json["allowedFrom"].as<String>();
    }
    if(json["allowedUntil"].is<JsonVariant>())
    {
        allowedUntil = json["allowedUntil"].as<String>();
    }
    if(json["allowedWeekdays"].is<JsonVariant>())
    {
        allowedWeekdays = json["allowedWeekdays"].as<String>();
    }
    if(json["allowedFromTime"].is<JsonVariant>())
    {
        allowedFromTime = json["allowedFromTime"].as<String>();
    }
    if(json["allowedUntilTime"].is<JsonVariant>())
    {
        allowedUntilTime = json["allowedUntilTime"].as<String>();
    }

    if(action)
    {
        bool idExists = false;

        if(authId)
        {
            idExists = std::find(_authIds.begin(), _authIds.end(), authId) != _authIds.end();
        }

        Nuki::CmdResult result = (Nuki::CmdResult)-1;
        int retryCount = 0;

        while(retryCount < _nrOfRetries)
        {
            if(strcmp(action, "delete") == 0)
            {
                if(idExists)
                {
                    result = _nukiOpener.deleteAuthorizationEntry(authId);
                    Log->print("Delete authorization: ");
                    NukiOpenerHelper::printCommandResult(result);
                }
                else
                {
                    _network->publishAuthCommandResult("noExistingAuthIdSet");
                    return;
                }
            }
            else if(strcmp(action, "add") == 0 || strcmp(action, "update") == 0)
            {
                if(name.length() < 1)
                {
                    if (strcmp(action, "update") != 0)
                    {
                        _network->publishAuthCommandResult("noNameSet");
                        return;
                    }
                }

                /*
                if(sharedKey.length() != 64)
                {
                    if (strcmp(action, "update") != 0)
                    {
                        _network->publishAuthCommandResult("noSharedKeySet");
                        return;
                    }
                }
                else
                {
                    for(int i=0; i<sharedKey.length();i+=2) secretKeyK[(i/2)] = std::stoi(sharedKey.substring(i, i+2).c_str(), nullptr, 16);
                }
                */

                unsigned int allowedFromAr[6];
                unsigned int allowedUntilAr[6];
                unsigned int allowedFromTimeAr[2];
                unsigned int allowedUntilTimeAr[2];
                uint8_t allowedWeekdaysInt = 0;

                if(timeLimited == 1)
                {
                    if(allowedFrom.length() > 0)
                    {
                        if(allowedFrom.length() == 19)
                        {
                            allowedFromAr[0] = (uint16_t)allowedFrom.substring(0, 4).toInt();
                            allowedFromAr[1] = (uint8_t)allowedFrom.substring(5, 7).toInt();
                            allowedFromAr[2] = (uint8_t)allowedFrom.substring(8, 10).toInt();
                            allowedFromAr[3] = (uint8_t)allowedFrom.substring(11, 13).toInt();
                            allowedFromAr[4] = (uint8_t)allowedFrom.substring(14, 16).toInt();
                            allowedFromAr[5] = (uint8_t)allowedFrom.substring(17, 19).toInt();

                            if(allowedFromAr[0] < 2000 || allowedFromAr[0] > 3000 || allowedFromAr[1] < 1 || allowedFromAr[1] > 12 || allowedFromAr[2] < 1 || allowedFromAr[2] > 31 || allowedFromAr[3] < 0 || allowedFromAr[3] > 23 || allowedFromAr[4] < 0 || allowedFromAr[4] > 59 || allowedFromAr[5] < 0 || allowedFromAr[5] > 59)
                            {
                                _network->publishAuthCommandResult("invalidAllowedFrom");
                                return;
                            }
                        }
                        else
                        {
                            _network->publishAuthCommandResult("invalidAllowedFrom");
                            return;
                        }
                    }

                    if(allowedUntil.length() > 0)
                    {
                        if(allowedUntil.length() == 19)
                        {
                            allowedUntilAr[0] = (uint16_t)allowedUntil.substring(0, 4).toInt();
                            allowedUntilAr[1] = (uint8_t)allowedUntil.substring(5, 7).toInt();
                            allowedUntilAr[2] = (uint8_t)allowedUntil.substring(8, 10).toInt();
                            allowedUntilAr[3] = (uint8_t)allowedUntil.substring(11, 13).toInt();
                            allowedUntilAr[4] = (uint8_t)allowedUntil.substring(14, 16).toInt();
                            allowedUntilAr[5] = (uint8_t)allowedUntil.substring(17, 19).toInt();

                            if(allowedUntilAr[0] < 2000 || allowedUntilAr[0] > 3000 || allowedUntilAr[1] < 1 || allowedUntilAr[1] > 12 || allowedUntilAr[2] < 1 || allowedUntilAr[2] > 31 || allowedUntilAr[3] < 0 || allowedUntilAr[3] > 23 || allowedUntilAr[4] < 0 || allowedUntilAr[4] > 59 || allowedUntilAr[5] < 0 || allowedUntilAr[5] > 59)
                            {
                                _network->publishAuthCommandResult("invalidAllowedUntil");
                                return;
                            }
                        }
                        else
                        {
                            _network->publishAuthCommandResult("invalidAllowedUntil");
                            return;
                        }
                    }

                    if(allowedFromTime.length() > 0)
                    {
                        if(allowedFromTime.length() == 5)
                        {
                            allowedFromTimeAr[0] = (uint8_t)allowedFromTime.substring(0, 2).toInt();
                            allowedFromTimeAr[1] = (uint8_t)allowedFromTime.substring(3, 5).toInt();

                            if(allowedFromTimeAr[0] < 0 || allowedFromTimeAr[0] > 23 || allowedFromTimeAr[1] < 0 || allowedFromTimeAr[1] > 59)
                            {
                                _network->publishAuthCommandResult("invalidAllowedFromTime");
                                return;
                            }
                        }
                        else
                        {
                            _network->publishAuthCommandResult("invalidAllowedFromTime");
                            return;
                        }
                    }

                    if(allowedUntilTime.length() > 0)
                    {
                        if(allowedUntilTime.length() == 5)
                        {
                            allowedUntilTimeAr[0] = (uint8_t)allowedUntilTime.substring(0, 2).toInt();
                            allowedUntilTimeAr[1] = (uint8_t)allowedUntilTime.substring(3, 5).toInt();

                            if(allowedUntilTimeAr[0] < 0 || allowedUntilTimeAr[0] > 23 || allowedUntilTimeAr[1] < 0 || allowedUntilTimeAr[1] > 59)
                            {
                                _network->publishAuthCommandResult("invalidAllowedUntilTime");
                                return;
                            }
                        }
                        else
                        {
                            _network->publishAuthCommandResult("invalidAllowedUntilTime");
                            return;
                        }
                    }

                    if(allowedWeekdays.indexOf("mon") >= 0)
                    {
                        allowedWeekdaysInt += 64;
                    }
                    if(allowedWeekdays.indexOf("tue") >= 0)
                    {
                        allowedWeekdaysInt += 32;
                    }
                    if(allowedWeekdays.indexOf("wed") >= 0)
                    {
                        allowedWeekdaysInt += 16;
                    }
                    if(allowedWeekdays.indexOf("thu") >= 0)
                    {
                        allowedWeekdaysInt += 8;
                    }
                    if(allowedWeekdays.indexOf("fri") >= 0)
                    {
                        allowedWeekdaysInt += 4;
                    }
                    if(allowedWeekdays.indexOf("sat") >= 0)
                    {
                        allowedWeekdaysInt += 2;
                    }
                    if(allowedWeekdays.indexOf("sun") >= 0)
                    {
                        allowedWeekdaysInt += 1;
                    }
                }

                if(strcmp(action, "add") == 0)
                {
                    _network->publishAuthCommandResult("addActionNotSupported");
                    return;

                    NukiOpener::NewAuthorizationEntry entry;
                    memset(&entry, 0, sizeof(entry));
                    size_t nameLen = name.length();
                    memcpy(&entry.name, name.c_str(), nameLen > 32 ? 32 : nameLen);
                    /*
                    memcpy(&entry.sharedKey, secretKeyK, 32);

                    if(idType != 1)
                    {
                        _network->publishAuthCommandResult("invalidIdType");
                        return;
                    }

                    entry.idType = idType;
                    */
                    entry.remoteAllowed = remoteAllowed == 1 ? 1 : 0;
                    entry.timeLimited = timeLimited == 1 ? 1 : 0;

                    if(allowedFrom.length() > 0)
                    {
                        entry.allowedFromYear = allowedFromAr[0];
                        entry.allowedFromMonth = allowedFromAr[1];
                        entry.allowedFromDay = allowedFromAr[2];
                        entry.allowedFromHour = allowedFromAr[3];
                        entry.allowedFromMinute = allowedFromAr[4];
                        entry.allowedFromSecond = allowedFromAr[5];
                    }

                    if(allowedUntil.length() > 0)
                    {
                        entry.allowedUntilYear = allowedUntilAr[0];
                        entry.allowedUntilMonth = allowedUntilAr[1];
                        entry.allowedUntilDay = allowedUntilAr[2];
                        entry.allowedUntilHour = allowedUntilAr[3];
                        entry.allowedUntilMinute = allowedUntilAr[4];
                        entry.allowedUntilSecond = allowedUntilAr[5];
                    }

                    entry.allowedWeekdays = allowedWeekdaysInt;

                    if(allowedFromTime.length() > 0)
                    {
                        entry.allowedFromTimeHour = allowedFromTimeAr[0];
                        entry.allowedFromTimeMin = allowedFromTimeAr[1];
                    }

                    if(allowedUntilTime.length() > 0)
                    {
                        entry.allowedUntilTimeHour = allowedUntilTimeAr[0];
                        entry.allowedUntilTimeMin = allowedUntilTimeAr[1];
                    }

                    result = _nukiOpener.addAuthorizationEntry(entry);
                    Log->print("Add authorization: ");
                    NukiOpenerHelper::printCommandResult(result);
                }
                else if (strcmp(action, "update") == 0)
                {
                    if(!authId)
                    {
                        _network->publishAuthCommandResult("noAuthIdSet");
                        return;
                    }

                    if(!idExists)
                    {
                        _network->publishAuthCommandResult("noExistingAuthIdSet");
                        return;
                    }

                    Nuki::CmdResult resultAuth = _nukiOpener.retrieveAuthorizationEntries(0, _preferences->getInt(preference_auth_max_entries, MAX_AUTH));
                    bool foundExisting = false;

                    if(resultAuth == Nuki::CmdResult::Success)
                    {
                        espDelayAck(5000);
                        std::list<NukiOpener::AuthorizationEntry> entries;
                        _nukiOpener.getAuthorizationEntries(&entries);

                        for(const auto& entry : entries)
                        {
                            if (authId != entry.authId)
                            {
                                continue;
                            }
                            else
                            {
                                foundExisting = true;
                            }

                            if(name.length() < 1)
                            {
                                memset(oldName, 0, sizeof(oldName));
                                memcpy(oldName, entry.name, sizeof(entry.name));
                            }
                            if(remoteAllowed == 2)
                            {
                                remoteAllowed = entry.remoteAllowed;
                            }
                            if(enabled == 2)
                            {
                                enabled = entry.enabled;
                            }
                            if(timeLimited == 2)
                            {
                                timeLimited = entry.timeLimited;
                            }
                            if(allowedFrom.length() < 1)
                            {
                                allowedFrom = "old";
                                allowedFromAr[0] = entry.allowedFromYear;
                                allowedFromAr[1] = entry.allowedFromMonth;
                                allowedFromAr[2] = entry.allowedFromDay;
                                allowedFromAr[3] = entry.allowedFromHour;
                                allowedFromAr[4] = entry.allowedFromMinute;
                                allowedFromAr[5] = entry.allowedFromSecond;
                            }
                            if(allowedUntil.length() < 1)
                            {
                                allowedUntil = "old";
                                allowedUntilAr[0] = entry.allowedUntilYear;
                                allowedUntilAr[1] = entry.allowedUntilMonth;
                                allowedUntilAr[2] = entry.allowedUntilDay;
                                allowedUntilAr[3] = entry.allowedUntilHour;
                                allowedUntilAr[4] = entry.allowedUntilMinute;
                                allowedUntilAr[5] = entry.allowedUntilSecond;
                            }
                            if(allowedWeekdays.length() < 1)
                            {
                                allowedWeekdaysInt = entry.allowedWeekdays;
                            }
                            if(allowedFromTime.length() < 1)
                            {
                                allowedFromTime = "old";
                                allowedFromTimeAr[0] = entry.allowedFromTimeHour;
                                allowedFromTimeAr[1] = entry.allowedFromTimeMin;
                            }

                            if(allowedUntilTime.length() < 1)
                            {
                                allowedUntilTime = "old";
                                allowedUntilTimeAr[0] = entry.allowedUntilTimeHour;
                                allowedUntilTimeAr[1] = entry.allowedUntilTimeMin;
                            }
                        }

                        if(!foundExisting)
                        {
                            _network->publishAuthCommandResult("failedToRetrieveExistingAuthorizationEntry");
                            return;
                        }
                    }
                    else
                    {
                        _network->publishAuthCommandResult("failedToRetrieveExistingAuthorizationEntry");
                        return;
                    }

                    NukiOpener::UpdatedAuthorizationEntry entry;

                    memset(&entry, 0, sizeof(entry));
                    entry.authId = authId;

                    if(name.length() < 1)
                    {
                        size_t nameLen = strlen(oldName);
                        memcpy(&entry.name, oldName, nameLen > 20 ? 20 : nameLen);
                    }
                    else
                    {
                        size_t nameLen = name.length();
                        memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
                    }
                    entry.remoteAllowed = remoteAllowed;
                    entry.enabled = enabled;
                    entry.timeLimited = timeLimited;

                    if(timeLimited == 1)
                    {
                        if(allowedFrom.length() > 0)
                        {
                            entry.allowedFromYear = allowedFromAr[0];
                            entry.allowedFromMonth = allowedFromAr[1];
                            entry.allowedFromDay = allowedFromAr[2];
                            entry.allowedFromHour = allowedFromAr[3];
                            entry.allowedFromMinute = allowedFromAr[4];
                            entry.allowedFromSecond = allowedFromAr[5];
                        }

                        if(allowedUntil.length() > 0)
                        {
                            entry.allowedUntilYear = allowedUntilAr[0];
                            entry.allowedUntilMonth = allowedUntilAr[1];
                            entry.allowedUntilDay = allowedUntilAr[2];
                            entry.allowedUntilHour = allowedUntilAr[3];
                            entry.allowedUntilMinute = allowedUntilAr[4];
                            entry.allowedUntilSecond = allowedUntilAr[5];
                        }

                        entry.allowedWeekdays = allowedWeekdaysInt;

                        if(allowedFromTime.length() > 0)
                        {
                            entry.allowedFromTimeHour = allowedFromTimeAr[0];
                            entry.allowedFromTimeMin = allowedFromTimeAr[1];
                        }

                        if(allowedUntilTime.length() > 0)
                        {
                            entry.allowedUntilTimeHour = allowedUntilTimeAr[0];
                            entry.allowedUntilTimeMin = allowedUntilTimeAr[1];
                        }
                    }

                    result = _nukiOpener.updateAuthorizationEntry(entry);
                    Log->print("Update authorization: ");
                    NukiOpenerHelper::printCommandResult(result);
                }
            }
            else
            {
                _network->publishAuthCommandResult("invalidAction");
                return;
            }

            if(result == Nuki::CmdResult::Success || result == Nuki::CmdResult::Failed || result == Nuki::CmdResult::NotPaired)
            {
                break;
            }
            ++retryCount;
            if(retryCount <= _nrOfRetries)
            {
                espDelayAck(_retryDelay);
            }
        }

        updateAuth(false);

        if((int)result != -1)
        {
            char resultStr[15];
            memset(&resultStr, 0, sizeof(resultStr));
            NukiOpener::cmdResultToString(result, resultStr);
            _network->publishAuthCommandResult(resultStr);
        }
    }
    else
    {
        _network->publishAuthCommandResult("noActionSet");
        return;
    }
}

void NukiOpenerWrapper::onTimeControlCommandReceived(const char *value)
{
    if(!_nukiConfigValid)
    {
        _network->publishTimeControlCommandResult("configNotReady");
        return;
    }

    if(!isPinValid())
    {
        _network->publishTimeControlCommandResult("noValidPinSet");
        return;
    }

    if(!_preferences->getBool(preference_timecontrol_control_enabled, false))
    {
        _network->publishTimeControlCommandResult("timeControlControlDisabled");
        return;
    }

    JsonDocument json;
    DeserializationError jsonError = deserializeJson(json, value);

    if(jsonError)
    {
        _network->publishTimeControlCommandResult("invalidJson");
        return;
    }

    const char *action = json["action"].as<const char*>();
    uint8_t entryId = json["entryId"].as<unsigned int>();
    uint8_t enabled;
    String weekdays;
    String time;
    String lockAction;
    NukiOpener::LockAction timeControlLockAction;

    if(json["enabled"].is<JsonVariant>())
    {
        enabled = json["enabled"].as<unsigned int>();
    }
    else
    {
        enabled = 2;
    }

    if(json["weekdays"].is<JsonVariant>())
    {
        weekdays = json["weekdays"].as<String>();
    }
    if(json["time"].is<JsonVariant>())
    {
        time = json["time"].as<String>();
    }
    if(json["lockAction"].is<JsonVariant>())
    {
        lockAction = json["lockAction"].as<String>();
    }

    if(lockAction.length() > 0)
    {
        timeControlLockAction = NukiOpenerHelper::lockActionToEnum(lockAction.c_str());

        if((int)timeControlLockAction == 0xff)
        {
            _network->publishTimeControlCommandResult("invalidLockAction");
            return;
        }
    }

    if(action)
    {
        bool idExists = false;

        if(entryId)
        {
            idExists = std::find(_timeControlIds.begin(), _timeControlIds.end(), entryId) != _timeControlIds.end();
        }

        Nuki::CmdResult result = (Nuki::CmdResult)-1;
        int retryCount = 0;

        while(retryCount < _nrOfRetries + 1)
        {
            if(strcmp(action, "delete") == 0)
            {
                if(idExists)
                {
                    result = _nukiOpener.removeTimeControlEntry(entryId);
                    Log->print("Delete timecontrol: ");
                    NukiOpenerHelper::printCommandResult(result);
                }
                else
                {
                    _network->publishTimeControlCommandResult("noExistingEntryIdSet");
                    return;
                }
            }
            else if(strcmp(action, "add") == 0 || strcmp(action, "update") == 0)
            {
                uint8_t timeHour;
                uint8_t timeMin;
                uint8_t weekdaysInt = 0;
                unsigned int timeAr[2];

                if(time.length() > 0)
                {
                    if(time.length() == 5)
                    {
                        timeAr[0] = (uint8_t)time.substring(0, 2).toInt();
                        timeAr[1] = (uint8_t)time.substring(3, 5).toInt();

                        if(timeAr[0] < 0 || timeAr[0] > 23 || timeAr[1] < 0 || timeAr[1] > 59)
                        {
                            _network->publishTimeControlCommandResult("invalidTime");
                            return;
                        }
                    }
                    else
                    {
                        _network->publishTimeControlCommandResult("invalidTime");
                        return;
                    }
                }

                if(weekdays.indexOf("mon") >= 0)
                {
                    weekdaysInt += 64;
                }
                if(weekdays.indexOf("tue") >= 0)
                {
                    weekdaysInt += 32;
                }
                if(weekdays.indexOf("wed") >= 0)
                {
                    weekdaysInt += 16;
                }
                if(weekdays.indexOf("thu") >= 0)
                {
                    weekdaysInt += 8;
                }
                if(weekdays.indexOf("fri") >= 0)
                {
                    weekdaysInt += 4;
                }
                if(weekdays.indexOf("sat") >= 0)
                {
                    weekdaysInt += 2;
                }
                if(weekdays.indexOf("sun") >= 0)
                {
                    weekdaysInt += 1;
                }

                if(strcmp(action, "add") == 0)
                {
                    NukiOpener::NewTimeControlEntry entry;
                    memset(&entry, 0, sizeof(entry));
                    entry.weekdays = weekdaysInt;

                    if(time.length() > 0)
                    {
                        entry.timeHour = timeAr[0];
                        entry.timeMin = timeAr[1];
                    }

                    entry.lockAction = timeControlLockAction;
                    result = _nukiOpener.addTimeControlEntry(entry);
                    Log->print("Add timecontrol: ");
                    NukiOpenerHelper::printCommandResult(result);
                }
                else if (strcmp(action, "update") == 0)
                {
                    if(!idExists)
                    {
                        _network->publishTimeControlCommandResult("noExistingEntryIdSet");
                        return;
                    }

                    Nuki::CmdResult resultTc = _nukiOpener.retrieveTimeControlEntries();
                    bool foundExisting = false;

                    if(resultTc == Nuki::CmdResult::Success)
                    {
                        espDelayAck(5000);
                        std::list<NukiOpener::TimeControlEntry> timeControlEntries;
                        _nukiOpener.getTimeControlEntries(&timeControlEntries);

                        for(const auto& entry : timeControlEntries)
                        {
                            if (entryId != entry.entryId)
                            {
                                continue;
                            }
                            else
                            {
                                foundExisting = true;
                            }

                            if(enabled == 2)
                            {
                                enabled = entry.enabled;
                            }
                            if(weekdays.length() < 1)
                            {
                                weekdaysInt = entry.weekdays;
                            }
                            if(time.length() < 1)
                            {
                                time = "old";
                                timeAr[0] = entry.timeHour;
                                timeAr[1] = entry.timeMin;
                            }
                            if(lockAction.length() < 1)
                            {
                                timeControlLockAction = entry.lockAction;
                            }
                        }

                        if(!foundExisting)
                        {
                            _network->publishTimeControlCommandResult("failedToRetrieveExistingTimeControlEntry");
                            return;
                        }
                    }
                    else
                    {
                        _network->publishTimeControlCommandResult("failedToRetrieveExistingTimeControlEntry");
                        return;
                    }

                    NukiOpener::TimeControlEntry entry;
                    memset(&entry, 0, sizeof(entry));
                    entry.entryId = entryId;
                    entry.enabled = enabled;
                    entry.weekdays = weekdaysInt;

                    if(time.length() > 0)
                    {
                        entry.timeHour = timeAr[0];
                        entry.timeMin = timeAr[1];
                    }

                    entry.lockAction = timeControlLockAction;
                    result = _nukiOpener.updateTimeControlEntry(entry);
                    Log->print("Update timecontrol: ");
                    NukiOpenerHelper::printCommandResult(result);
                }
            }
            else
            {
                _network->publishTimeControlCommandResult("invalidAction");
                return;
            }

            if(result == Nuki::CmdResult::Success || result == Nuki::CmdResult::Failed || result == Nuki::CmdResult::NotPaired)
            {
                break;
            }
            ++retryCount;
            if(retryCount <= _nrOfRetries)
            {
                espDelayAck(_retryDelay);
            }
        }

        if((int)result != -1)
        {
            char resultStr[15];
            memset(&resultStr, 0, sizeof(resultStr));
            NukiOpener::cmdResultToString(result, resultStr);
            _network->publishTimeControlCommandResult(resultStr);
        }

        _nextConfigUpdateTs = espMillis() + 300;
    }
    else
    {
        _network->publishTimeControlCommandResult("noActionSet");
        return;
    }
}

void NukiOpenerWrapper::onKeypadCommandReceived(const char *command, const uint &id, const String &name, const String &code, const int& enabled)
{
    if(_disableNonJSON)
    {
        return;
    }

    if(!_preferences->getBool(preference_keypad_control_enabled, false))
    {
        _network->publishKeypadCommandResult("KeypadControlDisabled");
        return;
    }

    if(!hasKeypad())
    {
        if(_nukiConfigValid)
        {
            _network->publishKeypadCommandResult("KeypadNotAvailable");
        }
        return;
    }
    if(!_keypadEnabled)
    {
        return;
    }

    bool idExists = std::find(_keypadCodeIds.begin(), _keypadCodeIds.end(), id) != _keypadCodeIds.end();
    int codeInt = code.toInt();
    bool codeValid = codeInt > 100000 && codeInt < 1000000 && (code.indexOf('0') == -1);
    Nuki::CmdResult result = (Nuki::CmdResult)-1;
    int retryCount = 0;

    while(retryCount < _nrOfRetries + 1)
    {
        if(strcmp(command, "add") == 0)
        {
            if(name == "" || name == "--")
            {
                _network->publishKeypadCommandResult("MissingParameterName");
                return;
            }
            if(codeInt == 0)
            {
                _network->publishKeypadCommandResult("MissingParameterCode");
                return;
            }
            if(!codeValid)
            {
                _network->publishKeypadCommandResult("CodeInvalid");
                return;
            }

            NukiOpener::NewKeypadEntry entry;
            memset(&entry, 0, sizeof(entry));
            size_t nameLen = name.length();
            memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
            entry.code = codeInt;
            result = _nukiRetryHandler->retryComm([&]()
            {
                return _nukiOpener.addKeypadEntry(entry);
            });
            Log->print("Add keypad code: ");
            NukiOpenerHelper::printCommandResult(result);
            updateKeypad(false);
        }
        else if(strcmp(command, "delete") == 0)
        {
            if(!idExists)
            {
                _network->publishKeypadCommandResult("UnknownId");
                return;
            }

            result = _nukiRetryHandler->retryComm([&]()
            {
                return _nukiOpener.deleteKeypadEntry(id);
            });
            Log->print("Delete keypad code: ");
            NukiOpenerHelper::printCommandResult(result);
            updateKeypad(false);
        }
        else if(strcmp(command, "update") == 0)
        {
            if(name == "" || name == "--")
            {
                _network->publishKeypadCommandResult("MissingParameterName");
                return;
            }
            if(codeInt == 0)
            {
                _network->publishKeypadCommandResult("MissingParameterCode");
                return;
            }
            if(!codeValid)
            {
                _network->publishKeypadCommandResult("CodeInvalid");
                return;
            }
            if(!idExists)
            {
                _network->publishKeypadCommandResult("UnknownId");
                return;
            }

            NukiOpener::UpdatedKeypadEntry entry;
            memset(&entry, 0, sizeof(entry));
            entry.codeId = id;
            size_t nameLen = name.length();
            memcpy(&entry.name, name.c_str(), nameLen > 20 ? 20 : nameLen);
            entry.code = codeInt;
            entry.enabled = enabled == 0 ? 0 : 1;
            result = _nukiRetryHandler->retryComm([&]()
            {
                return _nukiOpener.updateKeypadEntry(entry);
            });
            Log->print("Update keypad code: ");
            NukiOpenerHelper::printCommandResult(result);
            updateKeypad(false);
        }
        else if(strcmp(command, "--") == 0)
        {
            return;
        }
        else
        {
            _network->publishKeypadCommandResult("UnknownCommand");
            return;
        }

        if(result == Nuki::CmdResult::Success || result == Nuki::CmdResult::Failed || result == Nuki::CmdResult::NotPaired)
        {
            break;
        }
        ++retryCount;
        if(retryCount <= _nrOfRetries)
        {
            espDelayAck(_retryDelay);
        }
    }

    if((int)result != -1)
    {
        char resultStr[15];
        memset(&resultStr, 0, sizeof(resultStr));
        NukiOpener::cmdResultToString(result, resultStr);
        _network->publishKeypadCommandResult(resultStr);
    }
}

void NukiOpenerWrapper::onKeypadJsonCommandReceived(const char *value)
{
    char cmd[KEYPAD_JSON_MAX_LENGTH];

    if(strlen(value) >= sizeof(cmd))
    {
        _network->publishKeypadJsonCommandResult("invalidJson");
        return;
    }

    strcpy(cmd, value);

    if(xQueueSend(_keypadJsonQueue, cmd, 0) != pdTRUE)
    {
        _network->publishKeypadJsonCommandResult("busy");
    }
}
