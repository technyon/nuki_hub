#include <Arduino.h>
#include "NetworkDevice.h"
#include "../Logger.h"

#ifndef NUKI_HUB_UPDATER
#include "FS.h"
#include "SPIFFS.h"
#include "../MqttTopics.h"
#include "PreferencesKeys.h"
#ifdef CONFIG_SOC_SPIRAM_SUPPORTED
#include "esp_psram.h"
#endif

void NetworkDevice::init()
{
    #ifdef CONFIG_SOC_SPIRAM_SUPPORTED
    if(esp_psram_get_size() > 0)
    {
        //_mqttInternal = true;
        _mqttInternal = false;
    }
    #endif
    
    if(_preferences->getBool(preference_mqtt_ssl_enabled, false)) {
        if (!SPIFFS.begin(true)) {
            Log->println("SPIFFS Mount Failed");
        }
        else
        {
            File file = SPIFFS.open("/mqtt_ssl.ca");
            if (!file || file.isDirectory()) {
                Log->println("mqtt_ssl.ca not found");
            }
            else
            {
                Log->println("Reading mqtt_ssl.ca");
                String ca_cert = file.readString();
                file.close();
                char* caDest;
                caDest = (char *)malloc(sizeof(char) * (ca_cert.length()+1));
                strcpy(caDest, ca_cert.c_str());

                if(ca_cert.length() > 1)
                {
                    _useEncryption = true;
                    Log->println("MQTT over TLS.");
                    if(_mqttInternal)
                    {
                        _mqttClientSecure = new espMqttClientSecure(espMqttClientTypes::UseInternalTask::YES);
                    }
                    else
                    {
                        _mqttClientSecure = new espMqttClientSecure(espMqttClientTypes::UseInternalTask::NO);
                    }
                    _mqttClientSecure->setCACert(caDest);

                    File file2 = SPIFFS.open("/mqtt_ssl.crt");
                    File file3 = SPIFFS.open("/mqtt_ssl.key");
                    if (!file2 || file2.isDirectory() || !file3 || file3.isDirectory()) {
                        Log->println("mqtt_ssl.crt or mqtt_ssl.key not found");
                    }
                    else
                    {
                        String cert = file2.readString();
                        file2.close();
                        char* certDest;
                        certDest = (char *)malloc(sizeof(char) * (cert.length()+1));
                        strcpy(certDest, cert.c_str());

                        String key = file3.readString();
                        file3.close();
                        char* keyDest;
                        keyDest = (char *)malloc(sizeof(char) * (key.length()+1));
                        strcpy(keyDest, key.c_str());

                        if(cert.length() > 1 && key.length() > 1)
                        {
                            Log->println("MQTT with client certificate.");
                            _mqttClientSecure->setCertificate(certDest);
                            _mqttClientSecure->setPrivateKey(keyDest);
                        }
                    }
                }
            }
        }
    }

    if (!_useEncryption)
    {
        Log->println("MQTT without TLS.");
        if(_mqttInternal)
        {
            _mqttClient = new espMqttClient(espMqttClientTypes::UseInternalTask::YES);
        }
        else
        {
            _mqttClient = new espMqttClient(espMqttClientTypes::UseInternalTask::NO);
        }
    }

    if(_preferences->getBool(preference_mqtt_log_enabled, false) || _preferences->getBool(preference_webserial_enabled, false))
    {
        MqttLoggerMode mode;

        if(_preferences->getBool(preference_mqtt_log_enabled, false) && _preferences->getBool(preference_webserial_enabled, false))
        {
            mode = MqttLoggerMode::MqttAndSerialAndWeb;
        }
        else if (_preferences->getBool(preference_webserial_enabled, false))
        {
            mode = MqttLoggerMode::SerialAndWeb;
        }
        else
        {
            mode = MqttLoggerMode::MqttAndSerial;
        }

        _path = new char[200];
        memset(_path, 0, sizeof(_path));

        String pathStr = _preferences->getString(preference_mqtt_lock_path);
        pathStr.concat(mqtt_topic_log);
        strcpy(_path, pathStr.c_str());
        Log = new MqttLogger(*getMqttClient(), _path, mode);
    }
}
void NetworkDevice::update()
{
    if (_mqttEnabled && !_mqttInternal)
    {
        getMqttClient()->loop();
    }
}

void NetworkDevice::mqttSetClientId(const char *clientId)
{
    if (_useEncryption)
    {
        _mqttClientSecure->setClientId(clientId);
    }
    else
    {
        _mqttClient->setClientId(clientId);
    }
}

void NetworkDevice::mqttSetCleanSession(bool cleanSession)
{
    if (_useEncryption)
    {
        _mqttClientSecure->setCleanSession(cleanSession);
    }
    else
    {
        _mqttClient->setCleanSession(cleanSession);
    }
}

void NetworkDevice::mqttSetKeepAlive(uint16_t keepAlive)
{
    if (_useEncryption)
    {
        _mqttClientSecure->setKeepAlive(keepAlive);
    }
    else
    {
        _mqttClient->setKeepAlive(keepAlive);
    }
}

uint16_t NetworkDevice::mqttPublish(const char *topic, uint8_t qos, bool retain, const char *payload)
{
    return getMqttClient()->publish(topic, qos, retain, payload);
}

uint16_t NetworkDevice::mqttPublish(const char *topic, uint8_t qos, bool retain, const uint8_t *payload, size_t length)
{
    return getMqttClient()->publish(topic, qos, retain, payload, length);
}

bool NetworkDevice::mqttConnected() const
{
    return getMqttClient()->connected();
}

void NetworkDevice::mqttSetServer(const char *host, uint16_t port)
{
    if (_useEncryption)
    {
        _mqttClientSecure->setServer(host, port);
    }
    else
    {
        _mqttClient->setServer(host, port);
    }
}

bool NetworkDevice::mqttConnect()
{
    return getMqttClient()->connect();
}

bool NetworkDevice::mqttDisconnect(bool force)
{
    return getMqttClient()->disconnect(force);
}

void NetworkDevice::mqttSetWill(const char *topic, uint8_t qos, bool retain, const char *payload)
{
    if (_useEncryption)
    {
        _mqttClientSecure->setWill(topic, qos, retain, payload);
    }
    else
    {
        _mqttClient->setWill(topic, qos, retain, payload);
    }
}

void NetworkDevice::mqttSetCredentials(const char *username, const char *password)
{
    if (_useEncryption)
    {
        _mqttClientSecure->setCredentials(username, password);
    }
    else
    {
        _mqttClient->setCredentials(username, password);
    }
}

void NetworkDevice::mqttOnMessage(espMqttClientTypes::OnMessageCallback callback)
{
    if (_useEncryption)
    {
        _mqttClientSecure->onMessage(callback);
    }
    else
    {
        _mqttClient->onMessage(callback);
    }
}

void NetworkDevice::mqttOnConnect(espMqttClientTypes::OnConnectCallback callback)
{
    if(_useEncryption)
    {
        _mqttClientSecure->onConnect(callback);
    }
    else
    {
        _mqttClient->onConnect(callback);
    }
}

void NetworkDevice::mqttOnDisconnect(espMqttClientTypes::OnDisconnectCallback callback)
{
    if (_useEncryption)
    {
        _mqttClientSecure->onDisconnect(callback);
    }
    else
    {
        _mqttClient->onDisconnect(callback);
    }
}

uint16_t NetworkDevice::mqttSubscribe(const char *topic, uint8_t qos)
{
    return getMqttClient()->subscribe(topic, qos);
}

void NetworkDevice::mqttDisable()
{
    getMqttClient()->disconnect();
    _mqttEnabled = false;
}

bool NetworkDevice::isEncrypted()
{
    return _useEncryption;
}

MqttClient *NetworkDevice::getMqttClient() const
{
    if (_useEncryption)
    {
        return _mqttClientSecure;
    }
    else
    {
        return _mqttClient;
    }
}
#else
void NetworkDevice::update()
{
}
#endif