#ifndef PsychicClient_h
#define PsychicClient_h

#include "PsychicCore.h"

/*
 * PsychicClient :: Generic wrapper around the ESP-IDF socket
 */

class PsychicClient
{

  protected:
    httpd_handle_t _server;
    int _socket;

  public:
    PsychicClient(httpd_handle_t server, int socket);
    ~PsychicClient();

    // no idea if this is the right way to do it or not, but lets see.
    // pointer to our derived class (eg. PsychicWebSocketConnection)
    void* _friend;

    bool isNew = false;

    bool operator==(PsychicClient& rhs) const { return _socket == rhs.socket(); }

    httpd_handle_t server();
    int socket();
    esp_err_t close();

    IPAddress localIP();
    uint16_t localPort() const;
    IPAddress remoteIP();
    uint16_t remotePort() const;
};

#endif