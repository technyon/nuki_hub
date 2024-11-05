#include <Arduino.h>
#include "NetworkDevice.h"
#include "../Logger.h"

#ifndef NUKI_HUB_UPDATER
#include "../MqttTopics.h"
#include "espMqttClient.h"

void NetworkDevice::init()
{
    size_t caLength = _preferences->getString(preference_mqtt_ca, _ca, TLS_CA_MAX_SIZE);
    size_t crtLength = _preferences->getString(preference_mqtt_crt, _cert, TLS_CERT_MAX_SIZE);
    size_t keyLength = _preferences->getString(preference_mqtt_key, _key, TLS_KEY_MAX_SIZE);

    _useEncryption = caLength > 1;  // length is 1 when empty

    if(_useEncryption)
    {
        Log->println(F("MQTT over TLS."));
        _mqttClientSecure = new espMqttClientSecure(espMqttClientTypes::UseInternalTask::NO);
        _mqttClientSecure->setCACert(_ca);
        if(crtLength > 1 && keyLength > 1) // length is 1 when empty
        {
            Log->println(F("MQTT with client certificate."));
            _mqttClientSecure->setCertificate(_cert);
            _mqttClientSecure->setPrivateKey(_key);
        }
    } else
    {
        Log->println(F("MQTT without TLS."));
        _mqttClient = new espMqttClient(espMqttClientTypes::UseInternalTask::NO);
    }

    if(_preferences->getBool(preference_mqtt_log_enabled, false) || _preferences->getBool(preference_webserial_enabled, false))
    {
        MqttLoggerMode mode;

        if(_preferences->getBool(preference_mqtt_log_enabled, false) && _preferences->getBool(preference_webserial_enabled, false)) mode = MqttLoggerMode::MqttAndSerialAndWeb;
        else if (_preferences->getBool(preference_webserial_enabled, false)) mode = MqttLoggerMode::SerialAndWeb;
        else mode = MqttLoggerMode::MqttAndSerial;

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
    if (_mqttEnabled)
    {
        getMqttClient()->loop();
    }
}

void NetworkDevice::mqttSetClientId(const char *clientId)
{
    getMqttClient()->setClientId(clientId);
}

void NetworkDevice::mqttSetCleanSession(bool cleanSession)
{
    getMqttClient()->setCleanSession(cleanSession);
}

void NetworkDevice::mqttSetKeepAlive(uint16_t keepAlive)
{
    getMqttClient()->setKeepAlive(keepAlive);
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
    getMqttClient()->setServer(host, port);
}

bool NetworkDevice::mqttConnect()
{
    return getMqttClient()->connect();
}

bool NetworkDevice::mqttDisconnect(bool force)
{
    return getMqttClient()->disconnect(force);
}

void NetworkDevice::setWill(const char *topic, uint8_t qos, bool retain, const char *payload)
{
    getMqttClient()->setWill(topic, qos, retain, payload);
}

void NetworkDevice::mqttSetCredentials(const char *username, const char *password)
{
    getMqttClient()->setCredentials(username, password);
}

void NetworkDevice::mqttOnMessage(espMqttClientTypes::OnMessageCallback callback)
{
    getMqttClient()->onMessage(callback);
}

void NetworkDevice::mqttOnConnect(espMqttClientTypes::OnConnectCallback callback)
{
    getMqttClient()->onConnect(callback);
}

void NetworkDevice::mqttOnDisconnect(espMqttClientTypes::OnDisconnectCallback callback)
{
    getMqttClient()->onDisconnect(callback);
}

uint16_t NetworkDevice::mqttSubscribe(const char *topic, uint8_t qos)
{
    return getMqttClient()->subscribe(topic, qos);
}

void NetworkDevice::disableMqtt()
{
    getMqttClient()->disconnect();
    _mqttEnabled = false;
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