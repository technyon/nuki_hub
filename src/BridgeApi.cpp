#include "BridgeApi.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "Config.h"
#include "EspMillis.h"
#include "RestartReason.h"
#include "esp_http_client.h"
#include "mbedtls/sha256.h"

BridgeApi::BridgeApi(NukiWrapper* nuki, NukiOpenerWrapper* nukiOpener, NukiNetwork* network, NukiNetworkOpener* networkOpener, Preferences* preferences)
    : _nuki(nuki),
      _nukiOpener(nukiOpener),
      _network(network),
      _networkOpener(networkOpener),
      _preferences(preferences)
{
}

void BridgeApi::initialize(PsychicHttpServer* server)
{
    server->on("/info", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
    {
        if(!checkToken(request))
        {
            return sendUnauthorized(resp);
        }

        JsonDocument json;
        json["bridgeType"] = 1;
        json["ids"]["hardwareId"] = _preferences->getULong64(preference_nukihub_id, 0);
        json["ids"]["serverId"] = 0;
        json["versions"]["firmwareVersion"] = NUKI_HUB_VERSION;
        json["versions"]["wifiFirmwareVersion"] = "0";
        json["uptime"] = espMillis() / 1000;
        json["currentTime"] = timestamp(time(nullptr));
        json["serverConnected"] = false;
        JsonArray scanResults = json["scanResults"].to<JsonArray>();

        if(_nuki != nullptr && _nuki->isPaired())
        {
            JsonObject device = scanResults.add<JsonObject>();
            device["nukiId"] = _preferences->getUInt(preference_nuki_id_lock, 0);
            device["deviceType"] = 0;
            device["name"] = _nuki->name();
            device["rssi"] = 0;
            device["paired"] = true;
        }
        if(_nukiOpener != nullptr && _nukiOpener->isPaired())
        {
            JsonObject device = scanResults.add<JsonObject>();
            device["nukiId"] = _preferences->getUInt(preference_nuki_id_opener, 0);
            device["deviceType"] = 2;
            device["name"] = _nukiOpener->name();
            device["rssi"] = 0;
            device["paired"] = true;
        }

        return sendJson(resp, json, 200);
    });

    server->on("/list", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
    {
        if(!checkToken(request))
        {
            return sendUnauthorized(resp);
        }

        JsonDocument json;
        json.to<JsonArray>();

        if(_nuki != nullptr && _nuki->isPaired())
        {
            JsonObject device = json.add<JsonObject>();
            device["nukiId"] = _preferences->getUInt(preference_nuki_id_lock, 0);
            device["deviceType"] = 0;
            device["name"] = _nuki->name();
            device["firmwareVersion"] = _nuki->firmwareVersion();
            deviceState(0, device["lastKnownState"].to<JsonObject>());
        }
        if(_nukiOpener != nullptr && _nukiOpener->isPaired())
        {
            JsonObject device = json.add<JsonObject>();
            device["nukiId"] = _preferences->getUInt(preference_nuki_id_opener, 0);
            device["deviceType"] = 2;
            device["name"] = _nukiOpener->name();
            device["firmwareVersion"] = _nukiOpener->firmwareVersion();
            deviceState(2, device["lastKnownState"].to<JsonObject>());
        }

        return sendJson(resp, json, 200);
    });

    server->on("/lockState", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
    {
        if(!checkToken(request))
        {
            return sendUnauthorized(resp);
        }

        int type = deviceType(request);
        JsonDocument json;

        if(type < 0)
        {
            json["success"] = false;
            return sendJson(resp, json, 404);
        }

        deviceState(type, json.to<JsonObject>());
        json["success"] = true;
        return sendJson(resp, json, 200);
    });

    server->on("/lockAction", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
    {
        if(!checkToken(request))
        {
            return sendUnauthorized(resp);
        }

        int type = deviceType(request);
        JsonDocument json;

        if(type < 0 || !request->hasParam("action"))
        {
            json["success"] = false;
            return sendJson(resp, json, type < 0 ? 404 : 400);
        }

        LockActionResult result = lockAction(type, request->getParam("action")->value().toInt());
        json["success"] = result == LockActionResult::Success;
        json["batteryCritical"] = (deviceStateBattery(type) & 1) == 1;
        return sendJson(resp, json, 200);
    });

    server->on("/lock", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
    {
        if(!checkToken(request))
        {
            return sendUnauthorized(resp);
        }

        int type = deviceType(request);
        JsonDocument json;

        if(type < 0)
        {
            json["success"] = false;
            return sendJson(resp, json, 404);
        }

        json["success"] = lockAction(type, 2) == LockActionResult::Success;
        json["batteryCritical"] = (deviceStateBattery(type) & 1) == 1;
        return sendJson(resp, json, 200);
    });

    server->on("/unlock", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
    {
        if(!checkToken(request))
        {
            return sendUnauthorized(resp);
        }

        int type = deviceType(request);
        JsonDocument json;

        if(type < 0)
        {
            json["success"] = false;
            return sendJson(resp, json, 404);
        }

        json["success"] = lockAction(type, 1) == LockActionResult::Success;
        json["batteryCritical"] = (deviceStateBattery(type) & 1) == 1;
        return sendJson(resp, json, 200);
    });

    server->on("/callback/add", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
    {
        if(!checkToken(request))
        {
            return sendUnauthorized(resp);
        }

        JsonDocument callbacks;
        loadCallbacks(callbacks);
        JsonDocument json;

        if(!request->hasParam("url") || request->getParam("url")->value().length() == 0 || request->getParam("url")->value().length() > 254)
        {
            json["success"] = false;
            json["message"] = "url missing or invalid";
            return sendJson(resp, json, 400);
        }
        if(callbacks.size() >= 3)
        {
            json["success"] = false;
            json["message"] = "callback list full";
            return sendJson(resp, json, 400);
        }

        for(JsonVariant url : callbacks.as<JsonArray>())
        {
            if(url.as<String>() == request->getParam("url")->value())
            {
                json["success"] = true;
                return sendJson(resp, json, 200);
            }
        }

        callbacks.add(request->getParam("url")->value());
        saveCallbacks(callbacks);
        json["success"] = true;
        return sendJson(resp, json, 200);
    });

    server->on("/callback/list", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
    {
        if(!checkToken(request))
        {
            return sendUnauthorized(resp);
        }

        JsonDocument callbacks;
        loadCallbacks(callbacks);
        JsonDocument json;
        JsonArray list = json["callbacks"].to<JsonArray>();
        int id = 0;

        for(JsonVariant url : callbacks.as<JsonArray>())
        {
            JsonObject entry = list.add<JsonObject>();
            entry["id"] = id;
            entry["url"] = url.as<String>();
            ++id;
        }

        return sendJson(resp, json, 200);
    });

    server->on("/callback/remove", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
    {
        if(!checkToken(request))
        {
            return sendUnauthorized(resp);
        }

        JsonDocument callbacks;
        loadCallbacks(callbacks);
        JsonDocument json;

        if(!request->hasParam("id") || request->getParam("id")->value().toInt() < 0 || request->getParam("id")->value().toInt() >= callbacks.size())
        {
            json["success"] = false;
            json["message"] = "id missing or invalid";
            return sendJson(resp, json, 400);
        }

        callbacks.remove(request->getParam("id")->value().toInt());
        saveCallbacks(callbacks);
        json["success"] = true;
        return sendJson(resp, json, 200);
    });

    server->on("/log", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
    {
        if(!checkToken(request))
        {
            return sendUnauthorized(resp);
        }

        JsonDocument json;
        json.to<JsonArray>();
        return sendJson(resp, json, 200);
    });

    server->on("/reboot", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
    {
        if(!checkToken(request))
        {
            return sendUnauthorized(resp);
        }

        JsonDocument json;
        json["success"] = true;
        esp_err_t res = sendJson(resp, json, 200);
        espDelayAck(200);
        restartEsp(RestartReason::RequestedViaWebServer);
        return res;
    });

    server->on("/fwupdate", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
    {
        if(!checkToken(request))
        {
            return sendUnauthorized(resp);
        }

        JsonDocument json;
        json["success"] = false;
        return sendJson(resp, json, 200);
    });

    Log->println("Nuki Bridge HTTP API enabled");
}

void BridgeApi::update()
{
    int64_t ts = espMillis();

    if(ts < _nextCallbackCheckTs)
    {
        return;
    }
    _nextCallbackCheckTs = ts + 1000;

    JsonDocument callbacks;
    loadCallbacks(callbacks);

    if(callbacks.size() == 0)
    {
        return;
    }

    if(_nuki != nullptr && _nuki->isPaired())
    {
        JsonDocument json;
        json["nukiId"] = _preferences->getUInt(preference_nuki_id_lock, 0);
        json["deviceType"] = 0;
        deviceState(0, json.as<JsonObject>());
        json.remove("timestamp");
        String payload;
        serializeJson(json, payload);

        if(payload != _lastLockCallback)
        {
            _lastLockCallback = payload;
            for(JsonVariant url : callbacks.as<JsonArray>())
            {
                sendCallback(url.as<const char*>(), payload);
            }
        }
    }

    if(_nukiOpener != nullptr && _nukiOpener->isPaired())
    {
        JsonDocument json;
        json["nukiId"] = _preferences->getUInt(preference_nuki_id_opener, 0);
        json["deviceType"] = 2;
        deviceState(2, json.as<JsonObject>());
        json.remove("timestamp");
        String payload;
        serializeJson(json, payload);

        if(payload != _lastOpenerCallback)
        {
            _lastOpenerCallback = payload;
            for(JsonVariant url : callbacks.as<JsonArray>())
            {
                sendCallback(url.as<const char*>(), payload);
            }
        }
    }
}

bool BridgeApi::checkToken(PsychicRequest* request)
{
    String token = _preferences->getString(preference_bridge_api_token, "");

    if(token.length() == 0)
    {
        return false;
    }

    if(request->hasParam("token"))
    {
        return request->getParam("token")->value() == token;
    }

    if(request->hasParam("ts") && request->hasParam("rnr") && request->hasParam("hash"))
    {
        String data = request->getParam("ts")->value() + "," + request->getParam("rnr")->value() + "," + token;
        unsigned char digest[32];
        mbedtls_sha256((const unsigned char*)data.c_str(), data.length(), digest, 0);
        char hex[65];
        for(int i = 0; i < 32; i++)
        {
            sprintf(hex + i * 2, "%02x", digest[i]);
        }
        return request->getParam("hash")->value().equalsIgnoreCase(hex);
    }

    return false;
}

esp_err_t BridgeApi::sendJson(PsychicResponse* resp, JsonDocument& json, int code)
{
    String jsonStr;
    serializeJson(json, jsonStr);
    resp->setCode(code);
    resp->setContentType("application/json");
    resp->setContent(jsonStr.c_str());
    return resp->send();
}

esp_err_t BridgeApi::sendUnauthorized(PsychicResponse* resp)
{
    resp->setCode(401);
    resp->setContentType("text/plain");
    resp->setContent("HTTP 401 Unauthorized");
    return resp->send();
}

int BridgeApi::deviceType(PsychicRequest* request)
{
    uint32_t nukiId = request->hasParam("nukiId") ? strtoul(request->getParam("nukiId")->value().c_str(), nullptr, 10) : 0;

    if(_nuki != nullptr && _nuki->isPaired() && (nukiId == 0 || nukiId == _preferences->getUInt(preference_nuki_id_lock, 0)))
    {
        if(!request->hasParam("deviceType") || request->getParam("deviceType")->value().toInt() != 2)
        {
            return 0;
        }
    }
    if(_nukiOpener != nullptr && _nukiOpener->isPaired() && (nukiId == 0 || nukiId == _preferences->getUInt(preference_nuki_id_opener, 0)))
    {
        return 2;
    }

    return -1;
}

uint8_t BridgeApi::deviceStateBattery(int deviceType)
{
    if(deviceType == 2)
    {
        return _nukiOpener->keyTurnerState().criticalBatteryState;
    }
    return _nuki->keyTurnerState().criticalBatteryState;
}

void BridgeApi::deviceState(int deviceType, JsonObject json)
{
    if(deviceType == 2)
    {
        const NukiOpener::OpenerState& state = _nukiOpener->keyTurnerState();
        json["mode"] = state.nukiState == NukiOpener::State::ContinuousMode ? 3 : 2;
        json["state"] = (int)state.lockState;
        switch(state.lockState)
        {
        case NukiOpener::LockState::Uncalibrated:
            json["stateName"] = "untrained";
            break;
        case NukiOpener::LockState::Locked:
            json["stateName"] = "online";
            break;
        case NukiOpener::LockState::RTOactive:
            json["stateName"] = "rto active";
            break;
        case NukiOpener::LockState::Open:
            json["stateName"] = "open";
            break;
        case NukiOpener::LockState::Opening:
            json["stateName"] = "opening";
            break;
        default:
            json["stateName"] = "undefined";
            break;
        }
        json["batteryCritical"] = (state.criticalBatteryState & 1) == 1;
        json["keypadBatteryCritical"] = state.accessoryBatteryState != 255 && (state.accessoryBatteryState & 3) == 3;
        json["ringactionTimestamp"] = _networkOpener->lastRingTime() > 0 ? timestamp(_networkOpener->lastRingTime()) : "";
        json["ringactionState"] = _networkOpener->lastRingTs() > 0 && espMillis() - _networkOpener->lastRingTs() < 30000;
        json["timestamp"] = timestamp(time(nullptr));
        return;
    }

    const NukiLock::KeyTurnerState& state = _nuki->keyTurnerState();
    json["mode"] = 2;
    json["state"] = (int)state.lockState;
    switch(state.lockState)
    {
    case NukiLock::LockState::Uncalibrated:
        json["stateName"] = "uncalibrated";
        break;
    case NukiLock::LockState::Locked:
        json["stateName"] = "locked";
        break;
    case NukiLock::LockState::Unlocking:
        json["stateName"] = "unlocking";
        break;
    case NukiLock::LockState::Unlocked:
        json["stateName"] = "unlocked";
        break;
    case NukiLock::LockState::Locking:
        json["stateName"] = "locking";
        break;
    case NukiLock::LockState::Unlatched:
        json["stateName"] = "unlatched";
        break;
    case NukiLock::LockState::UnlockedLnga:
        json["stateName"] = "unlocked (lock 'n' go)";
        break;
    case NukiLock::LockState::Unlatching:
        json["stateName"] = "unlatching";
        break;
    case NukiLock::LockState::MotorBlocked:
        json["stateName"] = "motor blocked";
        break;
    default:
        json["stateName"] = "undefined";
        break;
    }
    json["batteryCritical"] = (state.criticalBatteryState & 1) == 1;
    json["batteryCharging"] = (state.criticalBatteryState & 2) == 2;
    json["batteryChargeState"] = (state.criticalBatteryState >> 2) * 2;
    json["keypadBatteryCritical"] = state.accessoryBatteryState != 255 && (state.accessoryBatteryState & 3) == 3;
    json["doorsensorState"] = (int)state.doorSensorState;
    switch(state.doorSensorState)
    {
    case NukiLock::DoorSensorState::Deactivated:
        json["doorsensorStateName"] = "deactivated";
        break;
    case NukiLock::DoorSensorState::DoorClosed:
        json["doorsensorStateName"] = "door closed";
        break;
    case NukiLock::DoorSensorState::DoorOpened:
        json["doorsensorStateName"] = "door opened";
        break;
    case NukiLock::DoorSensorState::DoorStateUnknown:
        json["doorsensorStateName"] = "door state unknown";
        break;
    case NukiLock::DoorSensorState::Calibrating:
        json["doorsensorStateName"] = "calibrating";
        break;
    default:
        json["doorsensorStateName"] = "unavailable";
        break;
    }
    json["timestamp"] = timestamp(time(nullptr));
}

LockActionResult BridgeApi::lockAction(int deviceType, int action)
{
    const char* lockActions[6] = {"", "unlock", "lock", "unlatch", "lockNgo", "lockNgoUnlatch"};
    const char* openerActions[6] = {"", "activateRTO", "deactivateRTO", "electricStrikeActuation", "activateCM", "deactivateCM"};

    if(action < 1 || action > 5)
    {
        return LockActionResult::UnknownAction;
    }

    if(deviceType == 2)
    {
        return _nukiOpener->onLockActionReceivedCallback(openerActions[action]);
    }
    return _nuki->onLockActionReceived(lockActions[action]);
}

void BridgeApi::loadCallbacks(JsonDocument& json)
{
    deserializeJson(json, _preferences->getString(preference_bridge_api_callbacks, "[]"));
    if(!json.is<JsonArray>())
    {
        json.to<JsonArray>();
    }
}

void BridgeApi::saveCallbacks(JsonDocument& json)
{
    String jsonStr;
    serializeJson(json, jsonStr);
    _preferences->putString(preference_bridge_api_callbacks, jsonStr);
}

void BridgeApi::sendCallback(const char* url, const String& payload)
{
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 3000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if(client == NULL)
    {
        return;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, payload.c_str(), payload.length());
    esp_err_t err = esp_http_client_perform(client);

    if(err != ESP_OK)
    {
        Log->print("Nuki Bridge API callback failed: ");
        Log->println(esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

String BridgeApi::timestamp(time_t time)
{
    tm tm;
    gmtime_r(&time, &tm);
    char str[26];
    strftime(str, sizeof(str), "%Y-%m-%dT%H:%M:%S+00:00", &tm);
    return String(str);
}
