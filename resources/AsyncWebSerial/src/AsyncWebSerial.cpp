/*
 * AsyncWebSerial.cpp
 *
 * This file implements the AsyncWebSerial class, which provides a web-based serial interface
 * using an asynchronous web server and WebSocket communication. It allows for sending
 * and receiving serial data over a web interface, with optional authentication.
 *
 * Usage:
 * - Call begin() to initialize the AsyncWebSerial with a server and URL.
 * - Use onMessage() to set a callback for received messages.
 * - Use setAuthentication() to enable authentication for the web interface.
 * - Use the Print interface to send data to the web interface.
 * - Call loop() periodically to clean up inactive clients.
 */

#include "AsyncWebSerial.h"
#include "AsyncWebSerialHTML.h"

void AsyncWebSerial::begin(AsyncWebServer *server, const char *url)
{
    _server = server;
    _socket = new AsyncWebSocket("/ws_serial");

    if (_isAuthenticationRequired)
        _socket->setAuthentication(_username.c_str(), _password.c_str());

    // handle web page request
    _server->on(url, HTTP_GET, [&](AsyncWebServerRequest *request) {
        if (_isAuthenticationRequired && !request->authenticate(_username.c_str(), _password.c_str()))
            return request->requestAuthentication();

        AsyncWebServerResponse *response =
            request->beginResponse(200, "text/html", ASYNCWEBSERIAL_HTML, sizeof(ASYNCWEBSERIAL_HTML));
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    });

    // handle websocket connection
    _socket->onEvent([&](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg,
                         uint8_t *data, size_t len) {
        switch (type) {
        case WS_EVT_CONNECT:
            // set the client to not close when the queue is full
            client->setCloseClientOnQueueFull(false);
            break;
        case WS_EVT_DATA:
            if (len > 0) {
                // invoke the message received callback
                if (_onMessageReceived != nullptr)
                    _onMessageReceived(data, len);
            }
            break;
        }
    });

    _server->addHandler(_socket);
}

// onMessage callback setter
void AsyncWebSerial::onMessage(WSMessageCallback callback)
{
    _onMessageReceived = callback;
}

// onMessage callback setter
void AsyncWebSerial::onMessage(WSStringMessageCallback callback)
{
    // keep a reference to the callback function
    _stringMessageCallback = callback;

    // set the internal callback to convert the uint8_t data to a string and
    // call the string callback
    _onMessageReceived = [&](uint8_t *data, size_t len) {
        if (data && len) {
            String msg;
            msg.reserve(len);
            msg = String((char *)data, len);
            _stringMessageCallback(msg);
        }
    };
}

void AsyncWebSerial::setAuthentication(const String &username, const String &password)
{
    _username                 = username;
    _password                 = password;
    _isAuthenticationRequired = !_username.isEmpty() && !_password.isEmpty();

    // if the socket is already created, set the authentication immediately
    if (_socket != nullptr)
        _socket->setAuthentication(_username.c_str(), _password.c_str());
}

void AsyncWebSerial::loop()
{
    if (_socket)
        _socket->cleanupClients();
}

// Print interface implementation
size_t AsyncWebSerial::write(uint8_t m)
{
    if (!_socket)
        return 0;

    return write(&m, 1);
    return 1;
}

// Print interface implementation
size_t AsyncWebSerial::write(const uint8_t *buffer, size_t size)
{
    if (!_socket || size == 0)
        return 0;

    String message((const char *)buffer, size);
    _socket->textAll(message);
    return size;
}