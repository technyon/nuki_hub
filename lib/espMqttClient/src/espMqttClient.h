/*
Copyright (c) 2022 Bert Melis. All rights reserved.

API is based on the original work of Marvin Roger:
https://github.com/marvinroger/async-mqtt-client

This work is licensed under the terms of the MIT license.  
For a copy, see <https://opensource.org/licenses/MIT> or
the LICENSE file.
*/

#pragma once

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
#include "Transport/ClientSync.h"
#include "Transport/ClientSecureSync.h"
#elif defined(__linux__)
#include "Transport/ClientPosix.h"
#endif

#include "MqttClientSetup.h"

class espMqttClient : public MqttClientSetup<espMqttClient> {
 public:
#if defined(ARDUINO_ARCH_ESP32)
  explicit espMqttClient(uint8_t priority = 1, uint8_t core = 1);
#else
  espMqttClient();
#endif

 protected:
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
  espMqttClientInternals::ClientSync _client;
#elif defined(__linux__)
  espMqttClientInternals::ClientPosix _client;
#endif
};

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
class espMqttClientSecure : public MqttClientSetup<espMqttClientSecure> {
 public:
  #if defined(ARDUINO_ARCH_ESP32)
  explicit espMqttClientSecure(uint8_t priority = 1, uint8_t core = 1);
  #else
  espMqttClientSecure();
  #endif
  espMqttClientSecure& setInsecure();
  #if defined(ARDUINO_ARCH_ESP32)
  espMqttClientSecure& setCACert(const char* rootCA);
  espMqttClientSecure& setCertificate(const char* clientCa);
  espMqttClientSecure& setPrivateKey(const char* privateKey);
  espMqttClientSecure& setPreSharedKey(const char* pskIdent, const char* psKey);
  #else
  espMqttClientSecure& setFingerprint(const uint8_t fingerprint[20]);
  espMqttClientSecure& setTrustAnchors(const X509List *ta);
  espMqttClientSecure& setClientRSACert(const X509List *cert, const PrivateKey *sk);
  espMqttClientSecure& setClientECCert(const X509List *cert, const PrivateKey *sk, unsigned allowed_usages, unsigned cert_issuer_key_type);
  espMqttClientSecure& setCertStore(CertStoreBase *certStore);
  #endif

 protected:
  espMqttClientInternals::ClientSecureSync _client;
};

#endif
