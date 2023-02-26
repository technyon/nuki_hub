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

class espMqttClientWifi : public MqttClientSetup<espMqttClientWifi> {
public:
#if defined(ARDUINO_ARCH_ESP32)
    explicit espMqttClientWifi(uint8_t priority = 1, uint8_t core = 1);
#else
    espMqttClient();
#endif

    void update();

protected:
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
    espMqttClientInternals::ClientSync _client;
#elif defined(__linux__)
    espMqttClientInternals::ClientPosix _client;
#endif
};

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
class espMqttClientWifiSecure : public MqttClientSetup<espMqttClientWifiSecure> {
public:
#if defined(ARDUINO_ARCH_ESP32)
    explicit espMqttClientWifiSecure(uint8_t priority = 1, uint8_t core = 1);
#else
    espMqttClientSecure();
#endif
    espMqttClientWifiSecure& setInsecure();
#if defined(ARDUINO_ARCH_ESP32)
    espMqttClientWifiSecure& setCACert(const char* rootCA);
    espMqttClientWifiSecure& setCertificate(const char* clientCa);
    espMqttClientWifiSecure& setPrivateKey(const char* privateKey);
    espMqttClientWifiSecure& setPreSharedKey(const char* pskIdent, const char* psKey);
#else
    espMqttClientSecure& setFingerprint(const uint8_t fingerprint[20]);
  espMqttClientSecure& setTrustAnchors(const X509List *ta);
  espMqttClientSecure& setClientRSACert(const X509List *cert, const PrivateKey *sk);
  espMqttClientSecure& setClientECCert(const X509List *cert, const PrivateKey *sk, unsigned allowed_usages, unsigned cert_issuer_key_type);
  espMqttClientSecure& setCertStore(CertStoreBase *certStore);
#endif

    void update();

protected:
    espMqttClientInternals::ClientSecureSync _client;
};

#endif
