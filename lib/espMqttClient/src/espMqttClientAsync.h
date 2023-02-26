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

#include "Transport/ClientAsync.h"

#include "MqttClientSetup.h"
#include "Transport/ClientSecureSync.h"

class espMqttClientAsync : public MqttClientSetup<espMqttClientAsync> {
 public:
#if defined(ARDUINO_ARCH_ESP32)
  explicit espMqttClientAsync(uint8_t priority = 1, uint8_t core = 1);
#else
  espMqttClientAsync();
#endif
  bool connect();

 protected:
  espMqttClientInternals::ClientAsync _clientAsync;
  static void _setupClient(espMqttClientAsync* c);
  static void _disconnectClient(espMqttClientAsync* c);

  static void onConnectCb(void* a, AsyncClient* c);
  static void onDataCb(void* a, AsyncClient* c, void* data, size_t len);
  static void onDisconnectCb(void* a, AsyncClient* c);
  static void onPollCb(void* a, AsyncClient* c);
};

#endif


#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
class espMqttClientSecureAsync : public MqttClientSetup<espMqttClientSecureAsync> {
public:
#if defined(ARDUINO_ARCH_ESP32)
    explicit espMqttClientSecureAsync(uint8_t priority = 1, uint8_t core = 1);
#else
    espMqttClientSecure();
#endif
    espMqttClientSecureAsync& setInsecure();
#if defined(ARDUINO_ARCH_ESP32)
    espMqttClientSecureAsync& setCACert(const char* rootCA);
    espMqttClientSecureAsync& setCertificate(const char* clientCa);
    espMqttClientSecureAsync& setPrivateKey(const char* privateKey);
    espMqttClientSecureAsync& setPreSharedKey(const char* pskIdent, const char* psKey);
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