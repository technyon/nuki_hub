#include "Network.h"
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include "Arduino.h"
#include "MqttTopics.h"
#include "PreferencesKeys.h"
#include "Pins.h"

Network* nwInst;

Network::Network(const NetworkDeviceType networkDevice, Preferences* preferences)
: _preferences(preferences)
{
    nwInst = this;

    _hostname = _preferences->getString(preference_hostname);
    setupDevice(networkDevice);

    _configTopics.reserve(5);
    _configTopics.push_back(mqtt_topic_config_button_enabled);
    _configTopics.push_back(mqtt_topic_config_led_enabled);
    _configTopics.push_back(mqtt_topic_config_led_brightness);
    _configTopics.push_back(mqtt_topic_config_auto_unlock);
    _configTopics.push_back(mqtt_topic_config_auto_lock);
}

Network::~Network()
{
    if(_device != nullptr)
    {
        delete _device;
        _device = nullptr;
    }
}

void Network::setupDevice(const NetworkDeviceType hardware)
{
    switch(hardware)
    {
        case NetworkDeviceType::W5500:
            Serial.println(F("Network device: W5500"));
            _device = new W5500Device(_hostname, _preferences);
            break;
        case NetworkDeviceType::WiFi:
            Serial.println(F("Network device: Builtin WiFi"));
            _device = new WifiDevice(_hostname);
            break;
        default:
            Serial.println(F("Unknown network device type, defaulting to WiFi"));
            _device = new WifiDevice(_hostname);
            break;
    }
}

void Network::initialize()
{
    if(_hostname == "")
    {
        _hostname = "nukihub";
        _preferences->putString(preference_hostname, _hostname);
    }

    _device->initialize();

    Serial.print(F("Host name: "));
    Serial.println(_hostname);

    const char* brokerAddr = _preferences->getString(preference_mqtt_broker).c_str();
    strcpy(_mqttBrokerAddr, brokerAddr);

    int port = _preferences->getInt(preference_mqtt_broker_port);
    if(port == 0)
    {
        port = 1883;
        _preferences->putInt(preference_mqtt_broker_port, port);
    }

    String mqttPath = _preferences->getString(preference_mqtt_lock_path);
    if(mqttPath.length() > 0)
    {
        size_t len = mqttPath.length();
        for(int i=0; i < len; i++)
        {
            _mqttPath[i] = mqttPath.charAt(i);
        }
    }
    else
    {
        strcpy(_mqttPath, "nuki");
        _preferences->putString(preference_mqtt_lock_path, _mqttPath);
    }

    String mqttOpenerPath = _preferences->getString(preference_mqtt_opener_path);
    if(mqttOpenerPath.length() > 0)
    {
        size_t len = mqttOpenerPath.length();
        for(int i=0; i < len; i++)
        {
            _mqttOpenerPath[i] = mqttOpenerPath.charAt(i);
        }
    }
    else
    {
        strcpy(_mqttOpenerPath, "nukiopener");
        _preferences->putString(preference_mqtt_opener_path, _mqttOpenerPath);
    }

    String mqttUser = _preferences->getString(preference_mqtt_user);
    if(mqttUser.length() > 0)
    {
        size_t len = mqttUser.length();
        for(int i=0; i < len; i++)
        {
            _mqttUser[i] = mqttUser.charAt(i);
        }
    }

    String mqttPass = _preferences->getString(preference_mqtt_password);
    if(mqttPass.length() > 0)
    {
        size_t len = mqttPass.length();
        for(int i=0; i < len; i++)
        {
            _mqttPass[i] = mqttPass.charAt(i);
        }
    }

    Serial.print(F("MQTT Broker: "));
    Serial.print(_mqttBrokerAddr);
    Serial.print(F(":"));
    Serial.println(port);

    _device->mqttClient()->setServer(_mqttBrokerAddr, port);
    _device->mqttClient()->setCallback(Network::onMqttDataReceivedCallback);

    _networkTimeout = _preferences->getInt(preference_network_timeout);
    if(_networkTimeout == 0)
    {
        _networkTimeout = -1;
        _preferences->putInt(preference_network_timeout, _networkTimeout);
    }
}

bool Network::reconnect()
{
    _mqttConnected = false;

    while (!_device->mqttClient()->connected() && millis() > _nextReconnect)
    {
        Serial.println(F("Attempting MQTT connection"));
        bool success = false;

        if(strlen(_mqttUser) == 0)
        {
            Serial.println(F("MQTT: Connecting without credentials"));
            success = _device->mqttClient()->connect(_preferences->getString(preference_hostname).c_str());
        }
        else
        {
            Serial.print(F("MQTT: Connecting with user: ")); Serial.println(_mqttUser);
            success = _device->mqttClient()->connect(_preferences->getString(preference_hostname).c_str(), _mqttUser, _mqttPass);
        }

        if (success)
        {
            Serial.println(F("MQTT connected"));
            _mqttConnected = true;
            delay(100);
            subscribe(mqtt_topic_lock_action);

            for(auto topic : _configTopics)
            {
                subscribe(topic);
            }
        }
        else
        {
            Serial.print(F("MQTT connect failed, rc="));
            Serial.println(_device->mqttClient()->state());
            _mqttConnected = false;
            _nextReconnect = millis() + 5000;
        }
    }
    return _mqttConnected;
}

void Network::update()
{
    long ts = millis();

    _device->update();

    if(!_device->isConnected())
    {
        Serial.println(F("Network not connected. Trying reconnect."));
        bool success = _device->reconnect();
        Serial.println(success ? F("Reconnect successful") : F("Reconnect failed"));
    }

    if(!_device->isConnected())
    {
        if(_networkTimeout > 0 && (ts - _lastConnectedTs > _networkTimeout * 1000))
        {
            Serial.println("Network timeout has been reached, restarting ...");
            delay(200);
            ESP.restart();
        }
        return;
    }

    _lastConnectedTs = ts;

    if(!_device->mqttClient()->connected())
    {
        bool success = reconnect();
        if(!success)
        {
            return;
        }
    }

    if(_presenceCsv != nullptr && strlen(_presenceCsv) > 0)
    {
        publishString(mqtt_topic_presence, _presenceCsv);
        _presenceCsv = nullptr;
    }

    _device->mqttClient()->loop();
}

void Network::onMqttDataReceivedCallback(char *topic, byte *payload, unsigned int length)
{
    nwInst->onMqttDataReceived(topic, payload, length);
}

void Network::onMqttDataReceived(char *&topic, byte *&payload, unsigned int &length)
{
    char value[50] = {0};
    size_t l = min(length, sizeof(value)-1);

    for(int i=0; i<l; i++)
    {
        value[i] = payload[i];
    }

    if(comparePrefixedPath(topic, mqtt_topic_lock_action))
    {
        if(strcmp(value, "") == 0 || strcmp(value, "ack") == 0 || strcmp(value, "unknown_action") == 0) return;

        Serial.print(F("Lock action received: "));
        Serial.println(value);
        bool success = false;
        if(_lockActionReceivedCallback != NULL)
        {
            success = _lockActionReceivedCallback(value);
        }
        publishString(mqtt_topic_lock_action, success ? "ack" : "unknown_action");
    }

    for(auto configTopic : _configTopics)
    {
        if(comparePrefixedPath(topic, configTopic))
        {
            if(_configUpdateReceivedCallback != nullptr)
            {
                _configUpdateReceivedCallback(configTopic, value);
            }
        }
    }
}

void Network::publishKeyTurnerState(const NukiLock::KeyTurnerState& keyTurnerState, const NukiLock::KeyTurnerState& lastKeyTurnerState)
{
    char str[50];

    if((_firstTunerStatePublish || keyTurnerState.lockState != lastKeyTurnerState.lockState) && keyTurnerState.lockState != NukiLock::LockState::Undefined)
    {
        memset(&str, 0, sizeof(str));
        lockstateToString(keyTurnerState.lockState, str);
        publishString(mqtt_topic_lock_state, str);
    }

    if(_firstTunerStatePublish || keyTurnerState.trigger != lastKeyTurnerState.trigger)
    {
        memset(&str, 0, sizeof(str));
        triggerToString(keyTurnerState.trigger, str);
        publishString(mqtt_topic_lock_trigger, str);
    }

    if(_firstTunerStatePublish || keyTurnerState.lastLockActionCompletionStatus != lastKeyTurnerState.lastLockActionCompletionStatus)
    {
        memset(&str, 0, sizeof(str));
        NukiLock::completionStatusToString(keyTurnerState.lastLockActionCompletionStatus, str);
        publishString(mqtt_topic_lock_completionStatus, str);
    }

    if(_firstTunerStatePublish || keyTurnerState.doorSensorState != lastKeyTurnerState.doorSensorState)
    {
        memset(&str, 0, sizeof(str));
        NukiLock::doorSensorStateToString(keyTurnerState.doorSensorState, str);
        publishString(mqtt_topic_door_sensor_state, str);
    }

    if(_firstTunerStatePublish || keyTurnerState.criticalBatteryState != lastKeyTurnerState.criticalBatteryState)
    {
        uint8_t level = (keyTurnerState.criticalBatteryState & 0b11111100) >> 1;
        bool critical = (keyTurnerState.criticalBatteryState & 0b00000001) > 0;
        bool charging = (keyTurnerState.criticalBatteryState & 0b00000010) > 0;
        publishInt(mqtt_topic_battery_level, level); // percent
        publishBool(mqtt_topic_battery_critical, critical);
        publishBool(mqtt_topic_battery_charging, charging);
    }

    _firstTunerStatePublish = false;
}

void Network::publishAuthorizationInfo(const uint32_t authId, const char *authName)
{
    publishUInt(mqtt_topic_lock_auth_id, authId);
    publishString(mqtt_topic_lock_auth_name, authName);
}

void Network::publishCommandResult(const char *resultStr)
{
    publishString(mqtt_topic_lock_action_command_result, resultStr);
}

void Network::publishBatteryReport(const NukiLock::BatteryReport& batteryReport)
{
    publishFloat(mqtt_topic_battery_voltage, (float)batteryReport.batteryVoltage / 1000.0);
    publishInt(mqtt_topic_battery_drain, batteryReport.batteryDrain); // milliwatt seconds
    publishFloat(mqtt_topic_battery_max_turn_current, (float)batteryReport.maxTurnCurrent / 1000.0);
    publishInt(mqtt_topic_battery_lock_distance, batteryReport.lockDistance); // degrees
}

void Network::publishConfig(const NukiLock::Config &config)
{
    publishBool(mqtt_topic_config_button_enabled, config.buttonEnabled == 1);
    publishBool(mqtt_topic_config_led_enabled, config.ledEnabled == 1);
    publishInt(mqtt_topic_config_led_brightness, config.ledBrightness);
}

void Network::publishAdvancedConfig(const NukiLock::AdvancedConfig &config)
{
    publishBool(mqtt_topic_config_auto_unlock, config.autoUnLockDisabled == 0);
    publishBool(mqtt_topic_config_auto_lock, config.autoLockEnabled == 1);
}

void Network::publishPresenceDetection(char *csv)
{
    _presenceCsv = csv;
}

void Network::setLockActionReceivedCallback(bool (*lockActionReceivedCallback)(const char *))
{
    _lockActionReceivedCallback = lockActionReceivedCallback;
}

void Network::setConfigUpdateReceivedCallback(void (*configUpdateReceivedCallback)(const char *, const char *))
{
    _configUpdateReceivedCallback = configUpdateReceivedCallback;
}

void Network::publishFloat(const char* topic, const float value, const uint8_t precision)
{
    char str[30];
    dtostrf(value, 0, precision, str);
    char path[200] = {0};
    buildMqttPath(topic, path);
    _device->mqttClient()->publish(path, str);
}

void Network::publishInt(const char *topic, const int value)
{
    char str[30];
    itoa(value, str, 10);
    char path[200] = {0};
    buildMqttPath(topic, path);
    _device->mqttClient()->publish(path, str);
}

void Network::publishUInt(const char *topic, const unsigned int value)
{
    char str[30];
    utoa(value, str, 10);
    char path[200] = {0};
    buildMqttPath(topic, path);
    _device->mqttClient()->publish(path, str);
}

void Network::publishBool(const char *topic, const bool value)
{
    char str[2] = {0};
    str[0] = value ? '1' : '0';
    char path[200] = {0};
    buildMqttPath(topic, path);
    _device->mqttClient()->publish(path, str);
}

void Network::publishString(const char *topic, const char *value)
{
    char path[200] = {0};
    buildMqttPath(topic, path);
    _device->mqttClient()->publish(path, value);
}


bool Network::isMqttConnected()
{
    return _mqttConnected;
}

void Network::buildMqttPath(const char* path, char* outPath)
{
    int offset = 0;
    for(const char& c : _mqttPath)
    {
        if(c == 0x00)
        {
            break;
        }
        outPath[offset] = c;
        ++offset;
    }
    int i=0;
    while(outPath[i] != 0x00)
    {
        outPath[offset] = path[i];
        ++i;
        ++offset;
    }
    outPath[i+1] = 0x00;
}

void Network::subscribe(const char *path)
{
    char prefixedPath[500];
    buildMqttPath(path, prefixedPath);
    _device->mqttClient()->subscribe(prefixedPath);
}

void Network::restartAndConfigureWifi()
{
    _device->reconfigure();
}

bool Network::comparePrefixedPath(const char *fullPath, const char *subPath)
{
    char prefixedPath[500];
    buildMqttPath(subPath, prefixedPath);
    return strcmp(fullPath, prefixedPath) == 0;
}
