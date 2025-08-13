#include "ImportExport.h"
#include "EspMillis.h"
#include "SPIFFS.h"
#include "Logger.h"
#include "PreferencesKeys.h"
#include <DuoAuthLib.h>
#include <TOTP-RC6236-generator.hpp>

ImportExport::ImportExport(Preferences *preferences)
    : _preferences(preferences)
{
    readSettings();
}

void ImportExport::readSettings()
{
    if (_preferences->getBool(preference_cred_duo_enabled, false))
    {
        _duoEnabled = true;
        _duoHost = _preferences->getString(preference_cred_duo_host, "");
        _duoIkey = _preferences->getString(preference_cred_duo_ikey, "");
        _duoSkey = _preferences->getString(preference_cred_duo_skey, "");
        _duoUser = _preferences->getString(preference_cred_duo_user, "");

        if (_duoHost == "" || _duoIkey == "" || _duoSkey == "" || _duoUser == "" || !_preferences->getBool(preference_update_time, false))
        {
            _duoEnabled = false;
        }
        else if (_preferences->getBool(preference_cred_bypass_boot_btn_enabled, false) || _preferences->getInt(preference_cred_bypass_gpio_high, -1) > -1  || _preferences->getInt(preference_cred_bypass_gpio_low, -1) > -1)
        {
            if (_preferences->getBool(preference_cred_bypass_boot_btn_enabled, false))
            {
                _bypassGPIO = true;
            }
            _bypassGPIOHigh = _preferences->getInt(preference_cred_bypass_gpio_high, -1);
            _bypassGPIOLow = _preferences->getInt(preference_cred_bypass_gpio_low, -1);
        }
    }

    _totpKey = _preferences->getString(preference_totp_secret, "");
    _totpEnabled = _totpKey.length() > 0;
    _bypassKey = _preferences->getString(preference_bypass_secret, "");
    _bypassEnabled = _bypassKey.length() > 0;
    _updateTime = _preferences->getBool(preference_update_time, false);
}

bool ImportExport::getDuoEnabled()
{
    return _duoEnabled;
}

bool ImportExport::getTOTPEnabled()
{
    return _totpEnabled;
}

bool ImportExport::getBypassEnabled()
{
    return _bypassEnabled;
}

bool ImportExport::getBypassGPIOEnabled()
{
    return _bypassGPIO;
}

int ImportExport::getBypassGPIOHigh()
{
    return _bypassGPIOHigh;
}
int ImportExport::getBypassGPIOLow()
{
    return _bypassGPIOLow;
}

bool ImportExport::startDuoAuth(char* pushType)
{
    int64_t timeout = esp_timer_get_time() - (30 * 1000 * 1000L);
    if(!_duoActiveRequest || timeout > _duoRequestTS)
    {
        const char* duo_host = _duoHost.c_str();
        const char* duo_ikey = _duoIkey.c_str();
        const char* duo_skey = _duoSkey.c_str();
        const char* duo_user = _duoUser.c_str();

        DuoAuthLib duoAuth;
        bool duoRequestResult;
        duoAuth.begin(duo_host, duo_ikey, duo_skey, &timeinfo);
        duoAuth.setPushType(pushType);
        duoRequestResult = duoAuth.pushAuth((char*)duo_user, true);

        if(duoRequestResult == true)
        {
            _duoTransactionId = duoAuth.getAuthTxId();
            _duoActiveRequest = true;
            _duoRequestTS = esp_timer_get_time();
            Log->println("Duo MFA Auth sent");
            return true;
        }
        else
        {
            Log->println("Failed Duo MFA Auth");
            return false;
        }
    }
    return true;
}

void ImportExport::setDuoCheckIP(String duoCheckIP)
{
    _duoCheckIP = duoCheckIP;
}

void ImportExport::setDuoCheckId(String duoCheckId)
{
    _duoCheckId = duoCheckId;
}

void ImportExport::saveSessions()
{
    if(_updateTime)
    {
        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
        }
        else
        {
            File file;
            file = SPIFFS.open("/duosessions.json", "w");
            serializeJson(_duoSessions, file);
            file.close();
        }
    }
}

int ImportExport::checkDuoAuth(PsychicRequest *request)
{
    const char* duo_host = _duoHost.c_str();
    const char* duo_ikey = _duoIkey.c_str();
    const char* duo_skey = _duoSkey.c_str();
    const char* duo_user = _duoUser.c_str();

    int type = 0;
    if(request->hasParam("type"))
    {
        const PsychicWebParameter* p = request->getParam("type");
        if(p->value() != "")
        {
            type = p->value().toInt();
        }
    }

    if (request->hasParam("id"))
    {
        const PsychicWebParameter* p = request->getParam("id");
        String id = p->value();
        DuoAuthLib duoAuth;
        if(_duoActiveRequest && _duoCheckIP == request->client()->localIP().toString() && id == _duoCheckId)
        {
            duoAuth.begin(duo_host, duo_ikey, duo_skey, &timeinfo);

            Log->println("Checking Duo Push Status...");
            duoAuth.authStatus(_duoTransactionId);

            if(duoAuth.pushWaiting())
            {
                Log->println("Duo Push Waiting...");
                return 2;
            }
            else
            {
                if (duoAuth.authSuccessful())
                {
                    Log->println("Successful Duo MFA Auth");
                    _duoActiveRequest = false;
                    _duoTransactionId = "";
                    _duoCheckIP = "";
                    _duoCheckId = "";

                    if(type==0)
                    {
                        int64_t durationLength = 60*60*_preferences->getInt(preference_cred_session_lifetime_duo_remember, 720);

                        if (!_sessionsOpts[request->client()->localIP().toString()])
                        {
                            durationLength = _preferences->getInt(preference_cred_session_lifetime_duo, 3600);
                        }
                        struct timeval time;
                        gettimeofday(&time, NULL);
                        int64_t time_us = (int64_t)time.tv_sec * 1000000L + (int64_t)time.tv_usec;
                        _duoSessions[id] = time_us + (durationLength*1000000L);
                        saveSessions();
                        if (_preferences->getBool(preference_mfa_reconfigure, false))
                        {
                            _preferences->putBool(preference_mfa_reconfigure, false);
                        }
                    }
                    else
                    {
                        _sessionsOpts[request->client()->localIP().toString() + "approve"] = true;
                    }
                    return 1;
                }
                else
                {
                    Log->println("Failed Duo MFA Auth");
                    _duoActiveRequest = false;
                    _duoTransactionId = "";
                    _duoCheckIP = "";
                    _duoCheckId = "";

                    if(type==0)
                    {
                        if (_preferences->getBool(preference_mfa_reconfigure, false))
                        {
                            _preferences->putBool(preference_cred_duo_enabled, false);
                            _duoEnabled = false;
                            _preferences->putBool(preference_mfa_reconfigure, false);
                        }
                    }
                    else
                    {
                        _sessionsOpts[request->client()->localIP().toString() + "approve"] = false;
                    }
                    return 0;
                }
            }
        }
    }
    return 0;
}

int ImportExport::checkDuoApprove()
{
    const char* duo_host = _duoHost.c_str();
    const char* duo_ikey = _duoIkey.c_str();
    const char* duo_skey = _duoSkey.c_str();
    const char* duo_user = _duoUser.c_str();

    DuoAuthLib duoAuth;
    if(_duoActiveRequest)
    {
        duoAuth.begin(duo_host, duo_ikey, duo_skey, &timeinfo);

        Log->println("Checking Duo Push Status...");
        duoAuth.authStatus(_duoTransactionId);

        if(duoAuth.pushWaiting())
        {
            Log->println("Duo Push Waiting...");
            return 2;
        }
        else
        {
            if (duoAuth.authSuccessful())
            {
                Log->println("Successful Duo MFA Auth");
                _duoActiveRequest = false;
                _duoTransactionId = "";
                _duoCheckIP = "";
                _duoCheckId = "";
                return 1;
            }
            else
            {
                Log->println("Failed Duo MFA Auth");
                _duoActiveRequest = false;
                _duoTransactionId = "";
                _duoCheckIP = "";
                _duoCheckId = "";
                return 0;
            }
        }
    }
    return 0;
}

bool ImportExport::checkTOTP(String* totpKey)
{
    if(_totpEnabled)
    {
        if((pow(_invalidCount, 5) + _lastCodeCheck) > espMillis())
        {
            _lastCodeCheck = espMillis();
            return false;
        }

        _lastCodeCheck = espMillis();

        String key(totpKey->c_str());

        time_t now;
        time(&now);
        int totpTime = -60;

        while (totpTime <= 60)
        {
            String key2(TOTP::currentOTP(now, _totpKey, 30, 6, totpTime)->c_str());

            if(key.toInt() == key2.toInt())
            {
                _invalidCount = 0;
                Log->println("Successful TOTP MFA Auth");
                return true;
            }
            totpTime += 30;
        }
        _invalidCount++;
        Log->println("Failed TOTP MFA Auth");
    }
    return false;
}

bool ImportExport::checkBypass(String bypass)
{
    if(_bypassEnabled)
    {
        if((pow(_invalidCount2, 5) + _lastCodeCheck2) > espMillis())
        {
            _lastCodeCheck2 = espMillis();
            return false;
        }

        _lastCodeCheck2 = espMillis();

        if(bypass == _bypassKey)
        {
            _invalidCount2 = 0;
            Log->println("Successful Bypass MFA Auth");
            return true;
        }
        _invalidCount2++;
        Log->println("Failed Bypass MFA Auth");
    }
    return false;
}

void ImportExport::exportHttpsJson(JsonDocument &json)
{
    if (!SPIFFS.begin(true))
    {
        Log->println("SPIFFS Mount Failed");
    }
    else
    {
        File file = SPIFFS.open("/http_ssl.crt");
        if (!file || file.isDirectory())
        {
            Log->println("http_ssl.crt not found");
        }
        else
        {
            Log->println("Reading http_ssl.crt");
            size_t filesize = file.size();
            char cert[filesize + 1];

            file.read((uint8_t *)cert, sizeof(cert));
            file.close();
            cert[filesize] = '\0';
            json["http_ssl.crt"] = cert;
        }
    }

    if (!SPIFFS.begin(true))
    {
        Log->println("SPIFFS Mount Failed");
    }
    else
    {
        File file = SPIFFS.open("/http_ssl.key");
        if (!file || file.isDirectory())
        {
            Log->println("http_ssl.key not found");
        }
        else
        {
            Log->println("Reading http_ssl.key");
            size_t filesize = file.size();
            char key[filesize + 1];

            file.read((uint8_t *)key, sizeof(key));
            file.close();
            key[filesize] = '\0';
            json["http_ssl.key"] = key;
        }
    }
}

void ImportExport::exportMqttsJson(JsonDocument &json)
{
    if (!SPIFFS.begin(true))
    {
        Log->println("SPIFFS Mount Failed");
    }
    else
    {
        File file = SPIFFS.open("/mqtt_ssl.ca");
        if (!file || file.isDirectory())
        {
            Log->println("mqtt_ssl.ca not found");
        }
        else
        {
            Log->println("Reading mqtt_ssl.ca");
            size_t filesize = file.size();
            char ca[filesize + 1];

            file.read((uint8_t *)ca, sizeof(ca));
            file.close();
            ca[filesize] = '\0';
            json["mqtt_ssl.ca"] = ca;
        }
    }

    if (!SPIFFS.begin(true))
    {
        Log->println("SPIFFS Mount Failed");
    }
    else
    {
        File file = SPIFFS.open("/mqtt_ssl.crt");
        if (!file || file.isDirectory())
        {
            Log->println("mqtt_ssl.crt not found");
        }
        else
        {
            Log->println("Reading mqtt_ssl.crt");
            size_t filesize = file.size();
            char cert[filesize + 1];

            file.read((uint8_t *)cert, sizeof(cert));
            file.close();
            cert[filesize] = '\0';
            json["mqtt_ssl.crt"] = cert;
        }
    }

    if (!SPIFFS.begin(true))
    {
        Log->println("SPIFFS Mount Failed");
    }
    else
    {
        File file = SPIFFS.open("/mqtt_ssl.key");
        if (!file || file.isDirectory())
        {
            Log->println("mqtt_ssl.key not found");
        }
        else
        {
            Log->println("Reading mqtt_ssl.key");
            size_t filesize = file.size();
            char key[filesize + 1];

            file.read((uint8_t *)key, sizeof(key));
            file.close();
            key[filesize] = '\0';
            json["mqtt_ssl.key"] = key;
        }
    }
}

void ImportExport::exportNukiHubJson(JsonDocument &json, bool redacted, bool pairing, bool nuki, bool nukiOpener)
{
    DebugPreferences debugPreferences;

    const std::vector<char*> keysPrefs = debugPreferences.getPreferencesKeys();
    const std::vector<char*> boolPrefs = debugPreferences.getPreferencesBoolKeys();
    const std::vector<char*> redactedPrefs = debugPreferences.getPreferencesRedactedKeys();
    const std::vector<char*> bytePrefs = debugPreferences.getPreferencesByteKeys();

    for(const auto& key : keysPrefs)
    {
        if(strcmp(key, preference_show_secrets) == 0)
        {
            continue;
        }
        if(strcmp(key, preference_latest_version) == 0)
        {
            continue;
        }
        if(strcmp(key, preference_totp_secret) == 0)
        {
            continue;
        }
        if(strcmp(key, preference_bypass_secret) == 0)
        {
            continue;
        }
        if(strcmp(key, preference_admin_secret) == 0)
        {
            continue;
        }
        if(!redacted) if(std::find(redactedPrefs.begin(), redactedPrefs.end(), key) != redactedPrefs.end())
            {
                continue;
            }
        if(!_preferences->isKey(key))
        {
            json[key] = "";
        }
        else if(std::find(boolPrefs.begin(), boolPrefs.end(), key) != boolPrefs.end())
        {
            json[key] = _preferences->getBool(key) ? "1" : "0";
        }
        else
        {
            switch(_preferences->getType(key))
            {
            case PT_I8:
                json[key] = String(_preferences->getChar(key));
                break;
            case PT_I16:
                json[key] = String(_preferences->getShort(key));
                break;
            case PT_I32:
                json[key] = String(_preferences->getInt(key));
                break;
            case PT_I64:
                json[key] = String(_preferences->getLong64(key));
                break;
            case PT_U8:
                json[key] = String(_preferences->getUChar(key));
                break;
            case PT_U16:
                json[key] = String(_preferences->getUShort(key));
                break;
            case PT_U32:
                json[key] = String(_preferences->getUInt(key));
                break;
            case PT_U64:
                json[key] = String(_preferences->getULong64(key));
                break;
            case PT_STR:
                json[key] = _preferences->getString(key);
                break;
            default:
                json[key] = _preferences->getString(key);
                break;
            }
        }
    }

    if(pairing)
    {
        if(nuki)
        {
            unsigned char currentBleAddress[6];
            unsigned char authorizationId[4] = {0x00};
            unsigned char secretKeyK[32] = {0x00};
            uint16_t storedPincode = 0000;
            uint32_t storedUltraPincode = 000000;
            bool isUltra = false;
            Preferences nukiBlePref;
            nukiBlePref.begin("NukiHub", false);
            nukiBlePref.getBytes("bleAddress", currentBleAddress, 6);
            nukiBlePref.getBytes("secretKeyK", secretKeyK, 32);
            nukiBlePref.getBytes("authorizationId", authorizationId, 4);
            nukiBlePref.getBytes("securityPinCode", &storedPincode, 2);
            nukiBlePref.getBytes("ultraPinCode", &storedUltraPincode, 4);
            isUltra = nukiBlePref.getBool("isUltra", false);
            nukiBlePref.end();
            char text[255];
            text[0] = '\0';
            for(int i = 0 ; i < 6 ; i++)
            {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", currentBleAddress[i]);
            }
            json["bleAddressLock"] = text;
            memset(text, 0, sizeof(text));
            text[0] = '\0';
            for(int i = 0 ; i < 32 ; i++)
            {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", secretKeyK[i]);
            }
            json["secretKeyKLock"] = text;
            memset(text, 0, sizeof(text));
            text[0] = '\0';
            for(int i = 0 ; i < 4 ; i++)
            {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", authorizationId[i]);
            }
            json["authorizationIdLock"] = text;
            memset(text, 0, sizeof(text));
            json["securityPinCodeLock"] = storedPincode;
            json["ultraPinCodeLock"] = storedUltraPincode;
            json["isUltra"] = isUltra ? "1" : "0";
        }
        if(nukiOpener)
        {
            unsigned char currentBleAddressOpn[6];
            unsigned char authorizationIdOpn[4] = {0x00};
            unsigned char secretKeyKOpn[32] = {0x00};
            uint16_t storedPincodeOpn = 0000;
            Preferences nukiBlePref;
            nukiBlePref.begin("NukiHubopener", false);
            nukiBlePref.getBytes("bleAddress", currentBleAddressOpn, 6);
            nukiBlePref.getBytes("secretKeyK", secretKeyKOpn, 32);
            nukiBlePref.getBytes("authorizationId", authorizationIdOpn, 4);
            nukiBlePref.getBytes("securityPinCode", &storedPincodeOpn, 2);
            nukiBlePref.end();
            char text[255];
            text[0] = '\0';
            for(int i = 0 ; i < 6 ; i++)
            {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", currentBleAddressOpn[i]);
            }
            json["bleAddressOpener"] = text;
            memset(text, 0, sizeof(text));
            text[0] = '\0';
            for(int i = 0 ; i < 32 ; i++)
            {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", secretKeyKOpn[i]);
            }
            json["secretKeyKOpener"] = text;
            memset(text, 0, sizeof(text));
            text[0] = '\0';
            for(int i = 0 ; i < 4 ; i++)
            {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", authorizationIdOpn[i]);
            }
            json["authorizationIdOpener"] = text;
            memset(text, 0, sizeof(text));
            json["securityPinCodeOpener"] = storedPincodeOpn;
        }
    }

    for(const auto& key : bytePrefs)
    {
        size_t storedLength = _preferences->getBytesLength(key);
        if(storedLength == 0)
        {
            continue;
        }
        uint8_t serialized[storedLength];
        memset(serialized, 0, sizeof(serialized));
        size_t size = _preferences->getBytes(key, serialized, sizeof(serialized));
        if(size == 0)
        {
            continue;
        }
        char text[255];
        text[0] = '\0';
        for(int i = 0 ; i < size ; i++)
        {
            size_t offset = strlen(text);
            sprintf(&(text[offset]), "%02x", serialized[i]);
        }
        json[key] = text;
        memset(text, 0, sizeof(text));
    }
}

JsonDocument ImportExport::importJson(JsonDocument &doc)
{
    JsonDocument json;
    unsigned char currentBleAddress[6];
    unsigned char authorizationId[4] = {0x00};
    unsigned char secretKeyK[32] = {0x00};
    unsigned char currentBleAddressOpn[6];
    unsigned char authorizationIdOpn[4] = {0x00};
    unsigned char secretKeyKOpn[32] = {0x00};

    DebugPreferences debugPreferences;

    const std::vector<char*> keysPrefs = debugPreferences.getPreferencesKeys();
    const std::vector<char*> boolPrefs = debugPreferences.getPreferencesBoolKeys();
    const std::vector<char*> bytePrefs = debugPreferences.getPreferencesByteKeys();
    const std::vector<char*> intPrefs = debugPreferences.getPreferencesIntKeys();
    const std::vector<char*> uintPrefs = debugPreferences.getPreferencesUIntKeys();
    const std::vector<char*> uint64Prefs = debugPreferences.getPreferencesUInt64Keys();

    for(const auto& key : keysPrefs)
    {
        if(doc[key].isNull())
        {
            continue;
        }
        if(strcmp(key, preference_show_secrets) == 0)
        {
            continue;
        }
        if(strcmp(key, preference_latest_version) == 0)
        {
            continue;
        }
        if(std::find(boolPrefs.begin(), boolPrefs.end(), key) != boolPrefs.end())
        {
            if (doc[key].as<String>().length() > 0)
            {
                _preferences->putBool(key, (doc[key].as<String>() == "1" ? true : false));
                json[key] = "changed";
            }
            else
            {
                json[key] = "removed";
                _preferences->remove(key);
            }
            continue;
        }
        if(std::find(intPrefs.begin(), intPrefs.end(), key) != intPrefs.end())
        {
            if (doc[key].as<String>().length() > 0)
            {
                json[key] = "changed";
                _preferences->putInt(key, doc[key].as<int>());
            }
            else
            {
                json[key] = "removed";
                _preferences->remove(key);
            }
            continue;
        }
        if(std::find(uintPrefs.begin(), uintPrefs.end(), key) != uintPrefs.end())
        {
            if (doc[key].as<String>().length() > 0)
            {
                json[key] = "changed";
                _preferences->putUInt(key, doc[key].as<uint32_t>());
            }
            else
            {
                json[key] = "removed";
                _preferences->remove(key);
            }
            continue;
        }
        if(std::find(uint64Prefs.begin(), uint64Prefs.end(), key) != uint64Prefs.end())
        {
            if (doc[key].as<String>().length() > 0)
            {
                json[key] = "changed";
                _preferences->putULong64(key, doc[key].as<uint64_t>());
            }
            else
            {
                json[key] = "removed";
                _preferences->remove(key);
            }
            continue;
        }
        if (doc[key].as<String>().length() > 0)
        {
            json[key] = "changed";
            _preferences->putString(key, doc[key].as<String>());
        }
        else
        {
            json[key] = "removed";
            _preferences->remove(key);
        }
    }

    for(const auto& key : bytePrefs)
    {
        if(!doc[key].isNull() && doc[key].is<JsonVariant>())
        {
            String value = doc[key].as<String>();
            unsigned char tmpchar[32];
            for(int i=0; i<value.length(); i+=2)
            {
                tmpchar[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
            }
            json[key] = "changed";
            _preferences->putBytes(key, (byte*)(&tmpchar), (value.length() / 2));
            memset(tmpchar, 0, sizeof(tmpchar));
        }
    }

    Preferences nukiBlePref;
    nukiBlePref.begin("NukiHub", false);

    if(!doc["bleAddressLock"].isNull())
    {
        if (doc["bleAddressLock"].as<String>().length() == 12)
        {
            String value = doc["bleAddressLock"].as<String>();
            for(int i=0; i<value.length(); i+=2)
            {
                currentBleAddress[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
            }
            json["bleAddressLock"] = "changed";
            nukiBlePref.putBytes("bleAddress", currentBleAddress, 6);
        }
    }
    if(!doc["secretKeyKLock"].isNull())
    {
        if (doc["secretKeyKLock"].as<String>().length() == 64)
        {
            String value = doc["secretKeyKLock"].as<String>();
            for(int i=0; i<value.length(); i+=2)
            {
                secretKeyK[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
            }
            json["secretKeyKLock"] = "changed";
            nukiBlePref.putBytes("secretKeyK", secretKeyK, 32);
        }
    }
    if(!doc["authorizationIdLock"].isNull())
    {
        if (doc["authorizationIdLock"].as<String>().length() == 8)
        {
            String value = doc["authorizationIdLock"].as<String>();
            for(int i=0; i<value.length(); i+=2)
            {
                authorizationId[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
            }
            json["authorizationIdLock"] = "changed";
            nukiBlePref.putBytes("authorizationId", authorizationId, 4);
        }
    }
    if(!doc["isUltra"].isNull())
    {
        if (doc["isUltra"].as<String>().length() >0)
        {
            json["isUltra"] = "changed";
            nukiBlePref.putBool("isUltra", (doc["isUltra"].as<String>() == "1" ? true : false));
        }
    }
    if(!doc["securityPinCodeLock"].isNull())
    {
        if(doc["securityPinCodeLock"].as<String>().length() > 0)
        {
            json["securityPinCodeLock"] = "changed";
            nukiBlePref.putBytes("securityPinCode", (byte*)(doc["securityPinCodeLock"].as<int>()), 2);
            //_nuki->setPin(doc["securityPinCodeLock"].as<int>());
        }
        else
        {
            json["securityPinCodeLock"] = "removed";
            unsigned char pincode[2] = {0x00};
            nukiBlePref.putBytes("securityPinCode", pincode, 2);
            //_nuki->setPin(0xffff);
        }
    }
    if(!doc["ultraPinCodeLock"].isNull())
    {
        if(doc["ultraPinCodeLock"].as<String>().length() > 0)
        {
            json["ultraPinCodeLock"] = "changed";
            nukiBlePref.putBytes("ultraPinCode", (byte*)(doc["ultraPinCodeLock"].as<int>()), 4);
            //_nuki->setUltraPin(doc["ultraPinCodeLock"].as<int>());
            _preferences->putInt(preference_lock_gemini_pin, doc["ultraPinCodeLock"].as<int>());
        }
        else
        {
            json["ultraPinCodeLock"] = "removed";
            unsigned char ultraPincode[4] = {0x00};
            nukiBlePref.putBytes("ultraPinCode", ultraPincode, 4);
            _preferences->putInt(preference_lock_gemini_pin, 0);
        }
    }
    nukiBlePref.end();
    nukiBlePref.begin("NukiHubopener", false);
    if(!doc["bleAddressOpener"].isNull())
    {
        if (doc["bleAddressOpener"].as<String>().length() == 12)
        {
            String value = doc["bleAddressOpener"].as<String>();
            for(int i=0; i<value.length(); i+=2)
            {
                currentBleAddressOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
            }
            json["bleAddressOpener"] = "changed";
            nukiBlePref.putBytes("bleAddress", currentBleAddressOpn, 6);
        }
    }
    if(!doc["secretKeyKOpener"].isNull())
    {
        if (doc["secretKeyKOpener"].as<String>().length() == 64)
        {
            String value = doc["secretKeyKOpener"].as<String>();
            for(int i=0; i<value.length(); i+=2)
            {
                secretKeyKOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
            }
            json["secretKeyKOpener"] = "changed";
            nukiBlePref.putBytes("secretKeyK", secretKeyKOpn, 32);
        }
    }
    if(!doc["authorizationIdOpener"].isNull())
    {
        if (doc["authorizationIdOpener"].as<String>().length() == 8)
        {
            String value = doc["authorizationIdOpener"].as<String>();
            for(int i=0; i<value.length(); i+=2)
            {
                authorizationIdOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
            }
            json["authorizationIdOpener"] = "changed";
            nukiBlePref.putBytes("authorizationId", authorizationIdOpn, 4);
        }
    }

    if(!doc["securityPinCodeOpener"].isNull())
    {
        if(doc["securityPinCodeOpener"].as<String>().length() > 0)
        {
            json["securityPinCodeOpener"] = "changed";
            nukiBlePref.putBytes("securityPinCode", (byte*)(doc["securityPinCodeOpener"].as<int>()), 2);
            //_nukiOpener->setPin(doc["securityPinCodeOpener"].as<int>());
        }
        else
        {
            json["securityPinCodeOpener"] = "removed";
            unsigned char pincode[2] = {0x00};
            nukiBlePref.putBytes("securityPinCode", pincode, 2);
            //_nukiOpener->setPin(0xffff);
        }
    }
    nukiBlePref.end();
    if(!doc["mqtt_ssl.ca"].isNull())
    {
        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
            json["mqtt_ssl.ca"] = "error";
        }
        else
        {
            if(doc["mqtt_ssl.ca"].as<String>().length() > 0)
            {
                File file = SPIFFS.open("/mqtt_ssl.ca", FILE_WRITE);
                if (!file)
                {
                    Log->println("Failed to open /mqtt_ssl.ca for writing");
                    json["mqtt_ssl.ca"] = "error";
                }
                else
                {
                    if (!file.print(doc["mqtt_ssl.ca"].as<String>()))
                    {
                        Log->println("Failed to write /mqtt_ssl.ca");
                        json["mqtt_ssl.crt"] = "error";
                    }
                    else
                    {
                        json["mqtt_ssl.ca"] = "changed";
                    }
                    file.close();
                }
            }
            else
            {
                if (!SPIFFS.remove("/mqtt_ssl.ca"))
                {
                    Log->println("Failed to delete /mqtt_ssl.ca");
                    json["mqtt_ssl.crt"] = "error";
                }
                else
                {
                    json["mqtt_ssl.ca"] = "removed";
                }
            }
        }
    }
    if(!doc["mqtt_ssl.crt"].isNull())
    {
        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
            json["mqtt_ssl.crt"] = "error";
        }
        else
        {
            if(doc["mqtt_ssl.crt"].as<String>().length() > 0)
            {
                File file = SPIFFS.open("/mqtt_ssl.crt", FILE_WRITE);
                if (!file)
                {
                    Log->println("Failed to open /mqtt_ssl.crt for writing");
                    json["mqtt_ssl.crt"] = "error";
                }
                else
                {
                    if (!file.print(doc["mqtt_ssl.crt"].as<String>()))
                    {
                        Log->println("Failed to write /mqtt_ssl.crt");
                        json["mqtt_ssl.crt"] = "error";
                    }
                    else
                    {
                        json["mqtt_ssl.crt"] = "changed";
                    }
                    file.close();
                }
            }
            else
            {
                if (!SPIFFS.remove("/mqtt_ssl.crt"))
                {
                    Log->println("Failed to delete /mqtt_ssl.crt");
                    json["mqtt_ssl.crt"] = "error";
                }
                else
                {
                    json["mqtt_ssl.crt"] = "removed";
                }
            }
        }
    }
    if(!doc["mqtt_ssl.key"].isNull())
    {
        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
            json["mqtt_ssl.key"] = "error";
        }
        else
        {
            if(doc["mqtt_ssl.key"].as<String>().length() > 0)
            {
                File file = SPIFFS.open("/mqtt_ssl.key", FILE_WRITE);
                if (!file)
                {
                    Log->println("Failed to open /mqtt_ssl.key for writing");
                    json["mqtt_ssl.key"] = "error";
                }
                else
                {
                    if (!file.print(doc["mqtt_ssl.key"].as<String>()))
                    {
                        Log->println("Failed to write /mqtt_ssl.key");
                        json["mqtt_ssl.key"] = "error";
                    }
                    else
                    {
                        json["mqtt_ssl.key"] = "changed";
                    }
                    file.close();
                }
            }
            else
            {
                if (!SPIFFS.remove("/mqtt_ssl.key"))
                {
                    Log->println("Failed to delete /mqtt_ssl.key");
                }
                else
                {
                    json["mqtt_ssl.key"] = "removed";
                }
            }
        }
    }
    if(!doc["http_ssl.crt"].isNull())
    {
        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
            json["http_ssl.crt"] = "error";
        }
        else
        {
            if(doc["http_ssl.crt"].as<String>().length() > 0)
            {
                File file = SPIFFS.open("/http_ssl.crt", FILE_WRITE);
                if (!file)
                {
                    Log->println("Failed to open /http_ssl.crt for writing");
                    json["http_ssl.crt"] = "error";
                }
                else
                {
                    if (!file.print(doc["http_ssl.crt"].as<String>()))
                    {
                        Log->println("Failed to write /http_ssl.crt");
                        json["http_ssl.crt"] = "error";
                    }
                    else
                    {
                        json["http_ssl.crt"] = "changed";
                    }
                    file.close();
                }
            }
            else
            {
                if (!SPIFFS.remove("/http_ssl.crt"))
                {
                    Log->println("Failed to delete /http_ssl.crt");
                    json["http_ssl.crt"] = "error";
                }
                else
                {
                    json["http_ssl.crt"] = "removed";
                }
            }
        }
    }
    if(!doc["http_ssl.key"].isNull())
    {
        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
            json["http_ssl.key"] = "error";
        }
        else
        {
            if(doc["http_ssl.key"].as<String>().length() > 0)
            {
                File file = SPIFFS.open("/http_ssl.key", FILE_WRITE);
                if (!file)
                {
                    Log->println("Failed to open /http_ssl.key for writing");
                    json["http_ssl.key"] = "error";
                }
                else
                {
                    if (!file.print(doc["http_ssl.key"].as<String>()))
                    {
                        Log->println("Failed to write /http_ssl.key");
                        json["http_ssl.key"] = "error";
                    }
                    else
                    {
                        json["http_ssl.key"] = "changed";
                    }
                    file.close();
                }
            }
            else
            {
                if (!SPIFFS.remove("/http_ssl.key"))
                {
                    Log->println("Failed to delete /http_ssl.key");
                    json["http_ssl.key"] = "error";
                }
                else
                {
                    json["http_ssl.key"] = "removed";
                }
            }
        }
    }

    return json;
}