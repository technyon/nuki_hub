// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou
 */
#include "MycilaWebSerial.h"

#include "MycilaWebSerialPage.h"

void WebSerialClass::setAuthentication(const String& username, const String& password) {
  _username = username;
  _password = password;
  _authenticate = !_username.isEmpty() && !_password.isEmpty();
  if (_ws) {
    _ws->setAuthentication(_username.c_str(), _password.c_str());
  }
}

void WebSerialClass::begin(AsyncWebServer* server, const char* url) {
  _server = server;

  String backendUrl = url;
  backendUrl.concat("ws");
  _ws = new AsyncWebSocket(backendUrl);

  if (_authenticate) {
    _ws->setAuthentication(_username.c_str(), _password.c_str());
  }

  _server->on(url, HTTP_GET, [&](AsyncWebServerRequest* request) {
    if (_authenticate) {
      if (!request->authenticate(_username.c_str(), _password.c_str()))
        return request->requestAuthentication();
    }
    AsyncWebServerResponse* response = request->beginResponse(200, "text/html", WEBSERIAL_HTML, sizeof(WEBSERIAL_HTML));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  _ws->onEvent([&](__unused AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, __unused void* arg, uint8_t* data, __unused size_t len) -> void {
    if (type == WS_EVT_CONNECT) {
      client->setCloseClientOnQueueFull(false);
      return;
    }
    if (type == WS_EVT_DATA) {
      AwsFrameInfo* info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len) {
        if (info->opcode == WS_TEXT) {
          data[len] = 0;
        }
        if (strcmp((char*)data, "ping") == 0)
          client->text("pong");
        else if (_recv)
          _recv(data, len);
      }
    }
  });

  _server->addHandler(_ws);
}

void WebSerialClass::onMessage(WSLMessageHandler recv) {
  _recv = recv;
}

void WebSerialClass::onMessage(WSLStringMessageHandler callback) {
  _recvString = callback;
  _recv = [&](uint8_t* data, size_t len) {
    if (data && len) {
      String msg;
      msg.reserve(len);
      msg.concat((char*)data);
      _recvString(msg);
    }
  };
}

size_t WebSerialClass::write(uint8_t m) {
  if (!_ws)
    return 0;

  // We do not support non-buffered write on webserial for the HIGH_PERF version
  // we fail with a stack trace allowing the user to change the code to use write(const uint8_t* buffer, size_t size) instead
  if (!_initialBufferCapacity) {
#ifdef ESP8266
    ets_printf("'-D WSL_FAIL_ON_NON_BUFFERED_WRITE' is set: non-buffered write is not supported. Please use write(const uint8_t* buffer, size_t size) instead.");
#else
    log_e("'-D WSL_FAIL_ON_NON_BUFFERED_WRITE' is set: non-buffered write is not supported. Please use write(const uint8_t* buffer, size_t size) instead.");
#endif
    assert(false);
    return 0;
  }

  write(&m, 1);
  return (1);
}

size_t WebSerialClass::write(const uint8_t* buffer, size_t size) {
  if (!_ws || size == 0)
    return 0;

  // No buffer, send directly (i.e. use case for log streaming)
  if (!_initialBufferCapacity) {
    size = buffer[size - 1] == '\n' ? size - 1 : size;
    _send(buffer, size);
    return size;
  }

  // fill the buffer while sending data for each EOL
  size_t start = 0, end = 0;
  while (end < size) {
    if (buffer[end] == '\n') {
      if (end > start) {
        _buffer.concat(reinterpret_cast<const char*>(buffer + start), end - start);
      }
      _send(reinterpret_cast<const uint8_t*>(_buffer.c_str()), _buffer.length());
      start = end + 1;
    }
    end++;
  }
  if (end > start) {
    _buffer.concat(reinterpret_cast<const char*>(buffer + start), end - start);
  }
  return size;
}

void WebSerialClass::_send(const uint8_t* buffer, size_t size) {
  if (_ws && size > 0) {
    _ws->cleanupClients(WSL_MAX_WS_CLIENTS);
    if (_ws->count()) {
      _ws->textAll((const char*)buffer, size);
    }
  }

  // if buffer grew too much, free it, otherwise clear it
  if (_initialBufferCapacity) {
    if (_buffer.length() > _initialBufferCapacity) {
      setBuffer(_initialBufferCapacity);
    } else {
      _buffer.clear();
    }
  }
}

void WebSerialClass::setBuffer(size_t initialCapacity) {
  assert(initialCapacity <= UINT16_MAX);
  _initialBufferCapacity = initialCapacity;
  _buffer = String();
  _buffer.reserve(initialCapacity);
}

WebSerialClass WebSerial;
