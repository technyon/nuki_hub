/*
Copyright (c) 2022 Bert Melis. All rights reserved.

This work is licensed under the terms of the MIT license.  
For a copy, see <https://opensource.org/licenses/MIT> or
the LICENSE file.
*/

#include "espMqttClient.h"

#if defined(ARDUINO_ARCH_ESP32)
espMqttClient::espMqttClient(uint8_t priority, uint8_t core)
: MqttClientSetup(true, priority, core)
, _client() {
#else
espMqttClient::espMqttClient()
: _client() {
#endif
  _transport = &_client;
}

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
#if defined(ARDUINO_ARCH_ESP32)
espMqttClientSecure::espMqttClientSecure(uint8_t priority, uint8_t core)
: MqttClientSetup(priority, core)
, _client() {
#else
espMqttClientSecure::espMqttClientSecure()
: _client() {
#endif
  _transport = &_client;
}

espMqttClientSecure& espMqttClientSecure::setInsecure() {
  _client.client.setInsecure();
  return *this;
}

#if defined(ARDUINO_ARCH_ESP32)
espMqttClientSecure& espMqttClientSecure::setCACert(const char* rootCA) {
  _client.client.setCACert(rootCA);
  return *this;
}

espMqttClientSecure& espMqttClientSecure::setCertificate(const char* clientCa) {
  _client.client.setCertificate(clientCa);
  return *this;
}

espMqttClientSecure& espMqttClientSecure::setPrivateKey(const char* privateKey) {
  _client.client.setPrivateKey(privateKey);
  return *this;
}

espMqttClientSecure& espMqttClientSecure::setPreSharedKey(const char* pskIdent, const char* psKey) {
  _client.client.setPreSharedKey(pskIdent, psKey);
  return *this;
}
#elif defined(ARDUINO_ARCH_ESP8266)
espMqttClientSecure& espMqttClientSecure::setFingerprint(const uint8_t fingerprint[20]) {
  _client.client.setFingerprint(fingerprint);
  return *this;
}

espMqttClientSecure& espMqttClientSecure::setTrustAnchors(const X509List *ta) {
  _client.client.setTrustAnchors(ta);
  return *this;
}

espMqttClientSecure& espMqttClientSecure::setClientRSACert(const X509List *cert, const PrivateKey *sk) {
  _client.client.setClientRSACert(cert, sk);
  return *this;
}

espMqttClientSecure& espMqttClientSecure::setClientECCert(const X509List *cert, const PrivateKey *sk, unsigned allowed_usages, unsigned cert_issuer_key_type) {
  _client.client.setClientECCert(cert, sk, allowed_usages, cert_issuer_key_type);
  return *this;
}

espMqttClientSecure& espMqttClientSecure::setCertStore(CertStoreBase *certStore) {
  _client.client.setCertStore(certStore);
  return *this;
}
#endif

#endif
