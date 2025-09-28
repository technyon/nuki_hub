#include "NukiNetwork.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "Config.h"
#include "RestartReason.h"
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include "util/NetworkDeviceInstantiator.h"
#ifndef CONFIG_IDF_TARGET_ESP32H2
#include "networkDevices/WifiDevice.h"
#endif
#include "networkDevices/EthernetDevice.h"
#include "hal/wdt_hal.h"
#include "esp_mac.h"
#include <ESP32Ping.h>

NukiNetwork* NukiNetwork::_inst = nullptr;

extern bool timeSynced;
extern bool wifiFallback;
extern bool disableNetwork;
extern bool forceEnableWebServer;
extern const uint8_t x509_crt_imported_bundle_bin_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_imported_bundle_bin_end[]   asm("_binary_x509_crt_bundle_end");

#ifndef NUKI_HUB_UPDATER
NukiNetwork::NukiNetwork(Preferences *preferences, Gpio* gpio, char* buffer, size_t bufferSize, ImportExport* importExport)
    : _preferences(preferences),
      _gpio(gpio),
      _buffer(buffer),
      _bufferSize(bufferSize),
      _importExport(importExport)
#else
NukiNetwork::NukiNetwork(Preferences *preferences)
    : _preferences(preferences)
#endif
{
    _inst = this;
    setupDevice();
}

void NukiNetwork::setupDevice()
{
    _ipConfiguration = new IPConfiguration(_preferences);
    int hardwareDetect = _preferences->getInt(preference_network_hardware, 0);
    Log->print("Hardware detect: ");
    Log->println(hardwareDetect);

    _firstBootAfterDeviceChange = _preferences->getBool(preference_ntw_reconfigure, false);

    if(hardwareDetect == 0)
    {
#ifndef CONFIG_IDF_TARGET_ESP32H2
        hardwareDetect = 1;
#else
        hardwareDetect = 11;
        _preferences->putInt(preference_network_custom_addr, 1);
        _preferences->putInt(preference_network_custom_cs, 8);
        _preferences->putInt(preference_network_custom_irq, 9);
        _preferences->putInt(preference_network_custom_rst, 10);
        _preferences->putInt(preference_network_custom_sck, 11);
        _preferences->putInt(preference_network_custom_miso, 12);
        _preferences->putInt(preference_network_custom_mosi, 13);
        _preferences->putBool(preference_ntw_reconfigure, true);
#endif
        _preferences->putInt(preference_network_hardware, hardwareDetect);
    }

    if(wifiFallback == true)
    {
#ifndef CONFIG_IDF_TARGET_ESP32H2
        if(!_firstBootAfterDeviceChange)
        {
            Log->println("Failed to connect to network. Wi-Fi fallback is disabled, rebooting.");
            wifiFallback = false;
            sleep(5);
            restartEsp(RestartReason::NetworkDeviceCriticalFailureNoWifiFallback);
        }

        Log->println("Switching to Wi-Fi device as fallback.");
        _networkDeviceType = NetworkDeviceType::WiFi;
#else
        int custEth = _preferences->getInt(preference_network_custom_phy, 0);

        if(custEth<3)
        {
            custEth++;
        }
        else
        {
            custEth = 0;
        }
        _preferences->putInt(preference_network_custom_phy, custEth);
        _preferences->putBool(preference_ntw_reconfigure, true);
#endif
    }
    else
    {
        _networkDeviceType = NetworkUtil::GetDeviceTypeFromPreference(hardwareDetect, _preferences->getInt(preference_network_custom_phy, 0));
    }

    _device = NetworkDeviceInstantiator::Create(_networkDeviceType, _hostname, _preferences, _ipConfiguration);

    Log->print("Network device: ");
    Log->println(_device->deviceName());

#ifndef NUKI_HUB_UPDATER
    _hadiscovery = new HomeAssistantDiscovery(_device, _preferences, _buffer, _bufferSize);
#endif
}

void NukiNetwork::reconfigureDevice()
{
    _device->reconfigure();
}

void NukiNetwork::scan(bool passive, bool async)
{
    _device->scan(passive, async);
}

const bool NukiNetwork::isApOpen() const
{
    return _device->isApOpen();
}

const String NukiNetwork::networkDeviceName() const
{
    return _device->deviceName();
}

const String NukiNetwork::networkBSSID() const
{
    return _device->BSSIDstr();
}

const NetworkDeviceType NukiNetwork::networkDeviceType()
{
    return _networkDeviceType;
}

void NukiNetwork::setKeepAliveCallback(std::function<void()> reconnectTick)
{
    _keepAliveCallback = reconnectTick;
}


void NukiNetwork::clearWifiFallback()
{
    wifiFallback = false;
}

const bool NukiNetwork::isConnected() const
{
    return _device->isConnected();
}

const bool NukiNetwork::isInternetConnected() const
{
    return _hasInternet;
}

const bool NukiNetwork::mqttConnected()
{
#ifndef NUKI_HUB_UPDATER
    return _device->mqttConnected();
#else
    return false;
#endif
}

const bool NukiNetwork::wifiConnected()
{
    if(_networkDeviceType != NetworkDeviceType::WiFi)
    {
        return true;
    }
    else
    {
        return _device->isConnected();
    }
}

const String NukiNetwork::localIP()
{
    return _device->localIP();
}

#ifdef NUKI_HUB_UPDATER
void NukiNetwork::initialize()
{
    _hostname = _preferences->getString(preference_hostname, "");

    if(_hostname == "")
    {
        char _nukiHubUidString[20];
        uint8_t mac[8];
        esp_efuse_mac_get_default(mac);
        uint64_t curDevId;
        memcpy(&curDevId, &mac, 8);
        sprintf(_nukiHubUidString, "%" PRIu64, curDevId);
        _hostname = (String)"NH" + _nukiHubUidString;
        _preferences->putString(preference_hostname, _hostname);
    }

    strcpy(_hostnameArr, _hostname.c_str());
    _device->initialize();

    Log->print("Host name: ");
    Log->println(_hostname);
}

bool NukiNetwork::update()
{
    wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
    wdt_hal_feed(&rtc_wdt_ctx);
    wdt_hal_write_protect_enable(&rtc_wdt_ctx);
    _device->update();
    return true;
}
#else
void NukiNetwork::initialize()
{
    readSettings();
    setMQTTConnectionSettings();

    _gpio->addCallback([this](const GpioAction& action, const int& pin)
    {
        gpioActionCallback(action, pin);
    });

    if(!disableNetwork)
    {
        _hostname = _preferences->getString(preference_hostname, "");

        if(_hostname == "")
        {
            char _nukiHubUidString[20];
            uint8_t mac[8];
            esp_efuse_mac_get_default(mac);
            uint64_t curDevId;
            memcpy(&curDevId, &mac, 8);
            sprintf(_nukiHubUidString, "%" PRIu64, curDevId);
            _hostname = (String)"NH" + _nukiHubUidString;
            _preferences->putString(preference_hostname, _hostname);
        }

        strcpy(_hostnameArr, _hostname.c_str());
        _device->initialize();

        Log->print("Host name: ");
        Log->println(_hostname);

        _device->mqttSetClientId(_hostnameArr);
        _device->mqttSetCleanSession(false);
        _device->mqttSetKeepAlive(60);

        char gpioPath[250];
        bool rebGpio = rebuildGpio();

        if(rebGpio)
        {
            Log->println("Rebuild MQTT GPIO structure");
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
                    publishString(_lockPath.c_str(), gpioPath, "input", false);
                    buildMqttPath(gpioPath, {mqtt_topic_gpio_prefix, (mqtt_topic_gpio_pin + std::to_string(pinEntry.pin)).c_str(), mqtt_topic_gpio_state});
                    publishString(_lockPath.c_str(), gpioPath, std::to_string(digitalRead(pinEntry.pin)).c_str(), _retainGpio);
                }
                break;
            case PinRole::GeneralOutput:
                if(rebGpio)
                {
                    buildMqttPath(gpioPath, {mqtt_topic_gpio_prefix, (mqtt_topic_gpio_pin + std::to_string(pinEntry.pin)).c_str(), mqtt_topic_gpio_role});
                    publishString(_lockPath.c_str(), gpioPath, "output", false);
                    buildMqttPath(gpioPath, {mqtt_topic_gpio_prefix, (mqtt_topic_gpio_pin + std::to_string(pinEntry.pin)).c_str(), mqtt_topic_gpio_state});
                    publishString(_lockPath.c_str(), gpioPath, "0", false);
                }
                buildMqttPath(gpioPath, {mqtt_topic_gpio_prefix, (mqtt_topic_gpio_pin + std::to_string(pinEntry.pin)).c_str(), mqtt_topic_gpio_state});
                subscribe(_lockPath.c_str(), gpioPath);
                break;
            default:
                break;
            }
        }
    }
}

void NukiNetwork::readSettings()
{
    _webEnabled = _preferences->getBool(preference_webserver_enabled, true);
    _disableNetworkIfNotConnected = _preferences->getBool(preference_disable_network_not_connected, false);
    _restartOnDisconnect = _preferences->getBool(preference_restart_on_disconnect, false);
    _checkUpdates = _preferences->getBool(preference_check_updates, false);
    _rssiPublishInterval = _preferences->getInt(preference_rssi_publish_interval, 0) * 1000;
    _retainGpio = _preferences->getBool(preference_retain_gpio, false);
    _haEnabled = _preferences->getBool(preference_mqtt_hass_enabled, false);
    
    if(_rssiPublishInterval == 0)
    {
        _rssiPublishInterval = 60000;
        _preferences->putInt(preference_rssi_publish_interval, 60);
    }

    _networkTimeout = _preferences->getInt(preference_network_timeout, 0);
    if(_networkTimeout == 0)
    {
        _networkTimeout = -1;
        _preferences->putInt(preference_network_timeout, _networkTimeout);
    }

    _publishDebugInfo = _preferences->getBool(preference_publish_debug_info, false);
}

void NukiNetwork::setMQTTConnectionSettings()
{
    String mqttPath = _preferences->getString(preference_mqtt_lock_path, "");
    memset(_nukiHubPath, 0, sizeof(_nukiHubPath));
    size_t len = mqttPath.length();
    for(int i=0; i < len; i++)
    {
        _nukiHubPath[i] = mqttPath.charAt(i);
    }

    String maintenancePathPrefix = _preferences->getString(preference_mqtt_lock_path);
    memset(_maintenancePathPrefix, 0, sizeof(_maintenancePathPrefix));
    len = maintenancePathPrefix.length();
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

    if(_preferences->getString(preference_mqtt_hass_discovery, "") != "" && !_preferences->getBool(preference_mqtt_hass_enabled, false))
    {
        _preferences->putBool(preference_mqtt_hass_enabled, true);
    }

    memset(_mqttBrokerAddr, 0, sizeof(_mqttBrokerAddr));
    memset(_mqttUser, 0, sizeof(_mqttUser));
    memset(_mqttPass, 0, sizeof(_mqttPass));

    String brokerAddr = _preferences->getString(preference_mqtt_broker);
    strcpy(_mqttBrokerAddr, brokerAddr.c_str());

    _mqttPort = _preferences->getInt(preference_mqtt_broker_port, 0);

    if(_mqttPort == 0)
    {
        _mqttPort = 1883;
        _preferences->putInt(preference_mqtt_broker_port, _mqttPort);
    }

    String mqttUser = _preferences->getString(preference_mqtt_user);
    if(mqttUser.length() > 0)
    {
        len = mqttUser.length();
        for(int i=0; i < len; i++)
        {
            _mqttUser[i] = mqttUser.charAt(i);
        }
    }

    String mqttPass = _preferences->getString(preference_mqtt_password);
    if(mqttPass.length() > 0)
    {
        len = mqttPass.length();
        for(int i=0; i < len; i++)
        {
            _mqttPass[i] = mqttPass.charAt(i);
        }
    }

    Log->print("MQTT Broker: ");
    Log->print(_mqttBrokerAddr);
    Log->print(":");
    Log->println(_mqttPort);

#ifndef NUKI_HUB_UPDATER
    _device->mqttOnConnect([&](bool sessionPresent)
    {
        onMqttConnect(sessionPresent);
    });
    _device->mqttOnDisconnect([&](espMqttClientTypes::DisconnectReason reason)
    {
        onMqttDisconnect(reason);
    });
#endif
}

const int NukiNetwork::getRestartServices()
{
    int restartServices = _restartServices;
    _restartServices = 0;
    return restartServices;
}

void NukiNetwork::setRestartServices(bool reconnect)
{
    if (reconnect)
    {
        _restartServices = 2;
    }
    else
    {
        _restartServices = 1;
    }
}

bool NukiNetwork::update()
{
    wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
    wdt_hal_feed(&rtc_wdt_ctx);
    wdt_hal_write_protect_enable(&rtc_wdt_ctx);
    int64_t ts = espMillis();
    _device->update();

    if(_importExport->getTOTPEnabled() && _importExport->_invalidCount > 0 && (ts - (120000 * _importExport->_invalidCount)) > _importExport->_lastCodeCheck)
    {
        _importExport->_invalidCount--;
    }

    if(_importExport->getBypassEnabled() && _importExport->_invalidCount2 > 0 && (ts - (120000 * _importExport->_invalidCount2)) > _importExport->_lastCodeCheck2)
    {
        _importExport->_invalidCount2--;
    }

    if(disableNetwork || !_mqttEnabled || _device->isApOpen())
    {
        return false;
    }

    if(!_device->isConnected() || (_mqttConnectCounter > 15 && !_firstConnect))
    {
        _mqttConnectCounter = 0;

        if(_firstDisconnected)
        {
            _firstDisconnected = false;
            _device->mqttDisconnect(true);
        }

        if(_restartOnDisconnect && espMillis() > 60000)
        {
            restartEsp(RestartReason::RestartOnDisconnectWatchdog);
        }
        else if(_disableNetworkIfNotConnected && espMillis() > 60000)
        {
            disableNetwork = true;
            restartEsp(RestartReason::DisableNetworkIfNotConnected);
        }
    }

    if(_logIp && _device->isConnected() && !_device->localIP().equals("0.0.0.0"))
    {
        _logIp = false;
        Log->print("IP: ");
        Log->println(_device->localIP());
        _firstDisconnected = true;
        
        checkInternetConnectivity();
    }

    if(!_logIp && _device->isConnected() && !_device->mqttConnected() )
    {
        bool success = reconnect();
        if(!success)
        {
            if (esp_task_wdt_status(NULL) == ESP_OK)
            {
                esp_task_wdt_reset();
            }
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            _mqttConnectCounter++;
            return false;
        }

        _mqttConnectCounter = 0;
        if(forceEnableWebServer && !_webEnabled)
        {
            forceEnableWebServer = false;
            if (esp_task_wdt_status(NULL) == ESP_OK)
            {
                esp_task_wdt_reset();
            }
            vTaskDelay(200 / portTICK_PERIOD_MS);
            setRestartServices(false);
        }
        else if(!_webEnabled)
        {
            forceEnableWebServer = false;
        }
        if (esp_task_wdt_status(NULL) == ESP_OK)
        {
            esp_task_wdt_reset();
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    if(!_device->isConnected() || !_device->mqttConnected() )
    {
        if(_networkTimeout > 0 && (ts - _lastConnectedTs > _networkTimeout * 1000) && ts > 60000)
        {
            if(!_webEnabled)
            {
                forceEnableWebServer = true;
            }
            Log->println("Network timeout has been reached, restarting ...");
            if (esp_task_wdt_status(NULL) == ESP_OK)
            {
                esp_task_wdt_reset();
            }
            vTaskDelay(200 / portTICK_PERIOD_MS);
            restartEsp(RestartReason::NetworkTimeoutWatchdog);
        }
        if (esp_task_wdt_status(NULL) == ESP_OK)
        {
            esp_task_wdt_reset();
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        return false;
    }

    _lastConnectedTs = ts;

    if(_device->signalStrength() != 127 && _rssiPublishInterval > 0 && ts - _lastRssiTs > _rssiPublishInterval)
    {
        _lastRssiTs = ts;
        int8_t rssi = _device->signalStrength();

        if(rssi != _lastRssi)
        {
            publishInt(_maintenancePathPrefix, mqtt_topic_wifi_rssi, _device->signalStrength(), true);
            _lastRssi = rssi;
        }
    }

    if(_overwriteNukiHubConfigTS > 0 && espMillis() > _overwriteNukiHubConfigTS)
    {
        publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_json, "--", true);
        _overwriteNukiHubConfigTS = -1;
    }

    if(_lastMaintenanceTs == 0 || (ts - _lastMaintenanceTs) > 30000)
    {
        int64_t curUptime = ts / 1000 / 60;
        if(curUptime > _publishedUpTime)
        {
            publishULong(_maintenancePathPrefix, mqtt_topic_uptime, curUptime, true);
            _publishedUpTime = curUptime;
        }
        //publishString(_maintenancePathPrefix, mqtt_topic_mqtt_connection_state, "online", true);

        if(_lastMaintenanceTs == 0)
        {
            publishString(_maintenancePathPrefix, mqtt_topic_restart_reason_fw, getRestartReason().c_str(), true);
            publishString(_maintenancePathPrefix, mqtt_topic_restart_reason_esp, getEspRestartReason().c_str(), true);
            publishString(_maintenancePathPrefix, mqtt_topic_info_nuki_hub_version, NUKI_HUB_VERSION, true);
            publishString(_maintenancePathPrefix, mqtt_topic_info_nuki_hub_build, NUKI_HUB_BUILD, true);
        }
        if(_publishDebugInfo)
        {
            publishUInt(_maintenancePathPrefix, mqtt_topic_freeheap, esp_get_free_heap_size(), true);
        }
        _lastMaintenanceTs = ts;
    }

    if(_checkUpdates && (!_haEnabled || (_haEnabled && _haSetupDone)) && _hasInternet)
    {
        if(_lastUpdateCheckTs == 0 || (ts - _lastUpdateCheckTs) > 86400000)
        {
            _lastUpdateCheckTs = ts;
            bool otaManifestSuccess = false;
            JsonDocument doc;

            NetworkClientSecure *client = new NetworkClientSecure;
            if (client)
            {
                client->setCACertBundle(x509_crt_imported_bundle_bin_start, x509_crt_imported_bundle_bin_end - x509_crt_imported_bundle_bin_start);
                {
                    HTTPClient https;
                    https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
                    https.useHTTP10(true);

                    if (https.begin(*client, GITHUB_OTA_MANIFEST_URL))
                    {
                        int httpResponseCode = https.GET();

                        if (httpResponseCode == HTTP_CODE_OK || httpResponseCode == HTTP_CODE_MOVED_PERMANENTLY)
                        {
                            DeserializationError jsonError = deserializeJson(doc, https.getStream());

                            if (!jsonError)
                            {
                                otaManifestSuccess = true;
                            }
                        }
                    }
                    https.end();
                }
                delete client;
            }

            if (otaManifestSuccess)
            {
                String currentVersion = NUKI_HUB_VERSION;

                if(atof(doc["release"]["version"]) >= atof(currentVersion.c_str()))
                {
                    _latestVersion = doc["release"]["fullversion"];
                }
                else if(currentVersion.indexOf("beta") > 0)
                {
                    _latestVersion = doc["beta"]["fullversion"];
                }
                else if(currentVersion.indexOf("master") > 0)
                {
                    _latestVersion = doc["master"]["fullversion"];
                }
                else
                {
                    _latestVersion = doc["release"]["fullversion"];
                }

                publishString(_maintenancePathPrefix, mqtt_topic_info_nuki_hub_latest, _latestVersion, true);

                if(strcmp(_latestVersion, _preferences->getString(preference_latest_version).c_str()) != 0)
                {
                    _preferences->putString(preference_latest_version, _latestVersion);
                }
            }
        }
    }

    for(const auto& gpioTs : _gpioTs)
    {
        uint8_t pin = gpioTs.first;
        int64_t ts = gpioTs.second;
        if(ts != 0 && ((espMillis() - ts) >= GPIO_DEBOUNCE_TIME))
        {
            _gpioTs[pin] = 0;

            uint8_t pinState = digitalRead(pin) == HIGH ? 1 : 0;
            char gpioPath[250];
            buildMqttPath(gpioPath, {mqtt_topic_gpio_prefix, (mqtt_topic_gpio_pin + std::to_string(pin)).c_str(), mqtt_topic_gpio_state});
            publishInt(_lockPath.c_str(), gpioPath, pinState, _retainGpio);

            Log->print("GPIO ");
            Log->print(pin);
            Log->print(" (Input) --> ");
            Log->println(pinState);
        }
    }

    return true;
}

void NukiNetwork::checkInternetConnectivity()
{
    _hasInternet = Ping.ping("github.com", 3);
}

void NukiNetwork::onMqttConnect(const bool &sessionPresent)
{
    _connectReplyReceived = true;
}

void NukiNetwork::onMqttDisconnect(const espMqttClientTypes::DisconnectReason &reason)
{
    _connectReplyReceived = false;
    Log->print("MQTT disconnected. Reason: ");
    switch(reason)
    {
    case espMqttClientTypes::DisconnectReason::USER_OK:
        Log->println("USER_OK");
        break;
    case espMqttClientTypes::DisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
        Log->println("MQTT_UNACCEPTABLE_PROTOCOL_VERSION");
        break;
    case espMqttClientTypes::DisconnectReason::MQTT_IDENTIFIER_REJECTED:
        Log->println("MQTT_IDENTIFIER_REJECTED");
        break;
    case espMqttClientTypes::DisconnectReason::MQTT_SERVER_UNAVAILABLE:
        Log->println("MQTT_SERVER_UNAVAILABLE");
        break;
    case espMqttClientTypes::DisconnectReason::MQTT_MALFORMED_CREDENTIALS:
        Log->println("MQTT_MALFORMED_CREDENTIALS");
        break;
    case espMqttClientTypes::DisconnectReason::MQTT_NOT_AUTHORIZED:
        Log->println("MQTT_NOT_AUTHORIZED");
        break;
    case espMqttClientTypes::DisconnectReason::TLS_BAD_FINGERPRINT:
        Log->println("TLS_BAD_FINGERPRINT");
        break;
    case espMqttClientTypes::DisconnectReason::TCP_DISCONNECTED:
        Log->println("TCP_DISCONNECTED");
        break;
    default:
        Log->println("Unknown");
        break;
    }
}

bool NukiNetwork::reconnect(bool force)
{
    _mqttConnectionState = 0;

    while (force || (!_device->mqttConnected() && espMillis() > _nextReconnect))
    {
        if (force)
        {
            _mqttReceivers.clear();
            _device->mqttRestart();
            setMQTTConnectionSettings();
        }

        force = false;

        if(strcmp(_mqttBrokerAddr, "") == 0)
        {
            Log->println("MQTT Broker not configured, aborting connection attempt.");
            _nextReconnect = espMillis() + 5000;

            if(_device->isConnected())
            {
                _lastConnectedTs = espMillis();
            }
            return false;
        }

        Log->println("Attempting MQTT connection");

        _connectReplyReceived = false;

        if(strlen(_mqttUser) == 0)
        {
            Log->println("MQTT: Connecting without credentials");
        }
        else
        {
            Log->print("MQTT: Connecting with user: ");
            Log->println(_mqttUser);
            _device->mqttSetCredentials(_mqttUser, _mqttPass);
        }

        _device->mqttSetWill(_mqttConnectionStateTopic, 1, true, _lastWillPayload);
        _device->mqttSetServer(_mqttBrokerAddr, _mqttPort);
        _device->mqttConnect();

        int64_t timeout = espMillis() + 60000;

        while(!_connectReplyReceived && espMillis() < timeout)
        {
            if (esp_task_wdt_status(NULL) == ESP_OK)
            {
                esp_task_wdt_reset();
            }
            vTaskDelay(50 / portTICK_PERIOD_MS);
            _device->update();
            if(_keepAliveCallback != nullptr)
            {
                _keepAliveCallback();
            }
        }

        if (_device->mqttConnected())
        {
            Log->println("MQTT connected");
            _mqttConnectedTs = millis();
            _mqttConnectionState = 1;
            if (esp_task_wdt_status(NULL) == ESP_OK)
            {
                esp_task_wdt_reset();
            }
            vTaskDelay(100 / portTICK_PERIOD_MS);
            _device->mqttOnMessage(onMqttDataReceivedCallback);

            if(_firstConnect)
            {
                _firstConnect = false;

                if(_preferences->getBool(preference_reset_mqtt_topics, false))
                {
                    char mqttLockPath[181] = {0};
                    char mqttOpenerPath[181] = {0};
                    char mqttOldOpenerPath[181] = {0};
                    char mqttOldOpenerPath2[181] = {0};
                    String mqttPath = _preferences->getString(preference_mqtt_lock_path, "");
                    mqttPath.concat("/lock");

                    size_t len = mqttPath.length();
                    for(int i=0; i < len; i++)
                    {
                        mqttLockPath[i] = mqttPath.charAt(i);
                    }

                    mqttPath = _preferences->getString(preference_mqtt_lock_path, "");
                    mqttPath.concat("/opener");

                    len = mqttPath.length();
                    for(int i=0; i < len; i++)
                    {
                        mqttOpenerPath[i] = mqttPath.charAt(i);
                    }

                    mqttPath = _preferences->getString(preference_mqtt_opener_path, "");

                    len = mqttPath.length();
                    for(int i=0; i < len; i++)
                    {
                        mqttOldOpenerPath[i] = mqttPath.charAt(i);
                    }

                    mqttPath = _preferences->getString(preference_mqtt_opener_path, "");
                    mqttPath.concat("/lock");

                    len = mqttPath.length();
                    for(int i=0; i < len; i++)
                    {
                        mqttOldOpenerPath2[i] = mqttPath.charAt(i);
                    }

                    MqttTopics mqttTopics;

                    const std::vector<char*> mqttTopicsKeys = mqttTopics.getMqttTopics();

                    for(const auto& topic : mqttTopicsKeys)
                    {
                        removeTopic(_maintenancePathPrefix, topic);
                        removeTopic(mqttLockPath, topic);
                        removeTopic(mqttOpenerPath, topic);
                        if (len > 5)
                        {
                            removeTopic(mqttOldOpenerPath, topic);
                            removeTopic(mqttOldOpenerPath2, topic);
                        }
                    }

                    _preferences->putBool(preference_reset_mqtt_topics, false);
                }

                publishString(_maintenancePathPrefix, mqtt_topic_network_device, _device->deviceName().c_str(), true);

                if(_preferences->getBool(preference_mqtt_hass_enabled, false))
                {
                    setupHASS(0, 0, {0}, {0}, {0}, false, false);
                    _haSetupDone = true;
                }

                initTopic(_maintenancePathPrefix, mqtt_topic_reset, "0");
                subscribe(_maintenancePathPrefix, mqtt_topic_reset);
                initTopic(_maintenancePathPrefix, mqtt_topic_freeheap, "");
                initTopic(_maintenancePathPrefix, mqtt_topic_log, "");
                initTopic(_maintenancePathPrefix, mqtt_topic_wifi_rssi, "");

                if(_preferences->getBool(preference_update_from_mqtt, false))
                {
                    initTopic(_maintenancePathPrefix, mqtt_topic_update, "0");
                    subscribe(_maintenancePathPrefix, mqtt_topic_update);
                }

                if(_preferences->getBool(preference_publish_config, false))
                {
                    initTopic(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_json, "--");
                }

                if(_preferences->getBool(preference_config_from_mqtt, false) || _preferences->getBool(preference_publish_config, false))
                {
                    initTopic(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action, "--");
                    subscribe(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action);
                    initTopic(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action_command_result, "--");
                }

                initTopic(_maintenancePathPrefix, mqtt_topic_webserver_action, "--");
                subscribe(_maintenancePathPrefix, mqtt_topic_webserver_action);
                initTopic(_maintenancePathPrefix, mqtt_topic_webserver_state, (_preferences->getBool(preference_webserver_enabled, true) || forceEnableWebServer ? "1" : "0"));

                for(const auto& it : _initTopics)
                {
                    publish(it.first.c_str(), it.second.c_str(), true);
                }
            }

            for(const String& topic : _subscribedTopics)
            {
                subscribe(topic.c_str(), MQTT_QOS_LEVEL);
            }

            publishString(_maintenancePathPrefix, mqtt_topic_mqtt_connection_state, "online", true);
            publishString(_maintenancePathPrefix, mqtt_topic_info_nuki_hub_ip, _device->localIP().c_str(), true);

            _mqttConnectionState = 2;
            for(const auto& callback : _reconnectedCallbacks)
            {
                callback();
            }
        }
        else
        {
            Log->print("MQTT connect failed");
            _mqttConnectionState = 0;
            _nextReconnect = espMillis() + 5000;
            //_device->mqttDisconnect(true);
        }
    }
    return _mqttConnectionState > 0;
}

void NukiNetwork::subscribe(const char* prefix, const char *path)
{
    char prefixedPath[500];
    buildMqttPath(prefixedPath, { prefix, path });
    _subscribedTopics.push_back(prefixedPath);
}

void NukiNetwork::initTopic(const char *prefix, const char *path, const char *value)
{
    char prefixedPath[500];
    buildMqttPath(prefixedPath, { prefix, path });
    String pathStr = prefixedPath;
    String valueStr = value;
    _initTopics[pathStr] = valueStr;
}

void NukiNetwork::buildMqttPath(const char *path, char *outPath)
{
    int offset = 0;
    char inPath[181] = {0};

    memcpy(inPath, _nukiHubPath, sizeof(_nukiHubPath));

    for(const char& c : inPath)
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

void NukiNetwork::buildMqttPath(char* outPath, std::initializer_list<const char*> paths)
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

void NukiNetwork::registerMqttReceiver(MqttReceiver* receiver)
{
    _mqttReceivers.push_back(receiver);
}

void NukiNetwork::onMqttDataReceivedCallback(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t len, size_t index, size_t total)
{
    uint8_t value[800] = {0};

    size_t l = min(len, sizeof(value)-1);

    for(int i=0; i<l; i++)
    {
        value[i] = payload[i];
    }

    _inst->onMqttDataReceived(properties, topic, value, len, index, total);
}

void NukiNetwork::onMqttDataReceived(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t& len, size_t& index, size_t& total)
{
    if(_mqttConnectedTs == -1 || (millis() - _mqttConnectedTs < 2000))
    {
        return;
    }

    parseGpioTopics(properties, topic, payload, len, index, total);

    onMqttDataReceived(topic, (byte*)payload, index);

    for(auto receiver : _mqttReceivers)
    {
        receiver->onMqttDataReceived(topic, (byte*)payload, index);
    }
}

void NukiNetwork::onMqttDataReceived(const char* topic, byte* payload, const unsigned int length)
{
    char* data = (char*)payload;

    if(comparePrefixedPath(topic, mqtt_topic_reset) && strcmp(data, "1") == 0 && !mqttRecentlyConnected())
    {
        Log->println("Restart requested via MQTT.");
        clearWifiFallback();
        if (esp_task_wdt_status(NULL) == ESP_OK)
        {
            esp_task_wdt_reset();
        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
        restartEsp(RestartReason::RequestedViaMqtt);
    }
    else if(comparePrefixedPath(topic, mqtt_topic_update) && strcmp(data, "1") == 0 && _preferences->getBool(preference_update_from_mqtt, false) && !mqttRecentlyConnected() && _hasInternet)
    {
        Log->println("Update requested via MQTT.");

        bool otaManifestSuccess = false;
        JsonDocument doc;

        NetworkClientSecure *client = new NetworkClientSecure;
        if (client)
        {
            client->setCACertBundle(x509_crt_imported_bundle_bin_start, x509_crt_imported_bundle_bin_end - x509_crt_imported_bundle_bin_start);
            {
                HTTPClient https;
                https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
                https.useHTTP10(true);

                if (https.begin(*client, GITHUB_OTA_MANIFEST_URL))
                {
                    int httpResponseCode = https.GET();

                    if (httpResponseCode == HTTP_CODE_OK || httpResponseCode == HTTP_CODE_MOVED_PERMANENTLY)
                    {
                        DeserializationError jsonError = deserializeJson(doc, https.getStream());

                        if (!jsonError)
                        {
                            otaManifestSuccess = true;
                        }
                    }
                }
                https.end();
            }
            delete client;
        }

        if (otaManifestSuccess)
        {
            String currentVersion = NUKI_HUB_VERSION;

            if(atof(doc["release"]["version"]) >= atof(currentVersion.c_str()))
            {
                if(strcmp(NUKI_HUB_VERSION, doc["release"]["fullversion"].as<const char*>()) == 0 && strcmp(NUKI_HUB_BUILD, doc["release"]["build"].as<const char*>()) == 0 && strcmp(NUKI_HUB_DATE, doc["release"]["time"].as<const char*>()) == 0)
                {
                    Log->println("Nuki Hub is already on the latest release version, OTA update aborted.");
                }
                else
                {
                    _preferences->putString(preference_ota_updater_url, GITHUB_LATEST_UPDATER_BINARY_URL);
                    _preferences->putString(preference_ota_main_url, GITHUB_LATEST_RELEASE_BINARY_URL);
                    Log->println("Updating to latest release version.");
                    if (esp_task_wdt_status(NULL) == ESP_OK)
                    {
                        esp_task_wdt_reset();
                    }
                    vTaskDelay(200 / portTICK_PERIOD_MS);
                    restartEsp(RestartReason::OTAReboot);
                }
            }
            else if(currentVersion.indexOf("beta") > 0)
            {
                if(strcmp(NUKI_HUB_VERSION, doc["beta"]["fullversion"].as<const char*>()) == 0 && strcmp(NUKI_HUB_BUILD, doc["beta"]["build"].as<const char*>()) == 0 && strcmp(NUKI_HUB_DATE, doc["beta"]["time"].as<const char*>()) == 0)
                {
                    Log->println("Nuki Hub is already on the latest beta version, OTA update aborted.");
                }
                else
                {
                    _preferences->putString(preference_ota_updater_url, GITHUB_BETA_UPDATER_BINARY_URL);
                    _preferences->putString(preference_ota_main_url, GITHUB_BETA_RELEASE_BINARY_URL);
                    Log->println("Updating to latest beta version.");
                    if (esp_task_wdt_status(NULL) == ESP_OK)
                    {
                        esp_task_wdt_reset();
                    }
                    vTaskDelay(200 / portTICK_PERIOD_MS);
                    restartEsp(RestartReason::OTAReboot);
                }
            }
            else if(currentVersion.indexOf("master") > 0)
            {
                if(strcmp(NUKI_HUB_VERSION, doc["master"]["fullversion"].as<const char*>()) == 0 && strcmp(NUKI_HUB_BUILD, doc["master"]["build"].as<const char*>()) == 0 && strcmp(NUKI_HUB_DATE, doc["master"]["time"].as<const char*>()) == 0)
                {
                    Log->println("Nuki Hub is already on the latest development version, OTA update aborted.");
                }
                else
                {
                    _preferences->putString(preference_ota_updater_url, GITHUB_MASTER_UPDATER_BINARY_URL);
                    _preferences->putString(preference_ota_main_url, GITHUB_MASTER_RELEASE_BINARY_URL);
                    Log->println("Updating to latest developmemt version.");
                    if (esp_task_wdt_status(NULL) == ESP_OK)
                    {
                        esp_task_wdt_reset();
                    }
                    vTaskDelay(200 / portTICK_PERIOD_MS);
                    restartEsp(RestartReason::OTAReboot);
                }
            }
            else
            {
                if(strcmp(NUKI_HUB_VERSION, doc["release"]["fullversion"].as<const char*>()) == 0 && strcmp(NUKI_HUB_BUILD, doc["release"]["build"].as<const char*>()) == 0 && strcmp(NUKI_HUB_DATE, doc["release"]["time"].as<const char*>()) == 0)
                {
                    Log->println("Nuki Hub is already on the latest release version, OTA update aborted.");
                }
                else
                {
                    _preferences->putString(preference_ota_updater_url, GITHUB_LATEST_UPDATER_BINARY_URL);
                    _preferences->putString(preference_ota_main_url, GITHUB_LATEST_RELEASE_BINARY_URL);
                    Log->println("Updating to latest release version.");
                    if (esp_task_wdt_status(NULL) == ESP_OK)
                    {
                        esp_task_wdt_reset();
                    }
                    vTaskDelay(200 / portTICK_PERIOD_MS);
                    restartEsp(RestartReason::OTAReboot);
                }
            }
        }
        else
        {
            Log->println("Failed to retrieve OTA manifest, OTA update aborted.");
        }
    }
    else if(comparePrefixedPath(topic, mqtt_topic_webserver_action) && !mqttRecentlyConnected())
    {
        if(strcmp(data, "") == 0 ||
                strcmp(data, "--") == 0)
        {
            return;
        }

        if(strcmp(data, "1") == 0)
        {
            if(_preferences->getBool(preference_webserver_enabled, true) || forceEnableWebServer)
            {
                return;
            }
            Log->println("Webserver enabled");
            _preferences->putBool(preference_webserver_enabled, true);
        }
        else if (strcmp(data, "0") == 0)
        {
            if(!_preferences->getBool(preference_webserver_enabled, true) && !forceEnableWebServer)
            {
                return;
            }
            Log->println("Webserver disabled");
            _preferences->putBool(preference_webserver_enabled, false);
        }
        clearWifiFallback();
        if (esp_task_wdt_status(NULL) == ESP_OK)
        {
            esp_task_wdt_reset();
        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
        setRestartServices(false);
    }
    else if(comparePrefixedPath(topic, mqtt_topic_nuki_hub_config_action) && !mqttRecentlyConnected())
    {
        if(strcmp(data, "") == 0 || strcmp(data, "--") == 0)
        {
            return;
        }
        else
        {
            Log->println("JSON config update received");
            JsonDocument doc;

            DeserializationError error = deserializeJson(doc, data);
            if (error)
            {
                Log->println("Invalid JSON for import/export");
                publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action_command_result, "{\"error\": \"jsonInvalid\"}", false);
                publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action, "--", true);
            }
            else
            {
                if(_preferences->getBool(preference_cred_duo_approval, false) && (_importExport->getTOTPEnabled() || _importExport->getDuoEnabled()))
                {
                    if(timeSynced && _importExport->getTOTPEnabled() && !doc["totp"].isNull())
                    {
                        String jsonTotp = doc["totp"];

                        if (!_importExport->checkTOTP(&jsonTotp))
                        {
                            publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action_command_result, "{\"error\": \"totpIncorrect\"}", false);
                            publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action, "--", true);
                            return;
                        }
                    }
                    else if (!timeSynced)
                    {
                        publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action_command_result, "{\"error\": \"duoTimeNotSynced\"}", false);
                        publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action, "--", true);
                        return;
                    }
                    else
                    {
                        bool duoRes = _importExport->startDuoAuth((char*)"Approve Nuki Hub setting change");
                        int duoResult = 2;

                        if (duoRes)
                        {
                            while (duoResult == 2)
                            {
                                duoResult = _importExport->checkDuoApprove();
                                if (esp_task_wdt_status(NULL) == ESP_OK)
                                {
                                    esp_task_wdt_reset();
                                }
                                vTaskDelay(2000 / portTICK_PERIOD_MS);
                            }
                        }

                        if (duoResult != 1)
                        {
                            publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action_command_result, "{\"error\": \"duoApprovalFailed\"}", false);
                            publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action, "--", true);
                            return;
                        }
                    }
                }

                if(!doc["exportHTTPS"].isNull() && _device->isEncrypted())
                {
                    if(_preferences->getBool(preference_publish_config, false))
                    {
                        if(_device->isEncrypted())
                        {
                            JsonDocument json;
                            _importExport->exportHttpsJson(json);
                            serializeJson(json, _buffer, _bufferSize);
                            publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_json, _buffer, false);

                            if (doc["exportHTTPS"].as<int>() > 0)
                            {
                                _overwriteNukiHubConfigTS = espMillis() + (doc["exportHTTPS"].as<int>() * 1000);
                            }
                        }
                        else
                        {
                            publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action_command_result, "{\"error\": \"mqttExportNotEncrypted\"}", false);
                        }
                    }
                    else
                    {
                        publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action_command_result, "{\"error\": \"mqttExportNotEnabled\"}", false);
                    }
                }
                else if(!doc["exportMQTTS"].isNull())
                {
                    if(_preferences->getBool(preference_publish_config, false))
                    {
                        if(_device->isEncrypted())
                        {
                            JsonDocument json;
                            _importExport->exportMqttsJson(json);
                            serializeJson(json, _buffer, _bufferSize);
                            publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_json, _buffer, false);

                            if (doc["exportMQTTS"].as<int>() > 0)
                            {
                                _overwriteNukiHubConfigTS = espMillis() + (doc["exportMQTTS"].as<int>() * 1000);
                            }
                        }
                        else
                        {
                            publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action_command_result, "{\"error\": \"mqttExportNotEncrypted\"}", false);
                        }
                    }
                    else
                    {
                        publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action_command_result, "{\"error\": \"mqttExportNotEnabled\"}", false);
                    }
                }
                else if(!doc["exportNH"].isNull())
                {
                    if(_preferences->getBool(preference_publish_config, false))
                    {
                        bool redacted = false;
                        if(!doc["redacted"].isNull())
                        {
                            if(_device->isEncrypted())
                            {
                                redacted = true;
                            }
                            else
                            {
                                publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action_command_result, "{\"error\": \"mqttExportNotEncrypted\"}", false);
                            }
                        }
                        bool pairing = false;
                        if(!doc["pairing"].isNull())
                        {
                            if(_device->isEncrypted())
                            {
                                pairing = true;
                            }
                            else
                            {
                                publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action_command_result, "{\"error\": \"mqttExportNotEncrypted\"}", false);
                            }
                        }
                        JsonDocument json;
                        _importExport->exportNukiHubJson(json, redacted, pairing, _preferences->getBool(preference_lock_enabled, true), _preferences->getBool(preference_opener_enabled, false));
                        serializeJson(json, _buffer, _bufferSize);
                        publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_json, _buffer, false);

                        if (doc["exportNH"].as<int>() > 0)
                        {
                            _overwriteNukiHubConfigTS = espMillis() + (doc["exportNH"].as<int>() * 1000);
                        }
                    }
                    else
                    {
                        publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action_command_result, "{\"error\": \"mqttExportNotEnabled\"}", false);
                    }
                }
                else
                {
                    if(_preferences->getBool(preference_config_from_mqtt, false))
                    {
                        JsonDocument json;
                        json = _importExport->importJson(doc);
                        serializeJson(json, _buffer, _bufferSize);
                        publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_json, _buffer, false);
                        publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action, "--", true);
                        if (esp_task_wdt_status(NULL) == ESP_OK)
                        {
                            esp_task_wdt_reset();
                        }
                        vTaskDelay(200 / portTICK_PERIOD_MS);
                        restartEsp(RestartReason::ConfigurationUpdated);
                    }
                    else
                    {
                        publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action_command_result, "{\"error\": \"mqttImportNotEnabled\"}", false);
                    }
                }
                publishString(_maintenancePathPrefix, mqtt_topic_nuki_hub_config_action, "--", true);
            }
        }
    }
}

void NukiNetwork::parseGpioTopics(const espMqttClientTypes::MessageProperties &properties, const char *topic, const uint8_t *payload, size_t& len, size_t& index, size_t& total)
{
    char gpioPath[250];
    buildMqttPath(gpioPath, {_lockPath.c_str(), mqtt_topic_gpio_prefix, mqtt_topic_gpio_pin});

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
            Log->print("GPIO ");
            Log->print(pin);
            Log->print(" (Output) --> ");
            Log->println(pinState);
            digitalWrite(pin, pinState);
        }

    }
}

void NukiNetwork::gpioActionCallback(const GpioAction &action, const int &pin)
{
    _gpioTs[pin] = espMillis();
}

void NukiNetwork::disableAutoRestarts()
{
    _networkTimeout = 0;
    _restartOnDisconnect = false;
}

const int NukiNetwork::mqttConnectionState() const
{
    return _mqttConnectionState;
}

const bool NukiNetwork::mqttRecentlyConnected() const
{
    return _mqttConnectedTs != -1 && (millis() - _mqttConnectedTs < 6000);
}

const bool NukiNetwork::pathEquals(const char* prefix, const char* path, const char* referencePath)
{
    char prefixedPath[500];
    buildMqttPath(prefixedPath, { prefix, path });
    return strcmp(prefixedPath, referencePath) == 0;
}

void NukiNetwork::publishFloat(const char* prefix, const char* topic, const float value, bool retain, const uint8_t precision)
{
    char str[30];
    dtostrf(value, 0, precision, str);
    publish(prefix, topic, str, retain);
}

void NukiNetwork::publishInt(const char* prefix, const char *topic, const int value, bool retain)
{
    char str[30];
    itoa(value, str, 10);
    publish(prefix, topic, str, retain);
}

void NukiNetwork::publishUInt(const char* prefix, const char *topic, const unsigned int value, bool retain)
{
    char str[30];
    utoa(value, str, 10);
    publish(prefix, topic, str, retain);
}

void NukiNetwork::publishULong(const char* prefix, const char *topic, const unsigned long value, bool retain)
{
    char str[30];
    ultoa(value, str, 10);
    publish(prefix, topic, str, retain);
}

void NukiNetwork::publishLongLong(const char* prefix, const char *topic, int64_t value, bool retain)
{
    char str[30];
    lltoa(value, str, 10);
    publish(prefix, topic, str, retain);
}

void NukiNetwork::publishBool(const char* prefix, const char *topic, const bool value, bool retain)
{
    char str[2] = {0};
    str[0] = value ? '1' : '0';
    publish(prefix, topic, str, retain);
}

void NukiNetwork::publishString(const char* prefix, const char *topic, const char *value, bool retain)
{
    publish(prefix, topic, value, retain);
}

void NukiNetwork::publish(const char* prefix, const char *topic, const char *value, bool retain)
{
    char path[200] = {0};
    buildMqttPath(path, { prefix, topic });
    _device->mqttPublish(path, MQTT_QOS_LEVEL, retain, value);
}

void NukiNetwork::publish(const char* path, const char *value, bool retain)
{
    _device->mqttPublish(path, MQTT_QOS_LEVEL, retain, value);
}

void NukiNetwork::removeTopic(const String& mqttPath, const String& mqttTopic)
{
    String path = mqttPath;
    path.concat(mqttTopic);
    publish(path.c_str(), "", true);

#ifdef DEBUG_NUKIHUB
    Log->print("Removing MQTT topic: ");
    Log->println(path.c_str());
#endif
}

void NukiNetwork::setupHASS(int type, uint32_t nukiId, char* nukiName, const char* firmwareVersion, const char* hardwareVersion, bool hasDoorSensor, bool hasKeypad)
{
    _hadiscovery->setupHASS(type, nukiId, nukiName, firmwareVersion, hardwareVersion, hasDoorSensor, hasKeypad);
}

void NukiNetwork::disableHASS()
{
    _hadiscovery->disableHASS();
}

void NukiNetwork::publishHassTopic(const String& mqttDeviceType,
                                   const String& mqttDeviceName,
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
    _hadiscovery->publishHassTopic(mqttDeviceType, mqttDeviceName, uidString, uidStringPostfix, displayName, name, baseTopic, stateTopic, deviceType, deviceClass, stateClass, entityCat, commandTopic, additionalEntries);
}

void NukiNetwork::removeHassTopic(const String& mqttDeviceType, const String& mqttDeviceName, const String& uidString)
{
    _hadiscovery->removeHassTopic(mqttDeviceType, mqttDeviceName, uidString);
}

void NukiNetwork::batteryTypeToString(const Nuki::BatteryType battype, char* str)
{
    switch (battype)
    {
    case Nuki::BatteryType::Alkali:
        strcpy(str, "Alkali");
        break;
    case Nuki::BatteryType::Accumulators:
        strcpy(str, "Accumulators");
        break;
    case Nuki::BatteryType::Lithium:
        strcpy(str, "Lithium");
        break;
    default:
        strcpy(str, "undefined");
        break;
    }
}

void NukiNetwork::advertisingModeToString(const Nuki::AdvertisingMode advmode, char* str)
{
    switch (advmode)
    {
    case Nuki::AdvertisingMode::Automatic:
        strcpy(str, "Automatic");
        break;
    case Nuki::AdvertisingMode::Normal:
        strcpy(str, "Normal");
        break;
    case Nuki::AdvertisingMode::Slow:
        strcpy(str, "Slow");
        break;
    case Nuki::AdvertisingMode::Slowest:
        strcpy(str, "Slowest");
        break;
    default:
        strcpy(str, "undefined");
        break;
    }
}

void NukiNetwork::timeZoneIdToString(const Nuki::TimeZoneId timeZoneId, char* str)
{
    switch (timeZoneId)
    {
    case Nuki::TimeZoneId::Africa_Cairo:
        strcpy(str, "Africa/Cairo");
        break;
    case Nuki::TimeZoneId::Africa_Lagos:
        strcpy(str, "Africa/Lagos");
        break;
    case Nuki::TimeZoneId::Africa_Maputo:
        strcpy(str, "Africa/Maputo");
        break;
    case Nuki::TimeZoneId::Africa_Nairobi:
        strcpy(str, "Africa/Nairobi");
        break;
    case Nuki::TimeZoneId::America_Anchorage:
        strcpy(str, "America/Anchorage");
        break;
    case Nuki::TimeZoneId::America_Argentina_Buenos_Aires:
        strcpy(str, "America/Argentina/Buenos_Aires");
        break;
    case Nuki::TimeZoneId::America_Chicago:
        strcpy(str, "America/Chicago");
        break;
    case Nuki::TimeZoneId::America_Denver:
        strcpy(str, "America/Denver");
        break;
    case Nuki::TimeZoneId::America_Halifax:
        strcpy(str, "America/Halifax");
        break;
    case Nuki::TimeZoneId::America_Los_Angeles:
        strcpy(str, "America/Los_Angeles");
        break;
    case Nuki::TimeZoneId::America_Manaus:
        strcpy(str, "America/Manaus");
        break;
    case Nuki::TimeZoneId::America_Mexico_City:
        strcpy(str, "America/Mexico_City");
        break;
    case Nuki::TimeZoneId::America_New_York:
        strcpy(str, "America/New_York");
        break;
    case Nuki::TimeZoneId::America_Phoenix:
        strcpy(str, "America/Phoenix");
        break;
    case Nuki::TimeZoneId::America_Regina:
        strcpy(str, "America/Regina");
        break;
    case Nuki::TimeZoneId::America_Santiago:
        strcpy(str, "America/Santiago");
        break;
    case Nuki::TimeZoneId::America_Sao_Paulo:
        strcpy(str, "America/Sao_Paulo");
        break;
    case Nuki::TimeZoneId::America_St_Johns:
        strcpy(str, "America/St_Johns");
        break;
    case Nuki::TimeZoneId::Asia_Bangkok:
        strcpy(str, "Asia/Bangkok");
        break;
    case Nuki::TimeZoneId::Asia_Dubai:
        strcpy(str, "Asia/Dubai");
        break;
    case Nuki::TimeZoneId::Asia_Hong_Kong:
        strcpy(str, "Asia/Hong_Kong");
        break;
    case Nuki::TimeZoneId::Asia_Jerusalem:
        strcpy(str, "Asia/Jerusalem");
        break;
    case Nuki::TimeZoneId::Asia_Karachi:
        strcpy(str, "Asia/Karachi");
        break;
    case Nuki::TimeZoneId::Asia_Kathmandu:
        strcpy(str, "Asia/Kathmandu");
        break;
    case Nuki::TimeZoneId::Asia_Kolkata:
        strcpy(str, "Asia/Kolkata");
        break;
    case Nuki::TimeZoneId::Asia_Riyadh:
        strcpy(str, "Asia/Riyadh");
        break;
    case Nuki::TimeZoneId::Asia_Seoul:
        strcpy(str, "Asia/Seoul");
        break;
    case Nuki::TimeZoneId::Asia_Shanghai:
        strcpy(str, "Asia/Shanghai");
        break;
    case Nuki::TimeZoneId::Asia_Tehran:
        strcpy(str, "Asia/Tehran");
        break;
    case Nuki::TimeZoneId::Asia_Tokyo:
        strcpy(str, "Asia/Tokyo");
        break;
    case Nuki::TimeZoneId::Asia_Yangon:
        strcpy(str, "Asia/Yangon");
        break;
    case Nuki::TimeZoneId::Australia_Adelaide:
        strcpy(str, "Australia/Adelaide");
        break;
    case Nuki::TimeZoneId::Australia_Brisbane:
        strcpy(str, "Australia/Brisbane");
        break;
    case Nuki::TimeZoneId::Australia_Darwin:
        strcpy(str, "Australia/Darwin");
        break;
    case Nuki::TimeZoneId::Australia_Hobart:
        strcpy(str, "Australia/Hobart");
        break;
    case Nuki::TimeZoneId::Australia_Perth:
        strcpy(str, "Australia/Perth");
        break;
    case Nuki::TimeZoneId::Australia_Sydney:
        strcpy(str, "Australia/Sydney");
        break;
    case Nuki::TimeZoneId::Europe_Berlin:
        strcpy(str, "Europe/Berlin");
        break;
    case Nuki::TimeZoneId::Europe_Helsinki:
        strcpy(str, "Europe/Helsinki");
        break;
    case Nuki::TimeZoneId::Europe_Istanbul:
        strcpy(str, "Europe/Istanbul");
        break;
    case Nuki::TimeZoneId::Europe_London:
        strcpy(str, "Europe/London");
        break;
    case Nuki::TimeZoneId::Europe_Moscow:
        strcpy(str, "Europe/Moscow");
        break;
    case Nuki::TimeZoneId::Pacific_Auckland:
        strcpy(str, "Pacific/Auckland");
        break;
    case Nuki::TimeZoneId::Pacific_Guam:
        strcpy(str, "Pacific/Guam");
        break;
    case Nuki::TimeZoneId::Pacific_Honolulu:
        strcpy(str, "Pacific/Honolulu");
        break;
    case Nuki::TimeZoneId::Pacific_Pago_Pago:
        strcpy(str, "Pacific/Pago_Pago");
        break;
    case Nuki::TimeZoneId::None:
        strcpy(str, "None");
        break;
    default:
        strcpy(str, "undefined");
        break;
    }
}

const uint16_t NukiNetwork::subscribe(const char *topic, uint8_t qos)
{
    Log->print("Subscribing to MQTT topic: ");
    Log->println(topic);
    return _device->mqttSubscribe(topic, qos);
}

const bool NukiNetwork::comparePrefixedPath(const char *fullPath, const char *subPath)
{
    char prefixedPath[500];
    buildMqttPath(subPath, prefixedPath);

    return strcmp(fullPath, prefixedPath) == 0;
}

void NukiNetwork::addReconnectedCallback(std::function<void()> reconnectedCallback)
{
    _reconnectedCallbacks.push_back(reconnectedCallback);
}

void NukiNetwork::disableMqtt()
{
    _device->mqttDisable();
    _mqttEnabled = false;
}
#endif
