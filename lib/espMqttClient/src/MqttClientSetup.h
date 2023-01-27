/*
Copyright (c) 2022 Bert Melis. All rights reserved.

API is based on the original work of Marvin Roger:
https://github.com/marvinroger/async-mqtt-client

This work is licensed under the terms of the MIT license.  
For a copy, see <https://opensource.org/licenses/MIT> or
the LICENSE file.
*/

#pragma once

#include "MqttClient.h"

class MqttClientSetup : public MqttClient {
public:
    void setKeepAlive(uint16_t keepAlive) {
        _keepAlive = keepAlive * 1000;  // s to ms conversion, will also do 16 to 32 bit conversion

    }

    void setClientId(const char* clientId) {
        _clientId = clientId;

    }

    void setCleanSession(bool cleanSession) {
        _cleanSession = cleanSession;

    }

    void setCredentials(const char* username, const char* password) {
        _username = username;
        _password = password;

    }

    void setWill(const char* topic, uint8_t qos, bool retain, const uint8_t* payload, size_t length) {
        _willTopic = topic;
        _willQos = qos;
        _willRetain = retain;
        _willPayload = payload;
        if (!_willPayload) {
            _willPayloadLength = 0;
        } else {
            _willPayloadLength = length;
        }

    }

    void setWill(const char* topic, uint8_t qos, bool retain, const char* payload) {
        return setWill(topic, qos, retain, reinterpret_cast<const uint8_t*>(payload), strlen(payload));
    }

    void setServer(IPAddress ip, uint16_t port) {
        _ip = ip;
        _port = port;
        _useIp = true;

    }

    void setServer(const char* host, uint16_t port) {
        _host = host;
        _port = port;
        _useIp = false;

    }

    void onConnect(espMqttClientTypes::OnConnectCallback callback) {
        _onConnectCallback = callback;

    }

    void onDisconnect(espMqttClientTypes::OnDisconnectCallback callback) {
        _onDisconnectCallback = callback;

    }

    void onSubscribe(espMqttClientTypes::OnSubscribeCallback callback) {
        _onSubscribeCallback = callback;

    }

    void onUnsubscribe(espMqttClientTypes::OnUnsubscribeCallback callback) {
        _onUnsubscribeCallback = callback;

    }

    void onMessage(espMqttClientTypes::OnMessageCallback callback) {
        _onMessageCallback = callback;

    }

    void onPublish(espMqttClientTypes::OnPublishCallback callback) {
        _onPublishCallback = callback;

    }

    /*
    void onError(espMqttClientTypes::OnErrorCallback callback) {
      _onErrorCallback = callback;

    }
    */

protected:
#if defined(ESP32)
    explicit MqttClientSetup(bool useTask, uint8_t priority = 1, uint8_t core = 1)
            : MqttClient(useTask, priority, core) {}
#else
    MqttClientSetup()
  : MqttClient() {}
#endif
};
