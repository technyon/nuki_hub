// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou
 */
#pragma once

#if defined(ESP8266)
#include "ESP8266WiFi.h"
#elif defined(ESP32)
#include "WiFi.h"
#endif

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <functional>

#define WSL_VERSION          "6.3.0"
#define WSL_VERSION_MAJOR    6
#define WSL_VERSION_MINOR    3
#define WSL_VERSION_REVISION 0

#ifndef WSL_MAX_WS_CLIENTS
#define WSL_MAX_WS_CLIENTS DEFAULT_MAX_WS_CLIENTS
#endif

// High performance mode:
// - Low memory footprint (no stack allocation, no global buffer by default)
// - Low latency (messages sent immediately to the WebSocket queue)
// - High throughput (up to 20 messages per second, no locking mechanism)
// Also recommended to tweak AsyncTCP and ESPAsyncWebServer settings, for example:
//  -D CONFIG_ASYNC_TCP_QUEUE_SIZE=128  // AsyncTCP queue size
//  -D CONFIG_ASYNC_TCP_RUNNING_CORE=1  // core for the async_task
//  -D WS_MAX_QUEUED_MESSAGES=128       // WS message queue size

typedef std::function<void(uint8_t* data, size_t len)> WSLMessageHandler;
typedef std::function<void(const String& msg)> WSLStringMessageHandler;

class WebSerialClass : public Print {
  public:
    void begin(AsyncWebServer* server, const char* url = "/webserial");
    inline void setAuthentication(const char* username, const char* password) { setAuthentication(String(username), String(password)); }
    void setAuthentication(const String& username, const String& password);
    void onMessage(WSLMessageHandler recv);
    void onMessage(WSLStringMessageHandler recv);
    size_t write(uint8_t) override;
    size_t write(const uint8_t* buffer, size_t size) override;

    // A buffer (shared across cores) can be initialised with an initial capacity to be able to use any Print functions event those that are not buffered and would
    // create a performance impact for WS calls. The goal of this buffer is to be used with lines ending with '\n', like log messages.
    // The buffer size will eventually grow until a '\n' is found, then the message will be sent to the WS clients and a new buffer will be created.
    // Set initialCapacity to 0 to disable buffering.
    // Must be called before begin(): calling it after will erase the buffer and its content will be lost.
    // The buffer is not enabled by default.
    void setBuffer(size_t initialCapacity);

  private:
    // Server
    AsyncWebServer* _server;
    AsyncWebSocket* _ws;
    WSLMessageHandler _recv = nullptr;
    WSLStringMessageHandler _recvString = nullptr;
    bool _authenticate = false;
    String _username;
    String _password;
    size_t _initialBufferCapacity = 0;
    String _buffer;
    void _send(const uint8_t* buffer, size_t size);
};

extern WebSerialClass WebSerial;
