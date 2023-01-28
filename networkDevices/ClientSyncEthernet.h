#pragma once

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)

#include "Transport/Transport.h"
#include "EthernetClient.h"

namespace espMqttClientInternals {

    class ClientSyncEthernet : public Transport {
    public:
        ClientSyncEthernet();
        bool connect(IPAddress ip, uint16_t port) override;
        bool connect(const char* host, uint16_t port) override;
        size_t write(const uint8_t* buf, size_t size) override;
        int available() override;
        int read(uint8_t* buf, size_t size) override;
        void stop() override;
        bool connected() override;
        bool disconnected() override;
        EthernetClient client;
    };

}  // namespace espMqttClientInternals

#endif