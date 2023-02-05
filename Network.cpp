#include "Network.h"
#include "PreferencesKeys.h"
#include "MqttTopics.h"
#include "networkDevices/W5500Device.h"
#include "networkDevices/WifiDevice.h"
#include "Logger.h"
#include "Config.h"


Network* Network::_inst = nullptr;

RTC_NOINIT_ATTR char WiFi_fallbackDetect[14];

Network::Network(Preferences *preferences, const String& maintenancePathPrefix)
: _preferences(preferences)
{
    _inst = this;
    _hostname = _preferences->getString(preference_hostname);

    memset(_maintenancePathPrefix, 0, sizeof(_maintenancePathPrefix));

    size_t len = maintenancePathPrefix.length();
    for(int i=0; i < len; i++)
    {
        _maintenancePathPrefix[i] = maintenancePathPrefix.charAt(i);
    }
    setupDevice();
}

void Network::setupDevice()
{
    int hardwareDetect = _preferences->getInt(preference_network_hardware);
    int hardwareDetectGpio = _preferences->getInt(preference_network_hardware_gpio);

    Log->print(F("Hardware detect     : ")); Log->println(hardwareDetect);
    Log->print(F("Hardware detect GPIO: ")); Log->println(hardwareDetectGpio);

    if(hardwareDetect == 0)
    {
        hardwareDetect = 2;
        _preferences->putInt(preference_network_hardware, hardwareDetect);
    }
    if(hardwareDetectGpio == 0)
    {
        hardwareDetectGpio = 26;
        _preferences->putInt(preference_network_hardware_gpio, hardwareDetectGpio);
    }

    if(strcmp(WiFi_fallbackDetect, "wifi_fallback") == 0)
    {
        Log->println(F("Switching to WiFi device as fallback."));
        _networkDeviceType = NetworkDeviceType::WiFi;
    }
    else
    {
        if(hardwareDetect == 1)
        {
            Log->println(F("W5500 hardware is disabled, using Wifi."));
            _networkDeviceType = NetworkDeviceType::WiFi;
        }
        else if(hardwareDetect == 2)
        {
            Log->print(F("Using PIN "));
            Log->print(hardwareDetectGpio);
            Log->println(F(" for network device selection"));

            pinMode(hardwareDetectGpio, INPUT_PULLUP);
            _networkDeviceType = NetworkDeviceType::W5500;
//                    digitalRead(hardwareDetectGpio) == HIGH ? NetworkDeviceType::WiFi : NetworkDeviceType::W5500;
        }
        else if(hardwareDetect == 3)
        {
            Log->print(F("W5500 on M5Stack Atom POE"));
            _networkDeviceType = NetworkDeviceType::W5500;
        }
        else
        {
            Log->println(F("Unknown hardware selected, falling back to Wifi."));
            _networkDeviceType = NetworkDeviceType::WiFi;
        }
    }

    switch(_networkDeviceType)
    {
        case NetworkDeviceType::W5500:
            Log->println(F("Network device: W5500"));
            _device = new W5500Device(_hostname, _preferences, hardwareDetect);
            break;
        case NetworkDeviceType::WiFi:
            Log->println(F("Network device: Builtin WiFi"));
            _device = new WifiDevice(_hostname, _preferences);
            break;
        default:
            Log->println(F("Unknown network device type, defaulting to WiFi"));
            _device = new WifiDevice(_hostname, _preferences);
            break;
    }

    _device->mqttOnConnect([&](bool sessionPresent)
        {
            onMqttConnect(sessionPresent);
        });
    _device->mqttOnDisconnect([&](espMqttClientTypes::DisconnectReason reason)
        {
            onMqttDisconnect(reason);
        });
}

void Network::initialize()
{
    _restartOnDisconnect = _preferences->getBool(preference_restart_on_disconnect);
    _rssiPublishInterval = _preferences->getInt(preference_rssi_publish_interval) * 1000;

    _hostname = _preferences->getString(preference_hostname);

    if(_hostname == "")
    {
        _hostname = "nukihub";
        _preferences->putString(preference_hostname, _hostname);
    }
    if(_rssiPublishInterval == 0)
    {
        _rssiPublishInterval = -1;
        _preferences->putInt(preference_rssi_publish_interval, _rssiPublishInterval);
    }
    strcpy(_hostnameArr, _hostname.c_str());
    _device->initialize();

    Log->print(F("Host name: "));
    Log->println(_hostname);

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

    Log->print(F("MQTT Broker: "));
    Log->print(_mqttBrokerAddr);
    Log->print(F(":"));
    Log->println(port);

    _device->mqttSetClientId(_hostnameArr);
    _device->mqttSetCleanSession(MQTT_CLEAN_SESSIONS);

    _networkTimeout = _preferences->getInt(preference_network_timeout);
    if(_networkTimeout == 0)
    {
        _networkTimeout = -1;
        _preferences->putInt(preference_network_timeout, _networkTimeout);
    }

    _publishFreeHeap = _preferences->getBool(preference_publish_heap);
}

bool Network::update()
{
    unsigned long ts = millis();

    _device->update();

    if(!_device->isConnected())
    {
        if(_restartOnDisconnect && millis() > 60000)
        {
            ESP.restart();
        }

        Log->println(F("Network not connected. Trying reconnect."));
        ReconnectStatus reconnectStatus = _device->reconnect();

        switch(reconnectStatus)
        {
            case ReconnectStatus::CriticalFailure:
                strcpy(WiFi_fallbackDetect, "wifi_fallback");
                Log->println("Network device has a critical failure, enable fallback to Wifi and reboot.");
                delay(200);
                ESP.restart();
                break;
            case ReconnectStatus::Success:
                memset(WiFi_fallbackDetect, 0, sizeof(WiFi_fallbackDetect));
                Log->println(F("Reconnect successful"));
                break;
            case ReconnectStatus::Failure:
                Log->println(F("Reconnect failed"));
                break;
        }

    }

    if(!_device->mqttConnected())
    {
        if(_networkTimeout > 0 && (ts - _lastConnectedTs > _networkTimeout * 1000) && ts > 60000)
        {
            Log->println("Network timeout has been reached, restarting ...");
            delay(200);
            ESP.restart();
        }

        bool success = reconnect();
        if(!success)
        {
            return false;
        }
    }

    _lastConnectedTs = ts;

    if(_presenceCsv != nullptr && strlen(_presenceCsv) > 0)
    {
        bool success = publishString(_mqttPresencePrefix, mqtt_topic_presence, _presenceCsv);
        if(!success)
        {
            Log->println(F("Failed to publish presence CSV data."));
            Log->println(_presenceCsv);
        }
        _presenceCsv = nullptr;
    }

    if(_device->signalStrength() != 127 && _rssiPublishInterval > 0 && ts - _lastRssiTs > _rssiPublishInterval)
    {
        _lastRssiTs = ts;
        int8_t rssi = _device->signalStrength();

        if(rssi != _lastRssi)
        {
            publishInt(_maintenancePathPrefix, mqtt_topic_wifi_rssi, _device->signalStrength());
            _lastRssi = rssi;
        }
    }

    if(_lastMaintenanceTs == 0 || (ts - _lastMaintenanceTs) > 30000)
    {
        publishULong(_maintenancePathPrefix, mqtt_topic_uptime, ts / 1000 / 60);
        if(_publishFreeHeap)
        {
            publishUInt(_maintenancePathPrefix, mqtt_topic_freeheap, esp_get_free_heap_size());
        }
        _lastMaintenanceTs = ts;
    }

    return true;
}


void Network::onMqttConnect(const bool &sessionPresent)
{
    _connectReplyReceived = true;
}

void Network::onMqttDisconnect(const espMqttClientTypes::DisconnectReason &reason)
{
    _connectReplyReceived = true;

    Log->print("MQTT disconnected. Reason: ");
    switch(reason)
    {
        case espMqttClientTypes::DisconnectReason::USER_OK:
            Log->println(F("USER_OK"));
            break;
        case espMqttClientTypes::DisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
            Log->println(F("MQTT_UNACCEPTABLE_PROTOCOL_VERSION"));
            break;
        case espMqttClientTypes::DisconnectReason::MQTT_IDENTIFIER_REJECTED:
            Log->println(F("MQTT_IDENTIFIER_REJECTED"));
            break;
        case espMqttClientTypes::DisconnectReason::MQTT_SERVER_UNAVAILABLE:
            Log->println(F("MQTT_SERVER_UNAVAILABLE"));
            break;
        case espMqttClientTypes::DisconnectReason::MQTT_MALFORMED_CREDENTIALS:
            Log->println(F("MQTT_MALFORMED_CREDENTIALS"));
            break;
        case espMqttClientTypes::DisconnectReason::MQTT_NOT_AUTHORIZED:
            Log->println(F("MQTT_NOT_AUTHORIZED"));
            break;
        case espMqttClientTypes::DisconnectReason::TLS_BAD_FINGERPRINT:
            Log->println(F("TLS_BAD_FINGERPRINT"));
            break;
        case espMqttClientTypes::DisconnectReason::TCP_DISCONNECTED:
            Log->println(F("TCP_DISCONNECTED"));
            break;
        default:
            Log->println(F("Unknown"));
            break;
    }
}

bool Network::reconnect()
{
    _mqttConnectionState = 0;
    int port = _preferences->getInt(preference_mqtt_broker_port);

    while (!_device->mqttConnected() && millis() > _nextReconnect)
    {
        if(strcmp(_mqttBrokerAddr, "") == 0)
        {
            Log->println(F("MQTT Broker not configured, aborting connection attempt."));
            _nextReconnect = millis() + 5000;
            return false;
        }

        Log->println(F("Attempting MQTT connection"));

        _connectReplyReceived = false;
        if(strlen(_mqttUser) == 0)
        {
            Log->println(F("MQTT: Connecting without credentials"));
            _device->mqttSetServer(_mqttBrokerAddr, port);
            _device->mqttConnect();
        }
        else
        {
            Log->print(F("MQTT: Connecting with user: ")); Log->println(_mqttUser);
            _device->mqttSetCredentials(_mqttUser, _mqttPass);
            _device->mqttSetServer(_mqttBrokerAddr, port);
            _device->mqttConnect();
        }

        unsigned long timeout = millis() + 60000;

        while(!_connectReplyReceived && millis() < timeout)
        {
            delay(200);
            if(_keepAliveCallback != nullptr)
            {
                _keepAliveCallback();
            }
        }

        if (_device->mqttConnected())
        {
            Log->println(F("MQTT connected"));
            _mqttConnectionState = 1;
            delay(100);

            _device->mqttOnMessage(Network::onMqttDataReceivedCallback);
            for(const String& topic : _subscribedTopics)
            {
                _device->mqttSubscribe(topic.c_str(), MQTT_QOS_LEVEL);
            }
            if(_firstConnect)
            {
                _firstConnect = false;
                for(const auto& it : _initTopics)
                {
                    _device->mqttPublish(it.first.c_str(), MQTT_QOS_LEVEL, true, it.second.c_str());
                }
            }
            delay(1000);
            _mqttConnectionState = 2;
        }
        else
        {
            Log->print(F("MQTT connect failed, rc="));
            _device->printError();
            _mqttConnectionState = 0;
            _nextReconnect = millis() + 5000;
            _device->mqttDisonnect(true);
        }
    }
    return _mqttConnectionState > 0;
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
    while(path[i] != 0x00)
    {
        outPath[offset] = path[i];
        ++i;
        ++offset;
    }

    outPath[offset] = 0x00;
}

void Network::registerMqttReceiver(MqttReceiver* receiver)
{
    _mqttReceivers.push_back(receiver);
}

void Network::onMqttDataReceivedCallback(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total)
{
    uint8_t value[50] = {0};
    size_t l = min(len, sizeof(value)-1);

    for(int i=0; i<l; i++)
    {
        value[i] = payload[i];
    }

    _inst->onMqttDataReceived(properties, topic, value, len, index, total);
}

void Network::onMqttDataReceived(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total)
{
    for(auto receiver : _mqttReceivers)
    {
        receiver->onMqttDataReceived(topic, (byte*)payload, index);
    }
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

int Network::mqttConnectionState()
{
    return _mqttConnectionState;
}

bool Network::encryptionSupported()
{
    return _device->supportsEncryption();
}

void Network::publishFloat(const char* prefix, const char* topic, const float value, const uint8_t precision)
{
    char str[30];
    dtostrf(value, 0, precision, str);
    char path[200] = {0};
    buildMqttPath(prefix, topic, path);
    _device->mqttPublish(path, MQTT_QOS_LEVEL, true, str);
}

void Network::publishInt(const char* prefix, const char *topic, const int value)
{
    char str[30];
    itoa(value, str, 10);
    char path[200] = {0};
    buildMqttPath(prefix, topic, path);
    _device->mqttPublish(path, MQTT_QOS_LEVEL, true, str);
}

void Network::publishUInt(const char* prefix, const char *topic, const unsigned int value)
{
    char str[30];
    utoa(value, str, 10);
    char path[200] = {0};
    buildMqttPath(prefix, topic, path);
    _device->mqttPublish(path, MQTT_QOS_LEVEL, true, str);
}

void Network::publishULong(const char* prefix, const char *topic, const unsigned long value)
{
    char str[30];
    utoa(value, str, 10);
    char path[200] = {0};
    buildMqttPath(prefix, topic, path);
    _device->mqttPublish(path, MQTT_QOS_LEVEL, true, str);
}

void Network::publishBool(const char* prefix, const char *topic, const bool value)
{
    char str[2] = {0};
    str[0] = value ? '1' : '0';
    char path[200] = {0};
    buildMqttPath(prefix, topic, path);
    _device->mqttPublish(path, MQTT_QOS_LEVEL, true, str);
}

bool Network::publishString(const char* prefix, const char *topic, const char *value)
{
    char path[200] = {0};
    buildMqttPath(prefix, topic, path);
    return _device->mqttPublish(path, MQTT_QOS_LEVEL, true, value) > 0;
}

void Network::publishHASSConfig(char* deviceType, const char* baseTopic, char* name, char* uidString, const bool& hasKeypad, char* lockAction, char* unlockAction, char* openAction, char* lockedState, char* unlockedState)
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

        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, configJSON.c_str());

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
        configJSON.concat("_battery_low\",\"dev_cla\":\"battery\",\"ent_cat\":\"diagnostic\",\"pl_off\":\"0\",\"pl_on\":\"1\",\"stat_t\":\"~");
        configJSON.concat(mqtt_topic_battery_critical);
        configJSON.concat("\"}");

        path = discoveryTopic;
        path.concat("/binary_sensor/");
        path.concat(uidString);
        path.concat("/battery_low/config");

        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, configJSON.c_str());

        if(hasKeypad)
        {
            // Keypad battery critical
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
            configJSON.concat(" keypad battery low\",\"unique_id\":\"");
            configJSON.concat(uidString);
            configJSON.concat(
                    "_keypad_battery_low\",\"dev_cla\":\"battery\",\"ent_cat\":\"diagnostic\",\"pl_off\":\"0\",\"pl_on\":\"1\",\"stat_t\":\"~");
            configJSON.concat(mqtt_topic_battery_keypad_critical);
            configJSON.concat("\"}");

            path = discoveryTopic;
            path.concat("/binary_sensor/");
            path.concat(uidString);
            path.concat("/keypad_battery_low/config");

            _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, configJSON.c_str());
        }

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
        configJSON.concat("\",\"stat_cla\":\"measurement\",\"unit_of_meas\":\"V\"");
        configJSON.concat("}");

        path = discoveryTopic;
        path.concat("/sensor/");
        path.concat(uidString);
        path.concat("/battery_voltage/config");

        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, configJSON.c_str());

        // Trigger
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
        configJSON.concat(" trigger\",\"unique_id\":\"");
        configJSON.concat(uidString);
        configJSON.concat(
                "_trigger\",\"ent_cat\":\"diagnostic\",\"stat_t\":\"~");
        configJSON.concat(mqtt_topic_lock_trigger);
        configJSON.concat("\",\"enabled_by_default\":true}");

        path = discoveryTopic;
        path.concat("/sensor/");
        path.concat(uidString);
        path.concat("/trigger/config");

        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, configJSON.c_str());
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
        configJSON.concat("\",\"stat_cla\":\"measurement\",\"unit_of_meas\":\"%\"");
        configJSON.concat("}");

        String path = discoveryTopic;
        path.concat("/sensor/");
        path.concat(uidString);
        path.concat("/battery_level/config");

        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, configJSON.c_str());
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

        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, configJSON.c_str());
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

        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, configJSON.c_str());
    }
}

void Network::publishHASSWifiRssiConfig(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    if(_device->signalStrength() == 127)
    {
        return;
    }

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
        configJSON.concat(" WiFi signal strength\",\"unique_id\":\"");
        configJSON.concat(uidString);
        configJSON.concat(
                "_wifi_signal_strength\",\"dev_cla\":\"signal_strength\",\"ent_cat\":\"diagnostic\",\"stat_t\":\"~");
        configJSON.concat(mqtt_topic_wifi_rssi);
        configJSON.concat("\",\"stat_cla\":\"measurement\",\"unit_of_meas\":\"dBm\"");
        configJSON.concat("}");

        String path = discoveryTopic;
        path.concat("/sensor/");
        path.concat(uidString);
        path.concat("/wifi_signal_strength/config");

        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, configJSON.c_str());
    }
}

void Network::publishHASSBleRssiConfig(char *deviceType, const char *baseTopic, char *name, char *uidString)
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
        configJSON.concat(" bluetooth signal strength\",\"unique_id\":\"");
        configJSON.concat(uidString);
        configJSON.concat("_bluetooth_signal_strength\",\"dev_cla\":\"signal_strength\",\"ent_cat\":\"diagnostic\",\"stat_t\":\"~");
        configJSON.concat(mqtt_topic_lock_rssi);
        configJSON.concat("\",\"stat_cla\":\"measurement\",\"unit_of_meas\":\"dBm\"");
        configJSON.concat("}");

        String path = discoveryTopic;
        path.concat("/sensor/");
        path.concat(uidString);
        path.concat("/bluetooth_signal_strength/config");

        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, configJSON.c_str());
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
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, "");

        path = discoveryTopic;
        path.concat("/binary_sensor/");
        path.concat(uidString);
        path.concat("/battery_low/config");
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, "");

        path = discoveryTopic;
        path.concat("/sensor/");
        path.concat(uidString);
        path.concat("/battery_voltage/config");
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, "");

        path = discoveryTopic;
        path.concat("/sensor/");
        path.concat(uidString);
        path.concat("/trigger/config");
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, "");

        path = discoveryTopic;
        path.concat("/sensor/");
        path.concat(uidString);
        path.concat("/battery_level/config");
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, "");

        path = discoveryTopic;
        path.concat("/binary_sensor/");
        path.concat(uidString);
        path.concat("/door_sensor/config");
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, "");

        path = discoveryTopic;
        path.concat("/binary_sensor/");
        path.concat(uidString);
        path.concat("/ring/config");
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, "");

        path = discoveryTopic;
        path.concat("/sensor/");
        path.concat(uidString);
        path.concat("/wifi_signal_strength/config");
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, "");

        path = discoveryTopic;
        path.concat("/sensor/");
        path.concat(uidString);
        path.concat("/bluetooth_signal_strength/config");
        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, "");
    }
}

void Network::publishPresenceDetection(char *csv)
{
    _presenceCsv = csv;
}

const NetworkDeviceType Network::networkDeviceType()
{
    return _networkDeviceType;
}

uint16_t Network::subscribe(const char *topic, uint8_t qos)
{
    return _device->mqttSubscribe(topic, qos);
}

void Network::setKeepAliveCallback(std::function<void()> reconnectTick)
{
    _keepAliveCallback = reconnectTick;
}
