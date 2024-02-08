#include "Network.h"
#include "PreferencesKeys.h"
#include "networkDevices/W5500Device.h"
#include "networkDevices/WifiDevice.h"
#include "Logger.h"
#include "Config.h"
#include <ArduinoJson.h>
#include "RestartReason.h"
#include "networkDevices/EthLan8720Device.h"

Network* Network::_inst = nullptr;
unsigned long Network::_ignoreSubscriptionsTs = 0;
bool _versionPublished = false;

RTC_NOINIT_ATTR char WiFi_fallbackDetect[14];

Network::Network(Preferences *preferences, Gpio* gpio, const String& maintenancePathPrefix, char* buffer, size_t bufferSize)
: _preferences(preferences),
  _gpio(gpio),
  _buffer(buffer),
  _bufferSize(bufferSize)
{
    // Remove obsolete W5500 hardware detection configuration
    if(_preferences->getInt(preference_network_hardware_gpio) != 0)
    {
        _preferences->remove(preference_network_hardware_gpio);
    }

    _inst = this;
    _hostname = _preferences->getString(preference_hostname);

    memset(_maintenancePathPrefix, 0, sizeof(_maintenancePathPrefix));
    size_t len = maintenancePathPrefix.length();
    for(int i=0; i < len; i++)
    {
        _maintenancePathPrefix[i] = maintenancePathPrefix.charAt(i);
    }

    _lockPath = _preferences->getString(preference_mqtt_lock_path);
    String connectionStateTopic = _lockPath + mqtt_topic_mqtt_connection_state;

    memset(_mqttConnectionStateTopic, 0, sizeof(_mqttConnectionStateTopic));
    len = connectionStateTopic.length();
    for(int i=0; i < len; i++)
    {
        _mqttConnectionStateTopic[i] = connectionStateTopic.charAt(i);
    }

    setupDevice();
}

void Network::setupDevice()
{
    _ipConfiguration = new IPConfiguration(_preferences);

    int hardwareDetect = _preferences->getInt(preference_network_hardware);

    Log->print(F("Hardware detect     : ")); Log->println(hardwareDetect);

    if(hardwareDetect == 0)
    {
        hardwareDetect = 1;
        _preferences->putInt(preference_network_hardware, hardwareDetect);
    }

    if(strcmp(WiFi_fallbackDetect, "wifi_fallback") == 0)
    {
        if(_preferences->getBool(preference_network_wifi_fallback_disabled))
        {
            Log->println(F("Failed to connect to network. Wifi fallback is disable, rebooting."));
            memset(WiFi_fallbackDetect, 0, sizeof(WiFi_fallbackDetect));
            sleep(5);
            restartEsp(RestartReason::NetworkDeviceCriticalFailureNoWifiFallback);
        }

        Log->println(F("Switching to WiFi device as fallback."));
        _networkDeviceType = NetworkDeviceType::WiFi;
    }
    else
    {
        Log->print(F("Network device: "));
        switch (hardwareDetect)
        {
            case 1:
                Log->println(F("Wifi only"));
                _networkDeviceType = NetworkDeviceType::WiFi;
                break;
            case 2:
                Log->print(F("Generic W5500"));
                _networkDeviceType = NetworkDeviceType::W5500;
                break;
            case 3:
                Log->println(F("W5500 on M5Stack Atom POE"));
                _networkDeviceType = NetworkDeviceType::W5500;
                break;
            case 4:
                Log->println(F("Olimex ESP32-POE / ESP-POE-ISO"));
                _networkDeviceType = NetworkDeviceType::Olimex_LAN8720;
                break;
            case 5:
                Log->println(F("WT32-ETH01"));
                _networkDeviceType = NetworkDeviceType::WT32_LAN8720;
                break;
            case 6:
                Log->println(F("M5STACK PoESP32 Unit"));
                _networkDeviceType = NetworkDeviceType::M5STACK_PoESP32_Unit;
                break;
            case 7:
                Log->println(F("LilyGO T-ETH-POE"));
                _networkDeviceType = NetworkDeviceType::LilyGO_T_ETH_POE;
                break;
            default:
                Log->println(F("Unknown hardware selected, falling back to Wifi."));
                _networkDeviceType = NetworkDeviceType::WiFi;
                break;
        }
    }

    switch (_networkDeviceType)
    {
        case NetworkDeviceType::W5500:
            _device = new W5500Device(_hostname, _preferences, _ipConfiguration, hardwareDetect);
            break;
        case NetworkDeviceType::Olimex_LAN8720:
            _device = new EthLan8720Device(_hostname, _preferences, _ipConfiguration, "Olimex (LAN8720)", ETH_PHY_ADDR, 12, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_TYPE, ETH_CLOCK_GPIO17_OUT);
            break;
        case NetworkDeviceType::WT32_LAN8720:
            _device = new EthLan8720Device(_hostname, _preferences, _ipConfiguration, "WT32-ETH01", 1, 16);
            break;
        case NetworkDeviceType::M5STACK_PoESP32_Unit:
            _device = new EthLan8720Device(_hostname, _preferences, _ipConfiguration, "M5STACK PoESP32 Unit", 1, 5, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_IP101);
            break;
        case NetworkDeviceType::LilyGO_T_ETH_POE:
            _device = new EthLan8720Device(_hostname, _preferences, _ipConfiguration, "LilyGO T-ETH-POE", 0, -1, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_TYPE, ETH_CLOCK_GPIO17_OUT);
            break;
        case NetworkDeviceType::WiFi:
            _device = new WifiDevice(_hostname, _preferences, _ipConfiguration);
            break;
        default:
            _device = new WifiDevice(_hostname, _preferences, _ipConfiguration);
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
        _rssiPublishInterval = 60;
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

    _publishDebugInfo = _preferences->getBool(preference_publish_debug_info);

    char gpioPath[250];
    bool rebGpio = rebuildGpio();

    if(rebGpio)
    {
        Log->println(F("Rebuild MQTT GPIO structure"));
    }
    for (const auto &pinEntry: _gpio->pinConfiguration())
    {
        switch (pinEntry.role)
        {
            case PinRole::GeneralInputPullDown:
            case PinRole::GeneralInputPullUp:
                if(rebGpio)
                {
                    buildMqttPath(gpioPath, {mqtt_topic_gpio_prefix, (mqtt_topic_gpio_pin + std::to_string(pinEntry.pin)).c_str(), mqtt_topic_gpio_role});
                    publishString(_lockPath.c_str(), gpioPath, "input");
                    buildMqttPath(gpioPath, {mqtt_topic_gpio_prefix, (mqtt_topic_gpio_pin + std::to_string(pinEntry.pin)).c_str(), mqtt_topic_gpio_state});
                    publishString(_lockPath.c_str(), gpioPath, std::to_string(digitalRead(pinEntry.pin)).c_str());
                }
                break;
            case PinRole::GeneralOutput:
                if(rebGpio)
                {
                    buildMqttPath(gpioPath, {mqtt_topic_gpio_prefix, (mqtt_topic_gpio_pin + std::to_string(pinEntry.pin)).c_str(), mqtt_topic_gpio_role});
                    publishString(_lockPath.c_str(), gpioPath, "output");
                    buildMqttPath(gpioPath, {mqtt_topic_gpio_prefix, (mqtt_topic_gpio_pin + std::to_string(pinEntry.pin)).c_str(), mqtt_topic_gpio_state});
                    publishString(_lockPath.c_str(), gpioPath, "0");
                }
                buildMqttPath(gpioPath, {mqtt_topic_gpio_prefix, (mqtt_topic_gpio_pin + std::to_string(pinEntry.pin)).c_str(), mqtt_topic_gpio_state});
                subscribe(_lockPath.c_str(), gpioPath);
                break;
        }
    }
    _gpio->addCallback([this](const GpioAction& action, const int& pin)
    {
        gpioActionCallback(action, pin);
    });
}

bool Network::update()
{
    unsigned long ts = millis();

    _device->update();

    if(!_mqttEnabled)
    {
        return true;
    }

    if(!_device->isConnected())
    {
        if(_restartOnDisconnect && millis() > 60000)
        {
            restartEsp(RestartReason::RestartOnDisconnectWatchdog);
        }

        Log->println(F("Network not connected. Trying reconnect."));
        ReconnectStatus reconnectStatus = _device->reconnect();

        switch(reconnectStatus)
        {
            case ReconnectStatus::CriticalFailure:
                strcpy(WiFi_fallbackDetect, "wifi_fallback");
                Log->println("Network device has a critical failure, enable fallback to Wifi and reboot.");
                delay(200);
                restartEsp(RestartReason::NetworkDeviceCriticalFailure);
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
            restartEsp(RestartReason::NetworkTimeoutWatchdog);
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
        if(_publishDebugInfo)
        {
            publishUInt(_maintenancePathPrefix, mqtt_topic_freeheap, esp_get_free_heap_size());
            publishString(_maintenancePathPrefix, mqtt_topic_restart_reason_fw, getRestartReason().c_str());
            publishString(_maintenancePathPrefix, mqtt_topic_restart_reason_esp, getEspRestartReason().c_str());
        }
        if (!_versionPublished) {
            publishString(_maintenancePathPrefix, mqtt_topic_info_nuki_hub_version, NUKI_HUB_VERSION);
            _versionPublished = true;
        }
        _lastMaintenanceTs = ts;
    }
    
    if(_preferences->getBool(preference_check_updates))
    {
        if(_lastUpdateCheckTs == 0 || (ts - _lastUpdateCheckTs) > 86400000)
        {
            _lastUpdateCheckTs = ts;
            
            https.useHTTP10(true);
            https.begin(GITHUB_LATEST_RELEASE_API_URL);

            int httpResponseCode = https.GET();

            if (httpResponseCode == HTTP_CODE_OK || httpResponseCode == HTTP_CODE_MOVED_PERMANENTLY) {
                DynamicJsonDocument doc(6144);
                DeserializationError jsonError = deserializeJson(doc, https.getStream());

                if (!jsonError) {    
                    _latestVersion = doc["tag_name"];
                    _latestVersionUrl = doc["assets"][0]["browser_download_url"];
                    publishString(_maintenancePathPrefix, mqtt_topic_info_nuki_hub_latest, _latestVersion);
                }            
            }

            https.end();
        }
    }
    
    for(const auto& gpioTs : _gpioTs)
    {
        uint8_t pin = gpioTs.first;
        unsigned long ts = gpioTs.second;
        if(ts != 0 && ((millis() - ts) >= GPIO_DEBOUNCE_TIME))
        {
            _gpioTs[pin] = 0;

            uint8_t pinState = digitalRead(pin) == HIGH ? 1 : 0;
            char gpioPath[250];
            buildMqttPath(gpioPath, {mqtt_topic_gpio_prefix, (mqtt_topic_gpio_pin + std::to_string(pin)).c_str(), mqtt_topic_gpio_state});
            publishInt(_lockPath.c_str(), gpioPath, pinState);

            Log->print(F("GPIO "));
            Log->print(pin);
            Log->print(F(" (Input) --> "));
            Log->println(pinState);
        }
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
        }
        else
        {
            Log->print(F("MQTT: Connecting with user: ")); Log->println(_mqttUser);
            _device->mqttSetCredentials(_mqttUser, _mqttPass);
        }

        _device->setWill(_mqttConnectionStateTopic, 1, true, _lastWillPayload);
        _device->mqttSetServer(_mqttBrokerAddr, port);
        _device->mqttConnect();

        unsigned long timeout = millis() + 60000;

        while(!_connectReplyReceived && millis() < timeout)
        {
            delay(50);
            _device->update();
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

            _ignoreSubscriptionsTs = millis() + 2000;
            _device->mqttOnMessage(Network::onMqttDataReceivedCallback);
            for(const String& topic : _subscribedTopics)
            {
                _device->mqttSubscribe(topic.c_str(), MQTT_QOS_LEVEL);
            }
            if(_firstConnect)
            {
                _firstConnect = false;
                publishString(_maintenancePathPrefix, mqtt_topic_network_device, _device->deviceName().c_str());
                for(const auto& it : _initTopics)
                {
                    _device->mqttPublish(it.first.c_str(), MQTT_QOS_LEVEL, true, it.second.c_str());
                }
            }

            publishString(_maintenancePathPrefix, mqtt_topic_mqtt_connection_state, "online");
            publishString(_maintenancePathPrefix, mqtt_topic_info_nuki_hub_ip, _device->localIP().c_str());

            _mqttConnectionState = 2;
            for(const auto& callback : _reconnectedCallbacks)
            {
                callback();
            }
        }
        else
        {
            Log->print(F("MQTT connect failed, rc="));
            _device->printError();
            _mqttConnectionState = 0;
            _nextReconnect = millis() + 5000;
            _device->mqttDisconnect(true);
        }
    }
    return _mqttConnectionState > 0;
}

void Network::subscribe(const char* prefix, const char *path)
{
    char prefixedPath[500];
    buildMqttPath(prefixedPath, { prefix, path });
    _subscribedTopics.push_back(prefixedPath);
}

void Network::initTopic(const char *prefix, const char *path, const char *value)
{
    char prefixedPath[500];
    buildMqttPath(prefixedPath, { prefix, path });
    String pathStr = prefixedPath;
    String valueStr = value;
    _initTopics[pathStr] = valueStr;
}

void Network::buildMqttPath(char* outPath, std::initializer_list<const char*> paths)
{
    int offset = 0;
    int pathCount = 0;

    for(const char* path : paths)
    {
        if(pathCount > 0 && path[0] != '/')
        {
            outPath[offset] = '/';
            ++offset;
        }

        int i = 0;
        while(path[i] != 0)
        {
            outPath[offset] = path[i];
            ++offset;
            ++i;
        }
        ++pathCount;
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

void Network::onMqttDataReceived(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t& len, size_t& index, size_t& total)
{
    parseGpioTopics(properties, topic, payload, len, index, total);

    if(millis() < _ignoreSubscriptionsTs)
    {
        return;
    }

    for(auto receiver : _mqttReceivers)
    {
        receiver->onMqttDataReceived(topic, (byte*)payload, index);
    }
}


void Network::parseGpioTopics(const espMqttClientTypes::MessageProperties &properties, const char *topic, const uint8_t *payload, size_t& len, size_t& index, size_t& total)
{
    char gpioPath[250];
    buildMqttPath(gpioPath, {_lockPath.c_str(), mqtt_topic_gpio_prefix, mqtt_topic_gpio_pin});
//    /nuki_t/gpio/pin_17/state
    size_t gpioLen = strlen(gpioPath);
    if(strncmp(gpioPath, topic, gpioLen) == 0)
    {
        char pinStr[3] = {0};
        pinStr[0] = topic[gpioLen];
        if(topic[gpioLen+1] != '/')
        {
            pinStr[1] = topic[gpioLen+1];
        }

        int pin = std::atoi(pinStr);

        if(_gpio->getPinRole(pin) == PinRole::GeneralOutput)
        {
            const uint8_t pinState = strcmp((const char*)payload, "1") == 0 ? HIGH : LOW;
            Log->print(F("GPIO "));
            Log->print(pin);
            Log->print(F(" (Output) --> "));
            Log->println(pinState);
            digitalWrite(pin, pinState);
        }

    }
}

void Network::gpioActionCallback(const GpioAction &action, const int &pin)
{
    _gpioTs[pin] = millis();
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

const char* Network::latestHubVersion()
{
    return _latestVersion;
}

const char* Network::latestHubVersionUrl()
{
    return _latestVersionUrl;
}

bool Network::encryptionSupported()
{
    return _device->supportsEncryption();
}

const String Network::networkDeviceName() const
{
    return _device->deviceName();
}

void Network::publishFloat(const char* prefix, const char* topic, const float value, const uint8_t precision)
{
    char str[30];
    dtostrf(value, 0, precision, str);
    char path[200] = {0};
    buildMqttPath(path, { prefix, topic });
    _device->mqttPublish(path, MQTT_QOS_LEVEL, true, str);
}

void Network::publishInt(const char* prefix, const char *topic, const int value)
{
    char str[30];
    itoa(value, str, 10);
    char path[200] = {0};
    buildMqttPath(path, { prefix, topic });
    _device->mqttPublish(path, MQTT_QOS_LEVEL, true, str);
}

void Network::publishUInt(const char* prefix, const char *topic, const unsigned int value)
{
    char str[30];
    utoa(value, str, 10);
    char path[200] = {0};
    buildMqttPath(path, { prefix, topic });
    _device->mqttPublish(path, MQTT_QOS_LEVEL, true, str);
}

void Network::publishULong(const char* prefix, const char *topic, const unsigned long value)
{
    char str[30];
    utoa(value, str, 10);
    char path[200] = {0};
    buildMqttPath(path, { prefix, topic });
    _device->mqttPublish(path, MQTT_QOS_LEVEL, true, str);
}

void Network::publishBool(const char* prefix, const char *topic, const bool value)
{
    char str[2] = {0};
    str[0] = value ? '1' : '0';
    char path[200] = {0};
    buildMqttPath(path, { prefix, topic });
    _device->mqttPublish(path, MQTT_QOS_LEVEL, true, str);
}

bool Network::publishString(const char* prefix, const char *topic, const char *value)
{
    char path[200] = {0};
    buildMqttPath(path, { prefix, topic });
    return _device->mqttPublish(path, MQTT_QOS_LEVEL, true, value) > 0;
}

void Network::publishHASSConfig(char* deviceType, const char* baseTopic, char* name, char* uidString, const char* availabilityTopic, const bool& hasKeypad, char* lockAction, char* unlockAction, char* openAction, char* lockedState, char* unlockedState)
{
    String discoveryTopic = _preferences->getString(preference_mqtt_hass_discovery);

    if (discoveryTopic != "")
    {
        DynamicJsonDocument json(JSON_BUFFER_SIZE);

        auto dev = json.createNestedObject("dev");
        auto ids = dev.createNestedArray("ids");
        ids.add(String("nuki_") + uidString);
        json["dev"]["mf"] = "Nuki";
        json["dev"]["mdl"] = deviceType;
        json["dev"]["name"] = name;
        
        String cuUrl = _preferences->getString(preference_mqtt_hass_cu_url);

        if (cuUrl != "")
        {
            json["dev"]["cu"] = cuUrl;
        }
        else
        {
            json["dev"]["cu"] = "http://" + _device->localIP();
        }
        
        json["~"] = baseTopic;
        json["name"] = nullptr;
        json["unique_id"] = String(uidString) + "_lock";
        json["cmd_t"] = String("~") + String(mqtt_topic_lock_action);
        json["avty"]["t"] = availabilityTopic;
        json["pl_lock"] = lockAction;
        json["pl_unlk"] = unlockAction;
        json["pl_open"] = openAction;
        json["stat_t"] = String("~") + mqtt_topic_lock_binary_state;
        json["stat_locked"] = lockedState;
        json["stat_unlocked"] = unlockedState;
        json["opt"] = "false";

        serializeJson(json, _buffer, _bufferSize);

        String path = discoveryTopic;
        path.concat("/lock/");
        path.concat(uidString);
        path.concat("/smartlock/config");

        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);

        // Battery critical
        publishHassTopic("binary_sensor",
                         "battery_low",
                         uidString,
                         "_battery_low",
                         "battery low",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_battery_critical,
                         deviceType,
                         "battery",
                         "",
                         "diagnostic",
                         "",
                         {{"pl_on", "1"},
                          {"pl_off", "0"}});

        if(hasKeypad)
        {
            // Keypad battery critical
            publishHassTopic("binary_sensor",
                             "keypad_battery_low",
                             uidString,
                             "_keypad_battery_low",
                             "keypad battery low",
                             name,
                             baseTopic,
                             String("~") + mqtt_topic_battery_keypad_critical,
                             deviceType,
                             "battery",
                             "",
                             "diagnostic",
                             "",
                             {{"pl_on", "1"},
                              {"pl_off", "0"}});
        }
        else
        {
            removeHassTopic("binary_sensor", "keypad_battery_low", uidString);
        }

        // Battery voltage
        publishHassTopic("sensor",
                         "battery_voltage",
                         uidString,
                         "_battery_voltage",
                         "battery voltage",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_battery_voltage,
                         deviceType,
                         "voltage",
                         "measurement",
                         "diagnostic",
                         "",
                         { {"unit_of_meas", "V"} });

        // Trigger
        publishHassTopic("sensor",
                         "trigger",
                         uidString,
                         "_trigger",
                         "trigger",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_lock_trigger,
                         deviceType,
                         "",
                         "",
                         "diagnostic",
                         "",
                         { { "enabled_by_default", "true" } });

        // MQTT Connected
        publishHassTopic("binary_sensor",
                         "mqtt_connected",
                         uidString,
                         "_mqtt_connected",
                         "MQTT connected",
                         name,
                         baseTopic,
                         _lockPath + mqtt_topic_mqtt_connection_state,
                         deviceType,
                         "",
                         "",
                         "diagnostic",
                         "",
                         {{"pl_on", "online"},
                          {"pl_off", "offline"},
                          {"ic", "mdi:lan-connect"}});

        // Reset
        publishHassTopic("switch",
                         "reset",
                         uidString,
                         "_reset",
                         "Restart NUKI Hub",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_reset,
                         deviceType,
                         "",
                         "",
                         "diagnostic",
                         String("~") + mqtt_topic_reset,
                         { { "ic", "mdi:restart" },
                           { "pl_on", "1" },
                           { "pl_off", "0" },
                           { "state_on", "1" },
                           { "state_off", "0" }});

        // Firmware version
        publishHassTopic("sensor",
                         "firmware_version",
                         uidString,
                         "_firmware_version",
                         "Firmware version",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_info_firmware_version,
                         deviceType,
                         "",
                         "",
                         "diagnostic",
                         "",
                         { { "enabled_by_default", "true" },
                           {"ic", "mdi:counter"}});

        // Hardware version
        publishHassTopic("sensor",
                         "hardware_version",
                         uidString,
                         "_hardware_version",
                         "Hardware version",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_info_hardware_version,
                         deviceType,
                         "",
                         "",
                         "diagnostic",
                         "",
                         { { "enabled_by_default", "true" },
                           {"ic", "mdi:counter"}});

        // NUKI Hub version
        publishHassTopic("sensor",
                         "nuki_hub_version",
                         uidString,
                         "_nuki_hub_version",
                         "NUKI Hub version",
                         name,
                         baseTopic,
                         _lockPath + mqtt_topic_info_nuki_hub_version,
                         deviceType,
                         "",
                         "",
                         "diagnostic",
                         "",
                         { { "enabled_by_default", "true" },
                           {"ic", "mdi:counter"}});
                      
        if(_preferences->getBool(preference_check_updates))
        {
            // NUKI Hub latest
            publishHassTopic("sensor", 
                             "nuki_hub_latest",
                             uidString,
                             "_nuki_hub_latest",
                             "NUKI Hub latest",
                             name,
                             baseTopic,
                             _lockPath + mqtt_topic_info_nuki_hub_latest,
                             deviceType,
                             "",
                             "",
                             "diagnostic",
                             "",
                             { { "enabled_by_default", "true" },
                               {"ic", "mdi:counter"}});
                               
            // NUKI Hub update
            char latest_version_topic[250];
            _lockPath.toCharArray(latest_version_topic,_lockPath.length() + 1);
            strcat(latest_version_topic, mqtt_topic_info_nuki_hub_latest);

            publishHassTopic("update", 
                             "nuki_hub_update",
                             uidString,
                             "_nuki_hub_update",
                             "NUKI Hub Firmware Update",
                             name,
                             baseTopic,
                             _lockPath + mqtt_topic_info_nuki_hub_version,
                             deviceType,
                             "firmware",
                             "",
                             "",
                             "",
                             { { "enabled_by_default", "true" },
                               { "entity_picture", "https://raw.githubusercontent.com/technyon/nuki_hub/master/icon/favicon-32x32.png" },
                               { "release_url", GITHUB_LATEST_RELEASE_URL },
                               { "latest_version_topic", latest_version_topic }});                               
        }
        else
        {
            removeHassTopic("sensor", "nuki_hub_latest", uidString);
            removeHassTopic("update", "nuki_hub_update", uidString);
        }

        // NUKI Hub IP Address
        publishHassTopic("sensor",
                         "nuki_hub_ip",
                         uidString,
                         "_nuki_hub_ip",
                         "NUKI Hub IP",
                         name,
                         baseTopic,
                         _lockPath + mqtt_topic_info_nuki_hub_ip,
                         deviceType,
                         "",
                         "",
                         "diagnostic",
                         "",
                         { { "enabled_by_default", "true" },
                           {"ic", "mdi:ip"}});

        // LED enabled
        publishHassTopic("switch",
                         "led_enabled",
                         uidString,
                         "_led_enabled",
                         "LED enabled",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_led_enabled,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_led_enabled,
                         { { "ic", "mdi:led-variant-on" },
                                          { "pl_on", "1" },
                                          { "pl_off", "0" },
                                          { "state_on", "1" },
                                          { "state_off", "0" }});

        // Button enabled
        publishHassTopic("switch",
                         "button_enabled",
                         uidString,
                         "_button_enabled",
                         "Button enabled",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_config_button_enabled,
                         deviceType,
                         "",
                         "",
                         "config",
                         String("~") + mqtt_topic_config_button_enabled,
                         { { "ic", "mdi:radiobox-marked" },
                           { "pl_on", "1" },
                           { "pl_off", "0" },
                           { "state_on", "1" },
                           { "state_off", "0" }});
 
        // Unlatch
        publishHassTopic("button",
                         "unlatch",
                         uidString,
                         "_unlatch_button",
                         "Unlatch",
                         name,
                         baseTopic,
                         "",
                         deviceType,
                         "",
                         "",
                         "",
                         String("~") + mqtt_topic_lock_action,
                         { { "enabled_by_default", "false" },
                           { "pl_prs", "unlatch" }}); 
                         
    }
}


void Network::publishHASSConfigAdditionalButtons(char *deviceType, const char *baseTopic, char *name, char *uidString, const char *availabilityTopic, const bool &hasKeypad, char *lockAction, char *unlockAction, char *openAction, char *lockedState, char *unlockedState)
{
    // Lock 'n' Go
    publishHassTopic("button",
                     "lockngo",
                     uidString,
                     "_lock_n_go_button",
                     "Lock 'n' Go",
                     name,
                     baseTopic,
                     "",
                     deviceType,
                     "",
                     "",
                     "",
                     String("~") + mqtt_topic_lock_action,
                     { { "enabled_by_default", "false" },
                       { "pl_prs", "lockNgo" }});

    // Lock 'n' Go with unlatch
    publishHassTopic("button",
                     "lockngounlatch",
                     uidString,
                     "_lock_n_go_unlatch_button",
                     "Lock 'n' Go with unlatch",
                     name,
                     baseTopic,
                     "",
                     deviceType,
                     "",
                     "",
                     "",
                     String("~") + mqtt_topic_lock_action,
                     { { "enabled_by_default", "false" },
                       { "pl_prs", "lockNgoUnlatch" }});
}

void Network::publishHASSConfigBatLevel(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    String discoveryTopic = _preferences->getString(preference_mqtt_hass_discovery);

    if (discoveryTopic != "")
    {
        publishHassTopic("sensor",
                         "battery_level",
                         uidString,
                         "_battery_level",
                         "battery level",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_battery_level,
                         deviceType,
                         "battery",
                         "measurement",
                         "diagnostic",
                         "",
                         { {"unit_of_meas", "%"} });
    }
}

void Network::publishHASSConfigDoorSensor(char *deviceType, const char *baseTopic, char *name, char *uidString,
                                        char *lockAction, char *unlockAction, char *openAction, char *lockedState,
                                        char *unlockedState)
{
    String discoveryTopic = _preferences->getString(preference_mqtt_hass_discovery);

    if (discoveryTopic != "")
    {
        publishHassTopic("binary_sensor",
                         "door_sensor",
                         uidString,
                         "_door_sensor",
                         "door sensor",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_lock_door_sensor_state,
                         deviceType,
                         "door",
                         "",
                         "",
                         "",
                         {{"pl_on", "doorOpened"},
                          {"pl_off", "doorClosed"},
                          {"pl_not_avail", "unavailable"}});
    }
}

void Network::publishHASSConfigRingDetect(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    String discoveryTopic = _preferences->getString(preference_mqtt_hass_discovery);

    if (discoveryTopic != "")
    {
        publishHassTopic("binary_sensor",
                         "ring",
                         uidString,
                         "_ring_detect",
                         "ring detect",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_lock_state,
                         deviceType,
                         "sound",
                         "",
                         "",
                         "",
                         {{"pl_on", "ring"},
                          {"pl_off", "locked"}});
    }
}


void Network::publishHASSConfigLedBrightness(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    publishHassTopic("number",
                     "led_brightness",
                     uidString,
                     "_led_brightness",
                     "LED brightness",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_config_led_brightness,
                     deviceType,
                     "",
                     "",
                     "config",
                     String("~") + mqtt_topic_config_led_brightness,
                     { { "ic", "mdi:brightness-6" },
                       { "min", "0" },
                       { "max", "5" }});
}

void Network::publishHASSConfigSoundLevel(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    publishHassTopic("number",
                     "sound_level",
                     uidString,
                     "_sound_level",
                     "Sound level",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_config_sound_level,
                     deviceType,
                     "",
                     "",
                     "config",
                     String("~") + mqtt_topic_config_sound_level,
                     { { "ic", "mdi:volume-source" },
                       { "min", "0" },
                       { "max", "255" },
                       { "mode", "slider" },
                       { "step", "25.5" }});
}


void Network::publishHASSConfigAccessLog(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    publishHassTopic("sensor",
                     "last_action_authorization",
                     uidString,
                     "_last_action_authorization",
                     "Last action authorization",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_lock_log,
                     deviceType,
                     "",
                     "",
                     "diagnostic",
                     "",
                     { { "ic", "mdi:format-list-bulleted" },
                                      { "value_template", "{{ (value_json|selectattr('type', 'eq', 'LockAction')|selectattr('action', 'in', ['Lock', 'Unlock', 'Unlatch'])|first|default).authorizationName|default }}" }});
}

void Network::publishHASSConfigKeypadAttemptInfo(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    publishHassTopic("sensor",
                     "keypad_status",
                     uidString,
                     "_keypad_stats",
                     "Keypad status",
                     name,
                     baseTopic,
                     String("~") + mqtt_topic_lock_log,
                     deviceType,
                     "",
                     "",
                     "diagnostic",
                     "",
                     { { "ic", "mdi:drag-vertical" },
                                      { "value_template", "{{ (value_json|selectattr('type', 'eq', 'KeypadAction')|first|default).completionStatus|default }}" }});
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
        publishHassTopic("sensor",
                         "wifi_signal_strength",
                         uidString,
                         "_wifi_signal_strength",
                         "wifi signal strength",
                         name,
                         baseTopic,
                         _lockPath + mqtt_topic_wifi_rssi,
                         deviceType,
                         "signal_strength",
                         "measurement",
                         "diagnostic",
                         "",
                         { {"unit_of_meas", "dBm"} });
    }
}

void Network::publishHASSBleRssiConfig(char *deviceType, const char *baseTopic, char *name, char *uidString)
{
    String discoveryTopic = _preferences->getString(preference_mqtt_hass_discovery);

    if (discoveryTopic != "")
    {
        publishHassTopic("sensor",
                         "bluetooth_signal_strength",
                         uidString,
                         "_bluetooth_signal_strength",
                         "bluetooth signal strength",
                         name,
                         baseTopic,
                         String("~") + mqtt_topic_lock_rssi,
                         deviceType,
                         "signal_strength",
                         "measurement",
                         "diagnostic",
                         "",
                         { {"unit_of_meas", "dBm"} });
    }
}

void Network::publishHassTopic(const String& mqttDeviceType,
                               const String& mattDeviceName,
                               const String& uidString,
                               const String& uidStringPostfix,
                               const String& displayName,
                               const String& name,
                               const String& baseTopic,
                               const String& stateTopic,
                               const String& deviceType,
                               const String& deviceClass,
                               const String& stateClass,
                               const String& entityCat,
                               const String& commandTopic,
                               std::vector<std::pair<char*, char*>> additionalEntries
)
{
    String discoveryTopic = _preferences->getString(preference_mqtt_hass_discovery);

    if (discoveryTopic != "")
    {
        DynamicJsonDocument json(_bufferSize);

        // Battery level
        json.clear();
        auto dev = json.createNestedObject("dev");
        auto ids = dev.createNestedArray("ids");
        ids.add(String("nuki_") + uidString);
        json["dev"]["mf"] = "Nuki";
        json["dev"]["mdl"] = deviceType;
        json["dev"]["name"] = name;
        json["~"] = baseTopic;
        json["name"] = displayName;
        json["unique_id"] = String(uidString) + uidStringPostfix;
        if(deviceClass != "")
        {
            json["dev_cla"] = deviceClass;
        }
        
        if(stateTopic != "")
        {
            json["stat_t"] = stateTopic;
        }
        
        if(stateClass != "")
        {
            json["stat_cla"] = stateClass;
        }
        if(entityCat != "")
        {
            json["ent_cat"] = entityCat;
        }
        if(commandTopic != "")
        {
            json["cmd_t"] = commandTopic;
        }
        
        json["avty"]["t"] = _lockPath + mqtt_topic_mqtt_connection_state;

        for(const auto& entry : additionalEntries)
        {
            if(strcmp(entry.second, "true") == 0)
            {
                json[entry.first] = true;
            }
            else if(strcmp(entry.second, "false") == 0)
            {
                json[entry.first] = false;
            }
            else
            {
                json[entry.first] = entry.second;
            }
        }

        serializeJson(json, _buffer, _bufferSize);

        String path = discoveryTopic;
        path.concat("/");
        path.concat(mqttDeviceType);
        path.concat("/");
        path.concat(uidString);
        path.concat("/");
        path.concat(mattDeviceName);
        path.concat("/config");

        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, _buffer);
    }
}


void Network::removeHassTopic(const String& mqttDeviceType, const String& mattDeviceName, const String& uidString)
{
    String discoveryTopic = _preferences->getString(preference_mqtt_hass_discovery);

    if (discoveryTopic != "")
    {
        String path = discoveryTopic;
        path.concat("/");
        path.concat(mqttDeviceType);
        path.concat("/");
        path.concat(uidString);
        path.concat("/");
        path.concat(mattDeviceName);
        path.concat("/config");

        _device->mqttPublish(path.c_str(), MQTT_QOS_LEVEL, true, "");
    }
}


void Network::removeHASSConfig(char* uidString)
{
    String discoveryTopic = _preferences->getString(preference_mqtt_hass_discovery);

    if(discoveryTopic != "")
    {
        removeHassTopic("lock", "smartlock", uidString);
        removeHassTopic("binary_sensor", "battery_low", uidString);
        removeHassTopic("binary_sensor", "keypad_battery_low", uidString);
        removeHassTopic("sensor", "battery_voltage", uidString);        
        removeHassTopic("sensor", "trigger", uidString);
        removeHassTopic("binary_sensor", "mqtt_connected", uidString);
        removeHassTopic("switch", "reset", uidString);        
        removeHassTopic("sensor", "firmware_version", uidString);
        removeHassTopic("sensor", "hardware_version", uidString);
        removeHassTopic("sensor", "nuki_hub_version", uidString);
        removeHassTopic("sensor", "nuki_hub_latest", uidString);
        removeHassTopic("update", "nuki_hub_update", uidString);
        removeHassTopic("sensor", "nuki_hub_ip", uidString);
        removeHassTopic("switch", "led_enabled", uidString);  
        removeHassTopic("switch", "button_enabled", uidString);  
        removeHassTopic("button", "unlatch", uidString);
        removeHassTopic("button", "lockngo", uidString);
        removeHassTopic("button", "lockngounlatch", uidString);
        removeHassTopic("sensor", "battery_level", uidString);
        removeHassTopic("binary_sensor", "door_sensor", uidString);
        removeHassTopic("binary_sensor", "ring", uidString);
        removeHassTopic("number", "led_brightness", uidString);
        removeHassTopic("sensor", "sound_level", uidString);        
        removeHassTopic("number", "sound_level", uidString);
        removeHassTopic("sensor", "last_action_authorization", uidString);
        removeHassTopic("sensor", "keypad_status", uidString);
        removeHassTopic("sensor", "wifi_signal_strength", uidString);
        removeHassTopic("sensor", "bluetooth_signal_strength", uidString);
    }
}

void Network::removeHASSConfigTopic(char *deviceType, char *name, char *uidString)
{
    removeHassTopic(deviceType, name, uidString);
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

void Network::addReconnectedCallback(std::function<void()> reconnectedCallback)
{
    _reconnectedCallbacks.push_back(reconnectedCallback);
}

void Network::clearWifiFallback()
{
    memset(WiFi_fallbackDetect, 0, sizeof(WiFi_fallbackDetect));
}

void Network::disableMqtt()
{
    _device->disableMqtt();
    _mqttEnabled = false;
}

NetworkDevice *Network::device()
{
    return _device;
}
