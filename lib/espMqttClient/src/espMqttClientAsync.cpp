/*
Copyright (c) 2022 Bert Melis. All rights reserved.

This work is licensed under the terms of the MIT license.  
For a copy, see <https://opensource.org/licenses/MIT> or
the LICENSE file.
*/

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)

#include "espMqttClientAsync.h"

#if defined(ARDUINO_ARCH_ESP32)
espMqttClientAsync::espMqttClientAsync(uint8_t priority, uint8_t core)
: MqttClientSetup(false, priority, core)
, _clientAsync() {
#else
espMqttClientAsync::espMqttClientAsync()
: _clientAsync() {
#endif
  _transport = &_clientAsync;
  // _onConnectHook = reinterpret_cast<MqttClient::OnConnectHook>(_setupClient);
  // _onConnectHookArg = this;
  _clientAsync.client.onConnect(onConnectCb, this);
  _clientAsync.client.onDisconnect(onDisconnectCb, this);
  _clientAsync.client.onData(onDataCb, this);
  _clientAsync.client.onPoll(onPollCb, this);
}

bool espMqttClientAsync::connect() {
  bool ret = MqttClient::connect();
  loop();
  return ret;
}

void espMqttClientAsync::_setupClient(espMqttClientAsync* c) {
  (void)c;
}

void espMqttClientAsync::onConnectCb(void* a, AsyncClient* c) {
  c->setNoDelay(true);
  espMqttClientAsync* client = reinterpret_cast<espMqttClientAsync*>(a);
  client->_state = MqttClient::State::connectingTcp2;
  client->loop();
}

void espMqttClientAsync::onDataCb(void* a, AsyncClient* c, void* data, size_t len) {
  (void)c;
  espMqttClientAsync* client = reinterpret_cast<espMqttClientAsync*>(a);
  client->_clientAsync.bufData = reinterpret_cast<uint8_t*>(data);
  client->_clientAsync.availableData = len;
  client->loop();
}

void espMqttClientAsync::onDisconnectCb(void* a, AsyncClient* c) {
  (void)c;
  espMqttClientAsync* client = reinterpret_cast<espMqttClientAsync*>(a);
  client->_state = MqttClient::State::disconnectingTcp2;
  client->loop();
}

void espMqttClientAsync::onPollCb(void* a, AsyncClient* c) {
  (void)c;
  espMqttClientAsync* client = reinterpret_cast<espMqttClientAsync*>(a);
  client->loop();
}

#endif


#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
#if defined(ARDUINO_ARCH_ESP32)
espMqttClientSecureAsync::espMqttClientSecureAsync(uint8_t priority, uint8_t core)
        : MqttClientSetup(false, priority, core)
        , _client() {
#else
    espMqttClientSecure::espMqttClientSecure()
: _client() {
#endif
    _transport = &_client;
}

espMqttClientSecureAsync& espMqttClientSecureAsync::setInsecure() {
    _client.client.setInsecure();
    return *this;
}

#if defined(ARDUINO_ARCH_ESP32)
espMqttClientSecureAsync& espMqttClientSecureAsync::setCACert(const char* rootCA) {
    _client.client.setCACert(rootCA);
    return *this;
}

espMqttClientSecureAsync& espMqttClientSecureAsync::setCertificate(const char* clientCa) {
    _client.client.setCertificate(clientCa);
    return *this;
}

espMqttClientSecureAsync& espMqttClientSecureAsync::setPrivateKey(const char* privateKey) {
    _client.client.setPrivateKey(privateKey);
    return *this;
}

espMqttClientSecureAsync& espMqttClientSecureAsync::setPreSharedKey(const char* pskIdent, const char* psKey) {
    _client.client.setPreSharedKey(pskIdent, psKey);
    return *this;
}
#elif defined(ARDUINO_ARCH_ESP8266)
espMqttClientSecureAsync& espMqttClientSecureAsync::setFingerprint(const uint8_t fingerprint[20]) {
  _client.client.setFingerprint(fingerprint);
  return *this;
}

espMqttClientSecureAsync& espMqttClientSecureAsync::setTrustAnchors(const X509List *ta) {
  _client.client.setTrustAnchors(ta);
  return *this;
}

espMqttClientSecureAsync& espMqttClientSecureAsync::setClientRSACert(const X509List *cert, const PrivateKey *sk) {
  _client.client.setClientRSACert(cert, sk);
  return *this;
}

espMqttClientSecureAsync& espMqttClientSecureAsync::setClientECCert(const X509List *cert, const PrivateKey *sk, unsigned allowed_usages, unsigned cert_issuer_key_type) {
  _client.client.setClientECCert(cert, sk, allowed_usages, cert_issuer_key_type);
  return *this;
}

espMqttClientSecureAsync& espMqttClientSecureAsync::setCertStore(CertStoreBase *certStore) {
  _client.client.setCertStore(certStore);
  return *this;
}
#endif
#endif