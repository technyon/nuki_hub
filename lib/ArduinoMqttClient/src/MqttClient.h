/*
  This file is part of the ArduinoMqttClient library.
  Copyright (c) 2019 Arduino SA. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _MQTT_CLIENT_H_
#define _MQTT_CLIENT_H_

#include <Arduino.h>
#include <Client.h>

#define MQTT_CONNECTION_REFUSED            -2
#define MQTT_CONNECTION_TIMEOUT            -1
#define MQTT_SUCCESS                        0
#define MQTT_UNACCEPTABLE_PROTOCOL_VERSION  1
#define MQTT_IDENTIFIER_REJECTED            2
#define MQTT_SERVER_UNAVAILABLE             3
#define MQTT_BAD_USER_NAME_OR_PASSWORD      4
#define MQTT_NOT_AUTHORIZED                 5

// Make this definition in your application code to use std::functions for onMessage callbacks instead of C-pointers:
// #define MQTT_CLIENT_STD_FUNCTION_CALLBACK

#ifdef MQTT_CLIENT_STD_FUNCTION_CALLBACK
#include <functional>
#endif

class MqttClient : public Client {
public:
  MqttClient(Client* client);
  MqttClient(Client& client);
  virtual ~MqttClient();

#ifdef MQTT_CLIENT_STD_FUNCTION_CALLBACK
  typedef std::function<void(MqttClient *client, int messageSize)> MessageCallback;
  void onMessage(MessageCallback callback);
#else
  inline void setClient(Client& client) { _client = &client; }
  void onMessage(void(*)(int));
#endif

  int parseMessage();
  String messageTopic() const;
  int messageDup() const;
  int messageQoS() const;
  int messageRetain() const;

  int beginMessage(const char* topic, unsigned long size, bool retain = false, uint8_t qos = 0, bool dup = false);
  int beginMessage(const String& topic, unsigned long size, bool retain = false, uint8_t qos = 0, bool dup = false);
  int beginMessage(const char* topic, bool retain = false, uint8_t qos = 0, bool dup = false);
  int beginMessage(const String& topic, bool retain = false, uint8_t qos = 0, bool dup = false);
  int endMessage();

  int beginWill(const char* topic, unsigned short size, bool retain, uint8_t qos);
  int beginWill(const String& topic, unsigned short size, bool retain, uint8_t qos);
  int beginWill(const char* topic, bool retain, uint8_t qos);
  int beginWill(const String& topic, bool retain, uint8_t qos);
  int endWill();

  int subscribe(const char* topic, uint8_t qos = 0);
  int subscribe(const String& topic, uint8_t qos = 0);
  int unsubscribe(const char* topic);
  int unsubscribe(const String& topic);

  void poll();

  // from Client
  virtual int connect(IPAddress ip, uint16_t port = 1883);
  virtual int connect(const char *host, uint16_t port = 1883);
#ifdef ESP8266
  virtual int connect(const IPAddress& ip, uint16_t port) { return 0; }; /* ESP8266 core defines this pure virtual in Client.h */
#endif
  virtual size_t write(uint8_t);
  virtual size_t write(const uint8_t *buf, size_t size);
  virtual int available();
  virtual int read();
  virtual int read(uint8_t *buf, size_t size);
  virtual int peek();
  virtual void flush();
  virtual void stop();
  virtual uint8_t connected();
  virtual operator bool();

  void setId(const char* id);
  void setId(const String& id);

  void setUsernamePassword(const char* username, const char* password);
  void setUsernamePassword(const String& username, const String& password);

  void setCleanSession(bool cleanSession);

  void setKeepAliveInterval(unsigned long interval);
  void setConnectionTimeout(unsigned long timeout);
  void setTxPayloadSize(unsigned short size);

  int connectError() const;
  int subscribeQoS() const;
#ifdef ESP8266
  virtual bool flush(unsigned int /*maxWaitMs*/) { flush(); return true; } /* ESP8266 core defines this pure virtual in Client.h */
  virtual bool stop(unsigned int /*maxWaitMs*/)  { stop(); return true; } /* ESP8266 core defines this pure virtual in Client.h */
#endif

private:
  int connect(IPAddress ip, const char* host, uint16_t port);
  int publishHeader(size_t length);
  void puback(uint16_t id);
  void pubrec(uint16_t id);
  void pubrel(uint16_t id);
  void pubcomp(uint16_t id);
  void ping();
  void disconnect();

  int beginPacket(uint8_t type, uint8_t flags, size_t length, uint8_t* buffer);
  int writeString(const char* s, uint16_t length);
  int write8(uint8_t val);
  int write16(uint16_t val);
  int writeData(const void* data, size_t length);
  int endPacket();

  void ackRxMessage();

  uint8_t clientConnected();
  int clientAvailable();
  int clientRead();
  int clientTimedRead();
  int clientPeek();
  size_t clientWrite(const uint8_t *buf, size_t size);

private:
  Client* _client;

#ifdef MQTT_CLIENT_STD_FUNCTION_CALLBACK
  MessageCallback _onMessage;
#else
  void (*_onMessage)(int);
#endif

  String _id;
  String _username;
  String _password;
  bool _cleanSession;

  unsigned long _keepAliveInterval;
  unsigned long _connectionTimeout;
  unsigned short _tx_payload_buffer_size;

  int _connectError;
  bool _connected;
  int _subscribeQos;

  int _rxState;
  uint8_t _rxType;
  uint8_t _rxFlags;
  size_t _rxLength;
  uint32_t _rxLengthMultiplier;
  int _returnCode;

  String _rxMessageTopic;
  size_t _rxMessageTopicLength;
  bool _rxMessageDup;
  uint8_t _rxMessageQoS;
  bool _rxMessageRetain;
  uint16_t _rxPacketId;
  uint8_t _rxMessageBuffer[3];
  size_t _rxMessageIndex;
  unsigned long _lastRx;

  String _txMessageTopic;
  bool _txMessageRetain;
  uint8_t _txMessageQoS;
  bool _txMessageDup;
  uint16_t _txPacketId;
  uint8_t* _txBuffer;
  size_t _txBufferIndex;
  bool _txStreamPayload;
  uint8_t* _txPayloadBuffer;
  size_t _txPayloadBufferIndex;
  unsigned long _lastPingTx;

  uint8_t* _willBuffer;
  uint16_t _willBufferIndex;
  size_t _willMessageIndex;
  uint8_t _willFlags;
};

#endif
