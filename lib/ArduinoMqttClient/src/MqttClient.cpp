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

#include "MqttClient.h"

// #define MQTT_CLIENT_DEBUG

#ifndef htons
  #ifdef __ARM__
    #define htons __REV16
  #else
    #define htons(s) ((s<<8) | (s>>8))
  #endif
#endif

#ifndef TX_PAYLOAD_BUFFER_SIZE
  #ifdef __AVR__
    #define TX_PAYLOAD_BUFFER_SIZE 128
  #else
    #define TX_PAYLOAD_BUFFER_SIZE 256
  #endif
#endif

#define MQTT_CONNECT      1
#define MQTT_CONNACK      2
#define MQTT_PUBLISH      3
#define MQTT_PUBACK       4
#define MQTT_PUBREC       5
#define MQTT_PUBREL       6
#define MQTT_PUBCOMP      7
#define MQTT_SUBSCRIBE    8
#define MQTT_SUBACK       9
#define MQTT_UNSUBSCRIBE 10
#define MQTT_UNSUBACK    11
#define MQTT_PINGREQ     12
#define MQTT_PINGRESP    13
#define MQTT_DISCONNECT  14

enum {
  MQTT_CLIENT_RX_STATE_READ_TYPE,
  MQTT_CLIENT_RX_STATE_READ_REMAINING_LENGTH,
  MQTT_CLIENT_RX_STATE_READ_VARIABLE_HEADER,
  MQTT_CLIENT_RX_STATE_READ_PUBLISH_TOPIC_LENGTH,
  MQTT_CLIENT_RX_STATE_READ_PUBLISH_TOPIC,
  MQTT_CLIENT_RX_STATE_READ_PUBLISH_PACKET_ID,
  MQTT_CLIENT_RX_STATE_READ_PUBLISH_PAYLOAD,
  MQTT_CLIENT_RX_STATE_DISCARD_PUBLISH_PAYLOAD
};

MqttClient::MqttClient(Client* client) :
  _client(client),
  _onMessage(NULL),
  _cleanSession(true),
  _keepAliveInterval(60 * 1000L),
  _connectionTimeout(30 * 1000L),
  _tx_payload_buffer_size(TX_PAYLOAD_BUFFER_SIZE),
  _connectError(MQTT_SUCCESS),
  _connected(false),
  _subscribeQos(0x00),
  _rxState(MQTT_CLIENT_RX_STATE_READ_TYPE),
  _txBufferIndex(0),
  _txPayloadBuffer(NULL),
  _txPayloadBufferIndex(0),
  _willBuffer(NULL),
  _willBufferIndex(0),
  _willMessageIndex(0),
  _willFlags(0x00)
{
  setTimeout(0);
}

MqttClient::MqttClient(Client& client) : MqttClient(&client)
{

}

MqttClient::~MqttClient()
{
  if (_willBuffer) {
    free(_willBuffer);

    _willBuffer = NULL;
  }

  if (_txPayloadBuffer) {
    free(_txPayloadBuffer);

    _txPayloadBuffer = NULL;
  }
}

#ifdef MQTT_CLIENT_STD_FUNCTION_CALLBACK
void MqttClient::onMessage(MessageCallback callback)
#else
void MqttClient::onMessage(void(*callback)(int))
#endif
{
  _onMessage = callback;
}

int MqttClient::parseMessage()
{
  if (_rxState == MQTT_CLIENT_RX_STATE_READ_PUBLISH_PAYLOAD) {
    // already had a message, but only partially read, discard the data
    _rxState = MQTT_CLIENT_RX_STATE_DISCARD_PUBLISH_PAYLOAD;
  }

  poll();

  if (_rxState != MQTT_CLIENT_RX_STATE_READ_PUBLISH_PAYLOAD) {
    // message not received or not ready
    return 0;
  }

  return _rxLength;
}

String MqttClient::messageTopic() const
{
  if (_rxState == MQTT_CLIENT_RX_STATE_READ_PUBLISH_PAYLOAD) {
    // message received and ready for reading
    return _rxMessageTopic;
  }

  return "";
}

int MqttClient::messageDup() const
{
  if (_rxState == MQTT_CLIENT_RX_STATE_READ_PUBLISH_PAYLOAD) {
    // message received and ready for reading
    return _rxMessageDup;
  }

  return -1;
}

int MqttClient::messageQoS() const
{
  if (_rxState == MQTT_CLIENT_RX_STATE_READ_PUBLISH_PAYLOAD) {
    // message received and ready for reading
    return _rxMessageQoS;
  }

  return -1;
}

int MqttClient::messageRetain() const
{
  if (_rxState == MQTT_CLIENT_RX_STATE_READ_PUBLISH_PAYLOAD) {
    // message received and ready for reading
    return _rxMessageRetain;
  }

  return -1;
}

int MqttClient::beginMessage(const char* topic, unsigned long size, bool retain, uint8_t qos, bool dup)
{
  _txMessageTopic = topic;
  _txMessageRetain = retain;
  _txMessageQoS = qos;
  _txMessageDup = dup;
  _txPayloadBufferIndex = 0;
  _txStreamPayload = (size != 0xffffffffL);

  if (_txStreamPayload) {
    if (!publishHeader(size)) {
      stop();

      return 0;
    }
  }

  return 1;
}

int MqttClient::beginMessage(const String& topic, unsigned long size, bool retain, uint8_t qos, bool dup)
{
  return beginMessage(topic.c_str(), size, retain, qos, dup);
}

int MqttClient::beginMessage(const char* topic, bool retain, uint8_t qos, bool dup)
{
  return beginMessage(topic, 0xffffffffL, retain, qos, dup);
}

int MqttClient::beginMessage(const String& topic, bool retain, uint8_t qos, bool dup)
{
  return beginMessage(topic.c_str(), retain, qos, dup);
}

int MqttClient::endMessage()
{
  if (!_txStreamPayload) {
    if (!publishHeader(_txPayloadBufferIndex) ||
        (clientWrite(_txPayloadBuffer, _txPayloadBufferIndex) != _txPayloadBufferIndex)) {
      stop();

      return 0;
    }
  }

  _txStreamPayload = false;

  if (_txMessageQoS) {
    if (_txMessageQoS == 2) {
      // wait for PUBREC
      _returnCode = -1;

      for (unsigned long start = millis(); ((millis() - start) < _connectionTimeout) && clientConnected();) {
        poll();

        if (_returnCode != -1) {
          if (_returnCode == 0) {
            break;
          } else {
            return 0;
          }
        }
        yield();
      }

      // reply with PUBREL
      pubrel(_txPacketId);
    }

    // wait for PUBACK or PUBCOMP
    _returnCode = -1;

    for (unsigned long start = millis(); ((millis() - start) < _connectionTimeout) && clientConnected();) {
      poll();

      if (_returnCode != -1) {
        return (_returnCode == 0);
      }
      yield();
    }

    return 0;
  }

  return 1;
}

int MqttClient::beginWill(const char* topic, unsigned short size, bool retain, uint8_t qos)
{
  int topicLength = strlen(topic);
  size_t willLength = (2 + topicLength + 2 + size);

  if (qos > 2) {
    // invalid QoS
  }

  _willBuffer = (uint8_t*)realloc(_willBuffer, willLength);

  _txBuffer = _willBuffer;
  _txBufferIndex = 0;
  writeString(topic, topicLength);
  write16(0); // dummy size for now
  _willMessageIndex = _txBufferIndex;

  _willFlags = (qos << 3) | 0x04;
  if (retain) {
    _willFlags |= 0x20;
  }

  return 0;
}

int MqttClient::beginWill(const String& topic, unsigned short size, bool retain, uint8_t qos)
{
  return beginWill(topic.c_str(), size, retain, qos);
}

int MqttClient::beginWill(const char* topic, bool retain, uint8_t qos)
{
  return beginWill(topic, _tx_payload_buffer_size, retain, qos);
}

int MqttClient::beginWill(const String& topic, bool retain, uint8_t qos)
{
  return beginWill(topic.c_str(), retain, qos);
}

int MqttClient::endWill()
{
  // update the index
  _willBufferIndex = _txBufferIndex;

  // update the will message size
  _txBufferIndex = (_willMessageIndex - 2);
  write16(_willBufferIndex - _willMessageIndex);

  _txBuffer = NULL;
  _willMessageIndex = 0;

  return 1;
}

int MqttClient::subscribe(const char* topic, uint8_t qos)
{
  int topicLength = strlen(topic);
  int remainingLength = topicLength + 5;

  if (qos > 2) {
    // invalid QoS
    return 0;
  }

  _txPacketId++;

  if (_txPacketId == 0) {
    _txPacketId = 1;
  }

  uint8_t packetBuffer[5 + remainingLength];

  beginPacket(MQTT_SUBSCRIBE, 0x02, remainingLength, packetBuffer);
  write16(_txPacketId);
  writeString(topic, topicLength);
  write8(qos);

  if (!endPacket()) {
    stop();

    return 0;
  }

  _returnCode = -1;
  _subscribeQos = 0x80;

  for (unsigned long start = millis(); ((millis() - start) < _connectionTimeout) && clientConnected();) {
    poll();

    if (_returnCode != -1) {
      _subscribeQos = _returnCode;

      return (_returnCode >= 0 && _returnCode <= 2);
    }
    yield();
  }

  stop();

  return 0;
}

int MqttClient::subscribe(const String& topic, uint8_t qos)
{
  return subscribe(topic.c_str(), qos);
}

int MqttClient::unsubscribe(const char* topic)
{
  int topicLength = strlen(topic);
  int remainingLength = topicLength + 4;

  _txPacketId++;

  if (_txPacketId == 0) {
    _txPacketId = 1;
  }

  uint8_t packetBuffer[5 + remainingLength];

  beginPacket(MQTT_UNSUBSCRIBE, 0x02, remainingLength, packetBuffer);
  write16(_txPacketId);
  writeString(topic, topicLength);

  if (!endPacket()) {
    stop();

    return 0;
  }

  _returnCode = -1;

  for (unsigned long start = millis(); ((millis() - start) < _connectionTimeout) && clientConnected();) {
    poll();

    if (_returnCode != -1) {
      return (_returnCode == 0);
    }
    yield();
  }

  stop();

  return 0;
}

int MqttClient::unsubscribe(const String& topic)
{
  return unsubscribe(topic.c_str());
}

void MqttClient::poll()
{
  if (clientAvailable() == 0 && !clientConnected()) {
    _rxState = MQTT_CLIENT_RX_STATE_READ_TYPE;
    _connected = false;
  }

  while (clientAvailable()) {
    byte b = clientRead();
    _lastRx = millis();

    switch (_rxState) {
      case MQTT_CLIENT_RX_STATE_READ_TYPE: {
        _rxType = (b >> 4);
        _rxFlags = (b & 0x0f);
        _rxLength = 0;
        _rxLengthMultiplier = 1;

        _rxState = MQTT_CLIENT_RX_STATE_READ_REMAINING_LENGTH;
        break;
      }

      case MQTT_CLIENT_RX_STATE_READ_REMAINING_LENGTH: {
        _rxLength += (b & 0x7f) * _rxLengthMultiplier;

        _rxLengthMultiplier *= 128;

        if (_rxLengthMultiplier > (128 * 128 * 128L)) {
          // malformed
          stop();

          return;
        }

        if ((b & 0x80) == 0) { // length done
          bool malformedResponse = false;

          if (_rxType == MQTT_CONNACK || 
              _rxType == MQTT_PUBACK  ||
              _rxType == MQTT_PUBREC  || 
              _rxType == MQTT_PUBCOMP ||
              _rxType == MQTT_UNSUBACK) {
            malformedResponse = (_rxFlags != 0x00 || _rxLength != 2);
          } else if (_rxType == MQTT_PUBLISH) {
            malformedResponse = ((_rxFlags & 0x06) == 0x06);
          } else if (_rxType == MQTT_PUBREL) {
            malformedResponse = (_rxFlags != 0x02 || _rxLength != 2);
          } else if (_rxType == MQTT_SUBACK) { 
            malformedResponse = (_rxFlags != 0x00 || _rxLength != 3);
          } else if (_rxType == MQTT_PINGRESP) {
            malformedResponse = (_rxFlags != 0x00 || _rxLength != 0);
          } else {
            // unexpected type
            malformedResponse = true;
          }

          if (malformedResponse) {
            stop();
            return;
          }

          if (_rxType == MQTT_PUBLISH) {
            _rxMessageDup = (_rxFlags & 0x80) != 0;
            _rxMessageQoS = (_rxFlags >> 1) & 0x03;
            _rxMessageRetain = (_rxFlags & 0x01);

            _rxState = MQTT_CLIENT_RX_STATE_READ_PUBLISH_TOPIC_LENGTH;
          } else if (_rxLength == 0) {
            _rxState = MQTT_CLIENT_RX_STATE_READ_TYPE;
          } else {
            _rxState = MQTT_CLIENT_RX_STATE_READ_VARIABLE_HEADER;
          }

          _rxMessageIndex = 0;
        }
        break;
      }

      case MQTT_CLIENT_RX_STATE_READ_VARIABLE_HEADER: {
        _rxMessageBuffer[_rxMessageIndex++] = b;

        if (_rxMessageIndex == _rxLength) {
          _rxState = MQTT_CLIENT_RX_STATE_READ_TYPE;

          if (_rxType == MQTT_CONNACK) {
            _returnCode = _rxMessageBuffer[1];
          } else if (_rxType == MQTT_PUBACK   ||
                      _rxType == MQTT_PUBREC  ||
                      _rxType == MQTT_PUBCOMP ||
                      _rxType == MQTT_UNSUBACK) {
            uint16_t packetId = (_rxMessageBuffer[0] << 8) | _rxMessageBuffer[1];

            if (packetId == _txPacketId) {
              _returnCode = 0;
            }
          } else if (_rxType == MQTT_PUBREL) {
            uint16_t packetId = (_rxMessageBuffer[0] << 8) | _rxMessageBuffer[1];

            if (_txStreamPayload) {
              // ignore, can't send as in the middle of a publish
            } else {
              pubcomp(packetId);
            }
          } else if (_rxType == MQTT_SUBACK) {
            uint16_t packetId = (_rxMessageBuffer[0] << 8) | _rxMessageBuffer[1];

            if (packetId == _txPacketId) {
              _returnCode = _rxMessageBuffer[2];
            }
          }
        }
        break;
      }

      case MQTT_CLIENT_RX_STATE_READ_PUBLISH_TOPIC_LENGTH: {
        _rxMessageBuffer[_rxMessageIndex++] = b;

        if (_rxMessageIndex == 2) {
          _rxMessageTopicLength = (_rxMessageBuffer[0] << 8) | _rxMessageBuffer[1];
          _rxLength -= 2;
          
          _rxMessageTopic = "";
          _rxMessageTopic.reserve(_rxMessageTopicLength);

          if (_rxMessageQoS) {
            if (_rxLength < (_rxMessageTopicLength + 2)) {
              stop();
              return;
            }
          } else {
            if (_rxLength < _rxMessageTopicLength) {
              stop();
              return;
            }
          }

          _rxMessageIndex = 0;
          _rxState = MQTT_CLIENT_RX_STATE_READ_PUBLISH_TOPIC;
        }

        break;
      }

      case MQTT_CLIENT_RX_STATE_READ_PUBLISH_TOPIC: {
        _rxMessageTopic += (char)b;

        if (_rxMessageTopicLength == _rxMessageTopic.length()) {
          _rxLength -= _rxMessageTopicLength;

          if (_rxMessageQoS) {
            _rxState = MQTT_CLIENT_RX_STATE_READ_PUBLISH_PACKET_ID;
          } else {
            _rxState = MQTT_CLIENT_RX_STATE_READ_PUBLISH_PAYLOAD;

            if (_onMessage) {
#ifdef MQTT_CLIENT_STD_FUNCTION_CALLBACK
              _onMessage(this,_rxLength);
#else
              _onMessage(_rxLength);
#endif

              if (_rxLength == 0) {
                _rxState = MQTT_CLIENT_RX_STATE_READ_TYPE;
              }
            }
          }
        }

        break;
      }

      case MQTT_CLIENT_RX_STATE_READ_PUBLISH_PACKET_ID: {
        _rxMessageBuffer[_rxMessageIndex++] = b;

        if (_rxMessageIndex == 2) {
          _rxLength -= 2;

          _rxPacketId = (_rxMessageBuffer[0] << 8) | _rxMessageBuffer[1];

          _rxState = MQTT_CLIENT_RX_STATE_READ_PUBLISH_PAYLOAD;

          if (_onMessage) {
#ifdef MQTT_CLIENT_STD_FUNCTION_CALLBACK
            _onMessage(this,_rxLength);
#else
            _onMessage(_rxLength);
#endif
          }

          if (_rxLength == 0) {
            // no payload to read, ack zero length message
            ackRxMessage();

            if (_onMessage) {
              _rxState = MQTT_CLIENT_RX_STATE_READ_TYPE;
            }
          }
        }

        break;
      }

      case MQTT_CLIENT_RX_STATE_READ_PUBLISH_PAYLOAD:
      case MQTT_CLIENT_RX_STATE_DISCARD_PUBLISH_PAYLOAD: {
        if (_rxLength > 0) {
          _rxLength--;
        }

        if (_rxLength == 0) {
          _rxState = MQTT_CLIENT_RX_STATE_READ_TYPE;
        } else {
          _rxState = MQTT_CLIENT_RX_STATE_DISCARD_PUBLISH_PAYLOAD;
        }

        break;
      }
    }

    if (_rxState == MQTT_CLIENT_RX_STATE_READ_PUBLISH_PAYLOAD) {
      break;
    }
  }

  if (_connected) {
    unsigned long now = millis();

    if ((now - _lastPingTx) >= _keepAliveInterval) {
      _lastPingTx = now;

      ping();
    } else if ((now - _lastRx) >= (_keepAliveInterval * 2)) {
      stop();
    }
  }
}

int MqttClient::connect(IPAddress ip, uint16_t port)
{
  return connect(ip, NULL, port);
}

int MqttClient::connect(const char *host, uint16_t port)
{
  return connect((uint32_t)0, host, port);
}

size_t MqttClient::write(uint8_t b)
{
  return write(&b, sizeof(b));
}

size_t MqttClient::write(const uint8_t *buf, size_t size)
{
  if (_willMessageIndex) {
    return writeData(buf, size);
  }

  if (_txStreamPayload) {
    return clientWrite(buf, size);
  }

  if ((_txPayloadBufferIndex + size) >= _tx_payload_buffer_size) {
    size = (_tx_payload_buffer_size - _txPayloadBufferIndex);
  }

  if (_txPayloadBuffer == NULL) {
    _txPayloadBuffer = (uint8_t*)malloc(_tx_payload_buffer_size);
  }

  memcpy(&_txPayloadBuffer[_txPayloadBufferIndex], buf, size);
  _txPayloadBufferIndex += size;

  return size;
}

int MqttClient::available()
{
  if (_rxState == MQTT_CLIENT_RX_STATE_READ_PUBLISH_PAYLOAD) {
    return _rxLength;
  }

  return 0;
}

int MqttClient::read()
{
  byte b;

  if (read(&b, sizeof(b)) != sizeof(b)) {
    return -1;
  }

  return b;
}

int MqttClient::read(uint8_t *buf, size_t size)
{
  size_t result = 0;

  if (_rxState == MQTT_CLIENT_RX_STATE_READ_PUBLISH_PAYLOAD) {
    size_t avail = available();

    if (size > avail) {
      size = avail;
    }

    while (result < size) {
      int b = clientTimedRead();

      if (b == -1) {
        break;
      } 

      result++;
      *buf++ = b;
    }

    if (result > 0) {
      _rxLength -= result;

      if (_rxLength == 0) {
        ackRxMessage();

        _rxState = MQTT_CLIENT_RX_STATE_READ_TYPE;
      }
    }
  }

  return result;
}

int MqttClient::peek()
{
  if (_rxState == MQTT_CLIENT_RX_STATE_READ_PUBLISH_PAYLOAD) {
    return clientPeek();
  }

  return -1;
}

void MqttClient::flush()
{
}

void MqttClient::stop()
{
  if (connected()) {
    disconnect();
  }

  _connected = false;
  _client->stop();
}

uint8_t MqttClient::connected()
{
  return clientConnected() && _connected;
}

MqttClient::operator bool()
{
  return true;
}

void MqttClient::setId(const char* id)
{
  _id = id;
}

void MqttClient::setId(const String& id)
{
  _id = id;
}

void MqttClient::setUsernamePassword(const char* username, const char* password)
{
  _username = username;
  _password = password;
}

void MqttClient::setUsernamePassword(const String& username, const String& password)
{
  _username = username;
  _password = password;
}

void MqttClient::setCleanSession(bool cleanSession)
{
  _cleanSession = cleanSession;
}

void MqttClient::setKeepAliveInterval(unsigned long interval)
{
  _keepAliveInterval = interval;
}

void MqttClient::setConnectionTimeout(unsigned long timeout)
{
  _connectionTimeout = timeout;
}

void MqttClient::setTxPayloadSize(unsigned short size)
{
  if (_txPayloadBuffer) {
    free(_txPayloadBuffer);
    _txPayloadBuffer = NULL;
    _txPayloadBufferIndex = 0;
  }
    
  _tx_payload_buffer_size = size;
}

int MqttClient::connectError() const
{
  return _connectError;
}

int MqttClient::subscribeQoS() const
{
  return _subscribeQos;
}

int MqttClient::connect(IPAddress ip, const char* host, uint16_t port)
{
  if (clientConnected()) {
    _client->stop();
  }
  _rxState = MQTT_CLIENT_RX_STATE_READ_TYPE;
  _connected = false;
  _txPacketId = 0x0000;

  if (host) {
    if (!_client->connect(host, port)) {
      _connectError = MQTT_CONNECTION_REFUSED;
      return 0;
    }
  } else {
    if (!_client->connect(ip, port)) {
      _connectError = MQTT_CONNECTION_REFUSED;
      return 0;
    }
  }

  _lastRx = millis();

  String id = _id;
  int idLength = id.length();
  int usernameLength = _username.length();
  int passwordLength = _password.length();
  uint8_t flags = 0;

  if (idLength == 0) {
    char tempId[17];

    snprintf(tempId, sizeof(tempId), "Arduino-%.8lx", millis());

    id = tempId;
    idLength = sizeof(tempId) - 1;
  }

  struct __attribute__ ((packed)) {
    struct {
      uint16_t length;
      char value[4];
    } protocolName;
    uint8_t level;
    uint8_t flags;
    uint16_t keepAlive;
  } connectVariableHeader;

  size_t remainingLength = sizeof(connectVariableHeader) + (2 + idLength) + _willBufferIndex;

  if (usernameLength) {
    flags |= 0x80;

    remainingLength += (2 + usernameLength);
  }

  if (passwordLength) {
    flags |= 0x40;

    remainingLength += (2 + passwordLength);
  }

  flags |= _willFlags;

  if (_cleanSession) {
    flags |= 0x02; // clean session
  }

  connectVariableHeader.protocolName.length = htons(sizeof(connectVariableHeader.protocolName.value));
  memcpy(connectVariableHeader.protocolName.value, "MQTT", sizeof(connectVariableHeader.protocolName.value));
  connectVariableHeader.level = 0x04;
  connectVariableHeader.flags = flags;
  connectVariableHeader.keepAlive = htons(_keepAliveInterval / 1000);

  uint8_t packetBuffer[5 + remainingLength];

  beginPacket(MQTT_CONNECT, 0x00, remainingLength, packetBuffer);
  writeData(&connectVariableHeader, sizeof(connectVariableHeader));
  writeString(id.c_str(), idLength);

  if (_willBufferIndex) {
    writeData(_willBuffer, _willBufferIndex);
  }

  if (usernameLength) {
    writeString(_username.c_str(), usernameLength);
  }

  if (passwordLength) {
    writeString(_password.c_str(), passwordLength);
  }

  if (!endPacket()) {
    _client->stop();

    _connectError = MQTT_SERVER_UNAVAILABLE;

    return 0;
  }

  _returnCode = MQTT_CONNECTION_TIMEOUT;

  for (unsigned long start = millis(); ((millis() - start) < _connectionTimeout) && clientConnected();) {
    poll();

    if (_returnCode != MQTT_CONNECTION_TIMEOUT) {
      break;
    }
    yield();
  }

  _connectError = _returnCode;

  if (_returnCode == MQTT_SUCCESS) {
    _connected = true;

    return 1;
  }

  _client->stop();

  return 0;
}

int MqttClient::publishHeader(size_t length)
{
  int topicLength = _txMessageTopic.length();
  int headerLength = topicLength + 2;

  if (_txMessageQoS > 2) {
    // invalid QoS
    return 0;
  }

  if (_txMessageQoS) {
    // add two for packet id
    headerLength += 2;

    _txPacketId++;

    if (_txPacketId == 0) {
      _txPacketId = 1;
    }
  }

  // only for packet header
  uint8_t packetHeaderBuffer[5 + headerLength];

  uint8_t flags = 0;

  if (_txMessageRetain) {
    flags |= 0x01;
  }

  if (_txMessageQoS) {
    flags |= (_txMessageQoS << 1);
  }

  if (_txMessageDup) {
    flags |= 0x08;
  }

  beginPacket(MQTT_PUBLISH, flags, headerLength + length, packetHeaderBuffer);
  writeString(_txMessageTopic.c_str(), topicLength);
  if (_txMessageQoS) {
    write16(_txPacketId);
  }

  // send packet header
  return endPacket();
}

void MqttClient::puback(uint16_t id)
{
  uint8_t packetBuffer[4];

  beginPacket(MQTT_PUBACK, 0x00, 2, packetBuffer);
  write16(id);
  endPacket();
}

void MqttClient::pubrec(uint16_t id)
{
  uint8_t packetBuffer[4];

  beginPacket(MQTT_PUBREC, 0x00, 2, packetBuffer);
  write16(id);
  endPacket();
}

void MqttClient::pubrel(uint16_t id)
{
  uint8_t packetBuffer[4];

  beginPacket(MQTT_PUBREL, 0x02, 2, packetBuffer);
  write16(id);
  endPacket();
}

void MqttClient::pubcomp(uint16_t id)
{
  uint8_t packetBuffer[4];

  beginPacket(MQTT_PUBCOMP, 0x00, 2, packetBuffer);
  write16(id);
  endPacket();
}

void MqttClient::ping()
{
  uint8_t packetBuffer[2];

  beginPacket(MQTT_PINGREQ, 0, 0, packetBuffer);
  endPacket();
}

void MqttClient::disconnect()
{
  uint8_t packetBuffer[2];

  beginPacket(MQTT_DISCONNECT, 0, 0, packetBuffer);
  endPacket();
}

int MqttClient::beginPacket(uint8_t type, uint8_t flags, size_t length, uint8_t* buffer)
{
  _txBuffer = buffer;
  _txBufferIndex = 0;

  write8((type << 4) | flags);

  do {
    uint8_t b = length % 128;
    length /= 128;

    if(length > 0) {
      b |= 0x80;
    }

    _txBuffer[_txBufferIndex++] = b;
  } while (length > 0);

  return _txBufferIndex;
}

int MqttClient::writeString(const char* s, uint16_t length)
{
  int result = 0;

  result += write16(length);
  result += writeData(s, length);

  return result;
}

int MqttClient::write8(uint8_t val)
{
  return writeData(&val, sizeof(val));
}

int MqttClient::write16(uint16_t val)
{
  val = htons(val);

  return writeData(&val, sizeof(val));
}

int MqttClient::writeData(const void* data, size_t length)
{
  memcpy(&_txBuffer[_txBufferIndex], data, length);
  _txBufferIndex += length;

  return length;
}

int MqttClient::endPacket()
{
  int result = (clientWrite(_txBuffer, _txBufferIndex) == _txBufferIndex);

  _txBufferIndex = 0;

  return result;
}

void MqttClient::ackRxMessage()
{
  if (_rxMessageQoS == 1) {
    puback(_rxPacketId);
  } else if (_rxMessageQoS == 2) {
    pubrec(_rxPacketId);
  }
}

int MqttClient::clientRead()
{
  int result = _client->read();

#ifdef MQTT_CLIENT_DEBUG
  if (result != -1) {
    Serial.print("RX: ");

    if (result < 16) {
      Serial.print('0');
    }

    Serial.println(result, HEX);
  }
#endif

  return result;
}

uint8_t MqttClient::clientConnected()
{
  return _client->connected();
}

int MqttClient::clientAvailable()
{
  return _client->available();
}

int MqttClient::clientTimedRead()
{
  unsigned long startMillis = millis();

  do {
    if (clientAvailable()) {
      return clientRead();
    } else if (!clientConnected()) {
      return -1;
    }

    yield();
  } while((millis() - startMillis) < 1000);

  return -1;
}

int MqttClient::clientPeek()
{
  return _client->peek();
}

size_t MqttClient::clientWrite(const uint8_t *buf, size_t size)
{
#ifdef MQTT_CLIENT_DEBUG
  Serial.print("TX[");
  Serial.print(size);
  Serial.print("]: ");
  for (size_t i = 0; i < size; i++) {
    uint8_t b = buf[i];

    if (b < 16) {
      Serial.print('0');
    }

    Serial.print(b, HEX);
    Serial.print(' ');
  }
  Serial.println();
#endif

  return _client->write(buf, size);
}
