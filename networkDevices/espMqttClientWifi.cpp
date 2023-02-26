/*
Copyright (c) 2022 Bert Melis. All rights reserved.

This work is licensed under the terms of the MIT license.
For a copy, see <https://opensource.org/licenses/MIT> or
the LICENSE file.
*/

#include "espMqttClientWifi.h"

#if defined(ARDUINO_ARCH_ESP32)
espMqttClientWifi::espMqttClientWifi(uint8_t priority, uint8_t core)
        : MqttClientSetup(false, priority, core)
        , _client() {
#else
    espMqttClient::espMqttClient()
: _client() {
#endif
    _transport = &_client;
}

void espMqttClientWifi::update()
{
    loop();
}

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
#if defined(ARDUINO_ARCH_ESP32)
espMqttClientWifiSecure::espMqttClientWifiSecure(uint8_t priority, uint8_t core)
        : MqttClientSetup(false, priority, core)
        , _client() {
#else
    espMqttClientSecure::espMqttClientSecure()
: _client() {
#endif
    _transport = &_client;
}

espMqttClientWifiSecure& espMqttClientWifiSecure::setInsecure() {
    _client.client.setInsecure();
    return *this;
}

#if defined(ARDUINO_ARCH_ESP32)
espMqttClientWifiSecure& espMqttClientWifiSecure::setCACert(const char* rootCA) {
    _client.client.setCACert(rootCA);
    return *this;
}

espMqttClientWifiSecure& espMqttClientWifiSecure::setCertificate(const char* clientCa) {
    _client.client.setCertificate(clientCa);
    return *this;
}

espMqttClientWifiSecure& espMqttClientWifiSecure::setPrivateKey(const char* privateKey) {
    _client.client.setPrivateKey(privateKey);
    return *this;
}

espMqttClientWifiSecure& espMqttClientWifiSecure::setPreSharedKey(const char* pskIdent, const char* psKey) {
    _client.client.setPreSharedKey(pskIdent, psKey);
    return *this;
}

void espMqttClientWifiSecure::update()
{
    loop();
}

#elif defined(ARDUINO_ARCH_ESP8266)
espMqttClientWifiSecure& espMqttClientWifiSecure::setFingerprint(const uint8_t fingerprint[20]) {
  _client.client.setFingerprint(fingerprint);
  return *this;
}

espMqttClientWifiSecure& espMqttClientWifiSecure::setTrustAnchors(const X509List *ta) {
  _client.client.setTrustAnchors(ta);
  return *this;
}

espMqttClientWifiSecure& espMqttClientWifiSecure::setClientRSACert(const X509List *cert, const PrivateKey *sk) {
  _client.client.setClientRSACert(cert, sk);
  return *this;
}

espMqttClientWifiSecure& espMqttClientWifiSecure::setClientECCert(const X509List *cert, const PrivateKey *sk, unsigned allowed_usages, unsigned cert_issuer_key_type) {
  _client.client.setClientECCert(cert, sk, allowed_usages, cert_issuer_key_type);
  return *this;
}

espMqttClientWifiSecure& espMqttClientWifiSecure::setCertStore(CertStoreBase *certStore) {
  _client.client.setCertStore(certStore);
  return *this;
}
#endif

#endif
