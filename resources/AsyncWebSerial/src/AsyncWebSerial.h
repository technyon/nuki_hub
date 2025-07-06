/**
 * @file AsyncWebSerial.h
 * @brief A web serial interface for the ESP32 microcontroller.
 * @license MIT License (https://opensource.org/licenses/MIT)
 */

#ifndef AsyncWebSerial_h
#define AsyncWebSerial_h

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <stdlib_noniso.h>

#include <functional>

// define the callable object type for the message received callback
typedef std::function<void(uint8_t *data, size_t len)> WSMessageCallback;
typedef std::function<void(const String &msg)> WSStringMessageCallback;

class AsyncWebSerial : public Print
{
  public:
    void begin(AsyncWebServer *server, const char *url = "/webserial");

    void setAuthentication(const String &username, const String &password);
    void onMessage(WSMessageCallback callback);
    void onMessage(WSStringMessageCallback callback);

    void loop();

    // Print interface implementation
    virtual size_t write(uint8_t) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;

  private:
    AsyncWebServer *_server;
    AsyncWebSocket *_socket;
    bool _isAuthenticationRequired = false;
    String _username;
    String _password;

    WSMessageCallback _onMessageReceived           = nullptr;
    WSStringMessageCallback _stringMessageCallback = nullptr;
};

#endif // AsyncWebSerial_h