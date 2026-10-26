#include <functional>
#include <algorithm>
#include "NukiWrapper.h"
#include "PreferencesKeys.h"
#include "MqttTopics.h"
#include "Logger.h"
#include "RestartReason.h"
#include <NukiLockUtils.h>
#include "Config.h"
#include "enums/NukiPinState.h"
#include "hal/wdt_hal.h"
#include <time.h>
#include "esp_sntp.h"
#include "util/NukiHelper.h"
#include "util/NukiRetryHandler.h"

void NukiWrapper::updateConfigBool(JsonDocument& jsonResult, const char* key, const char* value, uint8_t current, Nuki::CmdResult& cmdResult, std::function<Nuki::CmdResult(bool)> setter)
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

void NukiWrapper::updateConfigInt(JsonDocument& jsonResult, const char* key, const char* value, int min, int max, int current, Nuki::CmdResult& cmdResult, std::function<Nuki::CmdResult(int)> setter)
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

void NukiWrapper::onConfigUpdateReceived(const char *value)
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
    const char *basicKeys[16] = {"name", "latitude", "longitude", "autoUnlatch", "pairingEnabled", "buttonEnabled", "ledEnabled", "ledBrightness", "timeZoneOffset", "dstMode", "fobAction1",  "fobAction2", "fobAction3", "singleLock", "advertisingMode", "timeZone"};
    const char *advancedKeys[26] = {"unlockedPositionOffsetDegrees", "lockedPositionOffsetDegrees", "singleLockedPositionOffsetDegrees", "unlockedToLockedTransitionOffsetDegrees", "lockNgoTimeout", "singleButtonPressAction", "doubleButtonPressAction", "detachedCylinder", "batteryType", "automaticBatteryTypeDetection", "unlatchDuration", "autoLockTimeOut",  "autoUnLockDisabled", "nightModeEnabled", "nightModeStartTime", "nightModeEndTime", "nightModeAutoLockEnabled", "nightModeAutoUnlockDisabled", "nightModeImmediateLockOnStart", "autoLockEnabled", "immediateAutoLockEnabled", "autoUpdateEnabled", "rebootNuki", "motorSpeed", "enableSlowSpeedDuringNightMode", "recalibrateNuki"};
    bool basicUpdated = false;
    bool advancedUpdated = false;

    for(int i=0; i < 16; i++)
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

            if((int)_basicLockConfigaclPrefs[i] == 1)
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
                                cmdResult = _nukiLock.setName(std::string(jsonchar));
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
                                cmdResult = _nukiLock.setLatitude(keyvalue);
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
                                cmdResult = _nukiLock.setLongitude(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "autoUnlatch") == 0)
                    {
                        updateConfigBool(jsonResult, basicKeys[i], jsonchar, _nukiConfig.autoUnlatch, cmdResult, [&](bool value) { return _nukiLock.enableAutoUnlatch(value); });
                    }
                    else if(strcmp(basicKeys[i], "pairingEnabled") == 0)
                    {
                        updateConfigBool(jsonResult, basicKeys[i], jsonchar, _nukiConfig.pairingEnabled, cmdResult, [&](bool value) { return _nukiLock.enablePairing(value); });
                    }
                    else if(strcmp(basicKeys[i], "buttonEnabled") == 0)
                    {
                        updateConfigBool(jsonResult, basicKeys[i], jsonchar, _nukiConfig.buttonEnabled, cmdResult, [&](bool value) { return _nukiLock.enableButton(value); });
                    }
                    else if(strcmp(basicKeys[i], "ledEnabled") == 0)
                    {
                        updateConfigBool(jsonResult, basicKeys[i], jsonchar, _nukiConfig.ledEnabled, cmdResult, [&](bool value) { return _nukiLock.enableLedFlash(value); });
                    }
                    else if(strcmp(basicKeys[i], "ledBrightness") == 0)
                    {
                        updateConfigInt(jsonResult, basicKeys[i], jsonchar, 0, 5, _nukiConfig.ledBrightness, cmdResult, [&](int value) { return _nukiLock.setLedBrightness(value); });
                    }
                    else if(strcmp(basicKeys[i], "timeZoneOffset") == 0)
                    {
                        updateConfigInt(jsonResult, basicKeys[i], jsonchar, 0, 60, _nukiConfig.timeZoneOffset, cmdResult, [&](int value) { return _nukiLock.setTimeZoneOffset(value); });
                    }
                    else if(strcmp(basicKeys[i], "dstMode") == 0)
                    {
                        updateConfigBool(jsonResult, basicKeys[i], jsonchar, _nukiConfig.dstMode, cmdResult, [&](bool value) { return _nukiLock.enableDst(value); });
                    }
                    else if(strcmp(basicKeys[i], "fobAction1") == 0)
                    {
                        const uint8_t fobAct1 = NukiHelper::fobActionToInt(jsonchar);

                        if(fobAct1 != 99)
                        {
                            if(_nukiConfig.fobAction1 == fobAct1)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setFobAction(1, fobAct1);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "fobAction2") == 0)
                    {
                        const uint8_t fobAct2 = NukiHelper::fobActionToInt(jsonchar);

                        if(fobAct2 != 99)
                        {
                            if(_nukiConfig.fobAction2 == fobAct2)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setFobAction(2, fobAct2);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "fobAction3") == 0)
                    {
                        const uint8_t fobAct3 = NukiHelper::fobActionToInt(jsonchar);

                        if(fobAct3 != 99)
                        {
                            if(_nukiConfig.fobAction3 == fobAct3)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setFobAction(3, fobAct3);
                            }
                        }
                        else
                        {
                            jsonResult[basicKeys[i]] = "invalidValue";
                        }
                    }
                    else if(strcmp(basicKeys[i], "singleLock") == 0)
                    {
                        updateConfigBool(jsonResult, basicKeys[i], jsonchar, _nukiConfig.singleLock, cmdResult, [&](bool value) { return _nukiLock.enableSingleLock(value); });
                    }
                    else if(strcmp(basicKeys[i], "advertisingMode") == 0)
                    {
                        Nuki::AdvertisingMode advmode = NukiHelper::advertisingModeToEnum(jsonchar);

                        if((int)advmode != 0xff)
                        {
                            if(_nukiConfig.advertisingMode == advmode)
                            {
                                jsonResult[basicKeys[i]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setAdvertisingMode(advmode);
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
                                cmdResult = _nukiLock.setTimeZoneId(tzid);
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
                    NukiLock::cmdResultToString(cmdResult, resultStr);
                    jsonResult[basicKeys[i]] = resultStr;
                }
            }
            else
            {
                jsonResult[basicKeys[i]] = "accessDenied";
            }
        }
    }

    for(int j=0; j < 26; j++)
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

            if((int)_advancedLockConfigaclPrefs[j] == 1)
            {
                cmdResult = Nuki::CmdResult::Error;
                int retryCount = 0;

                while(retryCount < _nrOfRetries + 1)
                {
                    if(strcmp(advancedKeys[j], "unlockedPositionOffsetDegrees") == 0)
                    {
                        updateConfigInt(jsonResult, advancedKeys[j], jsonchar, -90, 180, _nukiAdvancedConfig.unlockedPositionOffsetDegrees, cmdResult, [&](int value) { return _nukiLock.setUnlockedPositionOffsetDegrees(value); });
                    }
                    else if(strcmp(advancedKeys[j], "lockedPositionOffsetDegrees") == 0)
                    {
                        updateConfigInt(jsonResult, advancedKeys[j], jsonchar, -180, 90, _nukiAdvancedConfig.lockedPositionOffsetDegrees, cmdResult, [&](int value) { return _nukiLock.setLockedPositionOffsetDegrees(value); });
                    }
                    else if(strcmp(advancedKeys[j], "singleLockedPositionOffsetDegrees") == 0)
                    {
                        updateConfigInt(jsonResult, advancedKeys[j], jsonchar, -180, 180, _nukiAdvancedConfig.singleLockedPositionOffsetDegrees, cmdResult, [&](int value) { return _nukiLock.setSingleLockedPositionOffsetDegrees(value); });
                    }
                    else if(strcmp(advancedKeys[j], "unlockedToLockedTransitionOffsetDegrees") == 0)
                    {
                        updateConfigInt(jsonResult, advancedKeys[j], jsonchar, -180, 180, _nukiAdvancedConfig.unlockedToLockedTransitionOffsetDegrees, cmdResult, [&](int value) { return _nukiLock.setUnlockedToLockedTransitionOffsetDegrees(value); });
                    }
                    else if(strcmp(advancedKeys[j], "lockNgoTimeout") == 0)
                    {
                        updateConfigInt(jsonResult, advancedKeys[j], jsonchar, 5, 60, _nukiAdvancedConfig.lockNgoTimeout, cmdResult, [&](int value) { return _nukiLock.setLockNgoTimeout(value); });
                    }
                    else if(strcmp(advancedKeys[j], "singleButtonPressAction") == 0)
                    {
                        NukiLock::ButtonPressAction sbpa = NukiHelper::buttonPressActionToEnum(jsonchar);

                        if((int)sbpa != 0xff)
                        {
                            if(_nukiAdvancedConfig.singleButtonPressAction == sbpa)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setSingleButtonPressAction(sbpa);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "doubleButtonPressAction") == 0)
                    {
                        NukiLock::ButtonPressAction dbpa = NukiHelper::buttonPressActionToEnum(jsonchar);

                        if((int)dbpa != 0xff)
                        {
                            if(_nukiAdvancedConfig.doubleButtonPressAction == dbpa)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setDoubleButtonPressAction(dbpa);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "detachedCylinder") == 0)
                    {
                        updateConfigBool(jsonResult, advancedKeys[j], jsonchar, _nukiAdvancedConfig.detachedCylinder, cmdResult, [&](bool value) { return _nukiLock.enableDetachedCylinder(value); });
                    }
                    else if(strcmp(advancedKeys[j], "batteryType") == 0)
                    {
                        Nuki::BatteryType battype = NukiHelper::batteryTypeToEnum(jsonchar);

                        if((int)battype != 0xff && !_isUltra)
                        {
                            if(_nukiAdvancedConfig.batteryType == battype)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setBatteryType(battype);
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

                        if((keyvalue == 0 || keyvalue == 1) && !_isUltra)
                        {
                            if(_nukiAdvancedConfig.automaticBatteryTypeDetection == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.enableAutoBatteryTypeDetection((keyvalue > 0));
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "unlatchDuration") == 0)
                    {
                        updateConfigInt(jsonResult, advancedKeys[j], jsonchar, 1, 30, _nukiAdvancedConfig.unlatchDuration, cmdResult, [&](int value) { return _nukiLock.setUnlatchDuration(value); });
                    }
                    else if(strcmp(advancedKeys[j], "autoLockTimeOut") == 0)
                    {
                        updateConfigInt(jsonResult, advancedKeys[j], jsonchar, 30, 1800, _nukiAdvancedConfig.autoLockTimeOut, cmdResult, [&](int value) { return _nukiLock.setAutoLockTimeOut(value); });
                    }
                    else if(strcmp(advancedKeys[j], "autoUnLockDisabled") == 0)
                    {
                        updateConfigBool(jsonResult, advancedKeys[j], jsonchar, _nukiAdvancedConfig.autoUnLockDisabled, cmdResult, [&](bool value) { return _nukiLock.disableAutoUnlock(value); });
                    }
                    else if(strcmp(advancedKeys[j], "nightModeEnabled") == 0)
                    {
                        updateConfigBool(jsonResult, advancedKeys[j], jsonchar, _nukiAdvancedConfig.nightModeEnabled, cmdResult, [&](bool value) { return _nukiLock.enableNightMode(value); });
                    }
                    else if(strcmp(advancedKeys[j], "nightModeStartTime") == 0)
                    {
                        String keystr = jsonchar;
                        unsigned char keyvalue[2];
                        keyvalue[0] = (uint8_t)keystr.substring(0, 2).toInt();
                        keyvalue[1] = (uint8_t)keystr.substring(3, 5).toInt();
                        if(keyvalue[0] >= 0 && keyvalue[0] <= 23 && keyvalue[1] >= 0 && keyvalue[1] <= 59)
                        {
                            if(_nukiAdvancedConfig.nightModeStartTime == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setNightModeStartTime(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "nightModeEndTime") == 0)
                    {
                        String keystr = jsonchar;
                        unsigned char keyvalue[2];
                        keyvalue[0] = (uint8_t)keystr.substring(0, 2).toInt();
                        keyvalue[1] = (uint8_t)keystr.substring(3, 5).toInt();
                        if(keyvalue[0] >= 0 && keyvalue[0] <= 23 && keyvalue[1] >= 0 && keyvalue[1] <= 59)
                        {
                            if(_nukiAdvancedConfig.nightModeEndTime == keyvalue)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setNightModeEndTime(keyvalue);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "nightModeAutoLockEnabled") == 0)
                    {
                        updateConfigBool(jsonResult, advancedKeys[j], jsonchar, _nukiAdvancedConfig.nightModeAutoLockEnabled, cmdResult, [&](bool value) { return _nukiLock.enableNightModeAutoLock(value); });
                    }
                    else if(strcmp(advancedKeys[j], "nightModeAutoUnlockDisabled") == 0)
                    {
                        updateConfigBool(jsonResult, advancedKeys[j], jsonchar, _nukiAdvancedConfig.nightModeAutoUnlockDisabled, cmdResult, [&](bool value) { return _nukiLock.disableNightModeAutoUnlock(value); });
                    }
                    else if(strcmp(advancedKeys[j], "nightModeImmediateLockOnStart") == 0)
                    {
                        updateConfigBool(jsonResult, advancedKeys[j], jsonchar, _nukiAdvancedConfig.nightModeImmediateLockOnStart, cmdResult, [&](bool value) { return _nukiLock.enableNightModeImmediateLockOnStart(value); });
                    }
                    else if(strcmp(advancedKeys[j], "autoLockEnabled") == 0)
                    {
                        updateConfigBool(jsonResult, advancedKeys[j], jsonchar, _nukiAdvancedConfig.autoLockEnabled, cmdResult, [&](bool value) { return _nukiLock.enableAutoLock(value); });
                    }
                    else if(strcmp(advancedKeys[j], "immediateAutoLockEnabled") == 0)
                    {
                        updateConfigBool(jsonResult, advancedKeys[j], jsonchar, _nukiAdvancedConfig.immediateAutoLockEnabled, cmdResult, [&](bool value) { return _nukiLock.enableImmediateAutoLock(value); });
                    }
                    else if(strcmp(advancedKeys[j], "autoUpdateEnabled") == 0)
                    {
                        updateConfigBool(jsonResult, advancedKeys[j], jsonchar, _nukiAdvancedConfig.autoUpdateEnabled, cmdResult, [&](bool value) { return _nukiLock.enableAutoUpdate(value); });
                    }
                    else if(strcmp(advancedKeys[j], "rebootNuki") == 0)
                    {
                        const uint8_t keyvalue = atoi(jsonchar);

                        if(keyvalue == 1)
                        {
                            cmdResult = _nukiLock.requestReboot();
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
                            cmdResult = _nukiLock.requestCalibration();
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "motorSpeed") == 0)
                    {
                        NukiLock::MotorSpeed motorSpeed = NukiHelper::motorSpeedToEnum(jsonchar);

                        if((int)motorSpeed != 0xff)
                        {
                            if(_nukiAdvancedConfig.motorSpeed == motorSpeed)
                            {
                                jsonResult[advancedKeys[j]] = "unchanged";
                            }
                            else
                            {
                                cmdResult = _nukiLock.setMotorSpeed(motorSpeed);
                            }
                        }
                        else
                        {
                            jsonResult[advancedKeys[j]] = "invalidValue";
                        }
                    }
                    else if(strcmp(advancedKeys[j], "enableSlowSpeedDuringNightMode") == 0)
                    {
                        updateConfigBool(jsonResult, advancedKeys[j], jsonchar, _nukiAdvancedConfig.enableSlowSpeedDuringNightMode, cmdResult, [&](bool value) { return _nukiLock.enableSlowSpeedDuringNightMode(value); });
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
                    NukiLock::cmdResultToString(cmdResult, resultStr);
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

void NukiWrapper::onKeypadJsonCommandReceived(const char *value)
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
