#include "Network.h"
#include "PreferencesKeys.h"
#include "MqttTopics.h"
#include "networkDevices/W5500Device.h"
#include "networkDevices/WifiDevice.h"


Network* Network::_inst = nullptr;

Network::Network(const NetworkDeviceType networkDevice, Preferences *preferences)
: _preferences(preferences)
{
    _inst = this;
    _hostname = _preferences->getString(preference_hostname);
    setupDevice(networkDevice);
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
            _device = new WifiDevice(_hostname, _preferences);
            break;
        default:
            Serial.println(F("Unknown network device type, defaulting to WiFi"));
            _device = new WifiDevice(_hostname, _preferences);
            break;
    }
}

void Network::initialize()
{
    _restartOnDisconnect = _preferences->getBool(preference_restart_on_disconnect);

    if(_hostname == "")
    {
        _hostname = "nukihub";
        _preferences->putString(preference_hostname, _hostname);
    }

    _device->initialize();

    Serial.print(F("Host name: "));
    Serial.println(_hostname);

    String brokerAddr = _preferences->getString(preference_mqtt_broker);
    strcpy(_mqttBrokerAddr, brokerAddr.c_str());

    int port = _preferences->getInt(preference_mqtt_broker_port);
    if(port == 0)
    {
        port = 1883;
        _preferences->putInt(preference_mqtt_broker_port, port);
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

int Network::update()
{
    unsigned long ts = millis();

    _device->update();

    if(!_device->isConnected())
    {
        if(_restartOnDisconnect && millis() > 60000)
        {
            ESP.restart();
        }

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
        return 2;
    }

    _lastConnectedTs = ts;

    if(!_device->mqttClient()->connected())
    {
        bool success = reconnect();
        if(!success)
        {
            return 1;
        }
    }

    if(_presenceCsv != nullptr && strlen(_presenceCsv) > 0)
    {
        bool success = publishString(_mqttPresencePrefix, mqtt_topic_presence, _presenceCsv);
        if(!success)
        {
            Serial.println(F("Failed to publish presence CSV data."));
            Serial.println(_presenceCsv);
        }
        _presenceCsv = nullptr;
    }

    _device->mqttClient()->loop();
    return 0;
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
            for(const String& topic : _subscribedTopics)
            {
                _device->mqttClient()->subscribe(topic.c_str());
            }
            if(_firstConnect)
            {
                _firstConnect = false;
                for(const auto& it : _initTopics)
                {
                    _device->mqttClient()->publish(it.first.c_str(), it.second.c_str(), true);
                }
            }
        }
        else
        {
            Serial.print(F("MQTT connect failed, rc="));
            Serial.println(_device->mqttClient()->state());
            _device->printError();
            _device->mqttClient()->disconnect();
            _mqttConnected = false;
            _nextReconnect = millis() + 5000;
        }
    }
    return _mqttConnected;
}

void Network::subscribe(const char* prefix, const char *path)
{
    char prefixedPath[500];
    buildMqttPath(prefix, path, prefixedPath);
    _subscribedTopics.push_back(prefixedPath);
}

void Network::initTopic(const char *prefix, const char *path, const char *value)
{
    char prefixedPath[500];
    buildMqttPath(prefix, path, prefixedPath);
    String pathStr = prefixedPath;
    String valueStr = value;
    _initTopics[pathStr] = valueStr;
}

void Network::buildMqttPath(const char* prefix, const char* path, char* outPath)
{
    int offset = 0;
    int i=0;
    while(prefix[i] != 0x00)
    {
        outPath[offset] = prefix[i];
        ++offset;
        ++i;
    }

    i=0;
    while(outPath[i] != 0x00)
    {
        outPath[offset] = path[i];
        ++i;
        ++offset;
    }

    outPath[i+1] = 0x00;
}

void Network::registerMqttReceiver(MqttReceiver* receiver)
{
    _mqttReceivers.push_back(receiver);
}

void Network::onMqttDataReceivedCallback(char *topic, byte *payload, unsigned int length)
{
    _inst->onMqttDataReceived(topic, payload, length);
}

void Network::onMqttDataReceived(char *&topic, byte *&payload, unsigned int &length)
{
    for(auto receiver : _mqttReceivers)
    {
        receiver->onMqttDataReceived(topic, payload, length);
    }
}

PubSubClient *Network::mqttClient()
{
    return _device->mqttClient();
}

void Network::reconfigureDevice()
{
    _device->reconfigure();
}

void Network::setMqttPresencePath(char *path)
{
    memset(_mqttPresencePrefix, 0, sizeof(_mqttPresencePrefix));
    strcpy(_mqttPresencePrefix, path);
}

void Network::disableAutoRestarts()
{
    _networkTimeout = 0;
    _restartOnDisconnect = false;
}

bool Network::isMqttConnected()
{
    return _mqttConnected;
}


void Network::publishFloat(const char* prefix, const char* topic, const float value, const uint8_t precision)
{
    char str[30];
    dtostrf(value, 0, precision, str);
    char path[200] = {0};
    buildMqttPath(prefix, topic, path);
    _device->mqttClient()->publish(path, str, true);
}

void Network::publishInt(const char* prefix, const char *topic, const int value)
{
    char str[30];
    itoa(value, str, 10);
    char path[200] = {0};
    buildMqttPath(prefix, topic, path);
    _device->mqttClient()->publish(path, str, true);
}

void Network::publishUInt(const char* prefix, const char *topic, const unsigned int value)
{
    char str[30];
    utoa(value, str, 10);
    char path[200] = {0};
    buildMqttPath(prefix, topic, path);
    _device->mqttClient()->publish(path, str, true);
}

void Network::publishULong(const char* prefix, const char *topic, const unsigned long value)
{
    char str[30];
    utoa(value, str, 10);
    char path[200] = {0};
    buildMqttPath(prefix, topic, path);
    _device->mqttClient()->publish(path, str, true);
}

void Network::publishBool(const char* prefix, const char *topic, const bool value)
{
    char str[2] = {0};
    str[0] = value ? '1' : '0';
    char path[200] = {0};
    buildMqttPath(prefix, topic, path);
    _device->mqttClient()->publish(path, str, true);
}

bool Network::publishString(const char* prefix, const char *topic, const char *value)
{
    char path[200] = {0};
    buildMqttPath(prefix, topic, path);
    return _device->mqttClient()->publish(path, value, true);
}

void Network::publishHASSConfig(char* deviceType, const char* baseTopic, char* name, char* uidString, char* lockAction, char* unlockAction, char* openAction, char* lockedState, char* unlockedState)
{
    String discoveryTopic = _preferences->getString(preference_mqtt_hass_discovery);

    if (discoveryTopic != "")
    {
        String configJSON = "{\"dev\":{\"ids\":[\"nuki_";
        configJSON.concat(uidString);
        configJSON.concat("\"],\"mf\":\"Nuki\",\"mdl\":\"");
        configJSON.concat(deviceType);
        configJSON.concat("\",\"name\":\"");
        configJSON.concat(name);
        configJSON.concat("\"},\"~\":\"");
        configJSON.concat(baseTopic);
        configJSON.concat("\",\"name\":\"");
        configJSON.concat(name);
        configJSON.concat("\",\"unique_id\":\"");
        configJSON.concat(uidString);
        configJSON.concat("_lock\",\"cmd_t\":\"~");
        configJSON.concat(mqtt_topic_lock_action);
        configJSON.concat("\",\"pl_lock\":\"");
        configJSON.concat(lockAction);
        configJSON.concat("\",\"pl_unlk\":\"");
        configJSON.concat(unlockAction);
        configJSON.concat("\",\"pl_open\":\"");
        configJSON.concat(openAction);
        configJSON.concat("\",\"stat_t\":\"~");
        configJSON.concat(mqtt_topic_lock_binary_state);
        configJSON.concat("\",\"stat_locked\":\"");
        configJSON.concat(lockedState);
        configJSON.concat("\",\"stat_unlocked\":\"");
        configJSON.concat(unlockedState);
        configJSON.concat("\",\"opt\":\"false\"}");

        String path = discoveryTopic;
        path.concat("/lock/");
        path.concat(uidString);
        path.concat("/smartlock/config");

        Serial.println("HASS Config:");
        Serial.println(configJSON);

        _device->mqttClient()->publish(path.c_str(), configJSON.c_str(), true);

        // Battery critical
        configJSON = "{\"dev\":{\"ids\":[\"nuki_";
        configJSON.concat(uidString);
        configJSON.concat("\"],\"mf\":\"Nuki\",\"mdl\":\"");
        configJSON.concat(deviceType);
        configJSON.concat("\",\"name\":\"");
        configJSON.concat(name);
        configJSON.concat("\"},\"~\":\"");
        configJSON.concat(baseTopic);
        configJSON.concat("\",\"name\":\"");
        configJSON.concat(name);
        configJSON.concat(" battery low\",\"unique_id\":\"");
        configJSON.concat(uidString);
        configJSON.concat(
                "_battery_low\",\"dev_cla\":\"battery\",\"ent_cat\":\"diagnostic\",\"pl_off\":\"0\",\"pl_on\":\"1\",\"stat_t\":\"~");
        configJSON.concat(mqtt_topic_battery_critical);
        configJSON.concat("\"}");

        path = discoveryTopic;
        path.concat("/binary_sensor/");
        path.concat(uidString);
        path.concat("/battery_low/config");

        _device->mqttClient()->publish(path.c_str(), configJSON.c_str(), true);

        // Battery voltage
        configJSON = "{\"dev\":{\"ids\":[\"nuki_";
        configJSON.concat(uidString);
        configJSON.concat("\"],\"mf\":\"Nuki\",\"mdl\":\"");
        configJSON.concat(deviceType);
        configJSON.concat("\",\"name\":\"");
        configJSON.concat(name);
        configJSON.concat("\"},\"~\":\"");
        configJSON.concat(baseTopic);
        configJSON.concat("\",\"name\":\"");
        configJSON.concat(name);
        configJSON.concat(" battery voltage\",\"unique_id\":\"");
        configJSON.concat(uidString);
        configJSON.concat(
                "_battery_voltage\",\"dev_cla\":\"voltage\",\"ent_cat\":\"diagnostic\",\"stat_t\":\"~");
        configJSON.concat(mqtt_topic_battery_voltage);
        configJSON.concat("\",\"state_cla\":\"measurement\",\"unit_of_meas\":\"V\"");
        configJSON.concat("}");

        path = discoveryTopic;
        path.concat("/sensor/");
        path.concat(uidString);
        path.concat("/battery_voltage/config");

        _device->mqttClient()->publish(path.c_str(), configJSON.c_str(), true);
    }
}

void Network::publishHASSConfigBatLevel(char *deviceType, const char *baseTopic, char *name, char *uidString,
                                        char *lockAction, char *unlockAction, char *openAction, char *lockedState,
                                        char *unlockedState)
{
    String discoveryTopic = _preferences->getString(preference_mqtt_hass_discovery);

    if (discoveryTopic != "")
    {
        // Battery level
        String configJSON = "{\"dev\":{\"ids\":[\"nuki_";
        configJSON.concat(uidString);
        configJSON.concat("\"],\"mf\":\"Nuki\",\"mdl\":\"");
        configJSON.concat(deviceType);
        configJSON.concat("\",\"name\":\"");
        configJSON.concat(name);
        configJSON.concat("\"},\"~\":\"");
        configJSON.concat(baseTopic);
        configJSON.concat("\",\"name\":\"");
        configJSON.concat(name);
        configJSON.concat(" battery level\",\"unique_id\":\"");
        configJSON.concat(uidString);
        configJSON.concat(
                "_battery_level\",\"dev_cla\":\"battery\",\"ent_cat\":\"diagnostic\",\"stat_t\":\"~");
        configJSON.concat(mqtt_topic_battery_level);
        configJSON.concat("\",\"state_cla\":\"measurement\",\"unit_of_meas\":\"%\"");
        configJSON.concat("}");

        String path = discoveryTopic;
        path.concat("/sensor/");
        path.concat(uidString);
        path.concat("/battery_level/config");

        _device->mqttClient()->publish(path.c_str(), configJSON.c_str(), true);
    }
}

void Network::publishHASSConfigDoorSensor(char *deviceType, const char *baseTopic, char *name, char *uidString,
                                        char *lockAction, char *unlockAction, char *openAction, char *lockedState,
                                        char *unlockedState)
{
    String discoveryTopic = _preferences->getString(preference_mqtt_hass_discovery);

    if (discoveryTopic != "")
    {
        String configJSON = "{\"dev\":{\"ids\":[\"nuki_";
        configJSON.concat(uidString);
        configJSON.concat("\"],\"mf\":\"Nuki\",\"mdl\":\"");
        configJSON.concat(deviceType);
        configJSON.concat("\",\"name\":\"");
        configJSON.concat(name);
        configJSON.concat("\"},\"~\":\"");
        configJSON.concat(baseTopic);
        configJSON.concat("\",\"name\":\"");
        configJSON.concat(name);
        configJSON.concat(" door sensor\",\"unique_id\":\"");
        configJSON.concat(uidString);
        configJSON.concat(
                "_door_sensor\",\"dev_cla\":\"door\",\"stat_t\":\"~");
        configJSON.concat(mqtt_topic_lock_door_sensor_state);
        configJSON.concat("\",\"pl_off\":\"doorClosed\",\"pl_on\":\"doorOpened\",\"pl_not_avail\":\"unavailable\"");
        configJSON.concat("}");

        String path = discoveryTopic;
        path.concat("/binary_sensor/");
        path.concat(uidString);
        path.concat("/door_sensor/config");

        _device->mqttClient()->publish(path.c_str(), configJSON.c_str(), true);
    }
}

void Network::publishHASSConfigRingDetect(char *deviceType, const char *baseTopic, char *name, char *uidString,
                                          char *lockAction, char *unlockAction, char *openAction, char *lockedState,
                                          char *unlockedState)
{
    String discoveryTopic = _preferences->getString(preference_mqtt_hass_discovery);

    if (discoveryTopic != "")
    {
        String configJSON = "{\"dev\":{\"ids\":[\"nuki_";
        configJSON.concat(uidString);
        configJSON.concat("\"],\"mf\":\"Nuki\",\"mdl\":\"");
        configJSON.concat(deviceType);
        configJSON.concat("\",\"name\":\"");
        configJSON.concat(name);
        configJSON.concat("\"},\"~\":\"");
        configJSON.concat(baseTopic);
        configJSON.concat("\",\"name\":\"");
        configJSON.concat(name);
        configJSON.concat(" ring\",\"unique_id\":\"");
        configJSON.concat(uidString);
        configJSON.concat(
                "_ring\",\"dev_cla\":\"sound\",\"stat_t\":\"~");
        configJSON.concat(mqtt_topic_lock_state);
        configJSON.concat("\",\"pl_off\":\"locked\",\"pl_on\":\"ring\"}");

        String path = discoveryTopic;
        path.concat("/binary_sensor/");
        path.concat(uidString);
        path.concat("/ring/config");

        _device->mqttClient()->publish(path.c_str(), configJSON.c_str(), true);
    }
}

void Network::removeHASSConfig(char* uidString)
{
    String discoveryTopic = _preferences->getString(preference_mqtt_hass_discovery);

    if(discoveryTopic != "")
    {
        String path = discoveryTopic;
        path.concat("/lock/");
        path.concat(uidString);
        path.concat("/smartlock/config");

        _device->mqttClient()->publish(path.c_str(), NULL, 0U, true);

        path = discoveryTopic;
        path.concat("/binary_sensor/");
        path.concat(uidString);
        path.concat("/battery_low/config");

        _device->mqttClient()->publish(path.c_str(), NULL, 0U, true);
    }
}

void Network::publishPresenceDetection(char *csv)
{
    _presenceCsv = csv;
}
