/*
Copyright (c) 2022 Bert Melis. All rights reserved.

This work is licensed under the terms of the MIT license.  
For a copy, see <https://opensource.org/licenses/MIT> or
the LICENSE file.
*/

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)

#include "ClientSyncW5500.h"
#include <lwip/sockets.h>  // socket options

namespace espMqttClientInternals {

    ClientSyncW5500::ClientSyncW5500()
            : client() {
        // empty
    }

    bool ClientSyncW5500::connect(IPAddress ip, uint16_t port) {
        bool ret = client.connect(ip, port);  // implicit conversion of return code int --> bool
        if (ret) {
#if defined(ARDUINO_ARCH_ESP8266)
            client.setNoDelay(true);
#elif defined(ARDUINO_ARCH_ESP32)
            // Set TCP option directly to bypass lack of working setNoDelay for WiFiClientSecure (for consistency also here)
            int val = true;

            // TODO
//            client.setSocketOption(IPPROTO_TCP, TCP_NODELAY, &val, sizeof(int));
#endif
        }
        return ret;
    }

    bool ClientSyncW5500::connect(const char* host, uint16_t port) {
        bool ret = client.connect(host, port);  // implicit conversion of return code int --> bool
        if (ret) {
#if defined(ARDUINO_ARCH_ESP8266)
            client.setNoDelay(true);
#elif defined(ARDUINO_ARCH_ESP32)
            // Set TCP option directly to bypass lack of working setNoDelay for WiFiClientSecure (for consistency also here)
            int val = true;

            // TODO
//            client.setSocketOption(IPPROTO_TCP, TCP_NODELAY, &val, sizeof(int));
#endif
        }
        return ret;
    }

    size_t ClientSyncW5500::write(const uint8_t* buf, size_t size) {
        return client.write(buf, size);
    }

    int ClientSyncW5500::available() {
        return client.available();
    }

    int ClientSyncW5500::read(uint8_t* buf, size_t size) {
        return client.read(buf, size);
    }

    void ClientSyncW5500::stop() {
        client.stop();
    }

    bool ClientSyncW5500::connected() {
        return client.connected();
    }

    bool ClientSyncW5500::disconnected() {
        return !client.connected();
    }

}  // namespace espMqttClientInternals

#endif
