/*
Copyright (c) 2022 Bert Melis. All rights reserved.

This work is licensed under the terms of the MIT license.
For a copy, see <https://opensource.org/licenses/MIT> or
the LICENSE file.
*/

#pragma once

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)

#ifdef FRAMEWORK_ARDUINO_SOLO1
#include <WiFiClientSecure.h>  // includes IPAddress
#else
#include <NetworkClientSecure.h>  // includes IPAddress
#endif

#include "Transport.h"

namespace espMqttClientInternals {

class ClientSecureSync : public Transport {
 public:
  ClientSecureSync();
  bool connect(IPAddress ip, uint16_t port) override;
  bool connect(const char* host, uint16_t port) override;
  size_t write(const uint8_t* buf, size_t size) override;
  int read(uint8_t* buf, size_t size) override;
  void stop() override;
  bool connected() override;
  bool disconnected() override;
  #if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0))
  WiFiClientSecure client;
  #else
  NetworkClientSecure client;
  #endif
};

}  // namespace espMqttClientInternals

#endif
