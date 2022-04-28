#pragma once

#include "EthernetWebServer.h"

class AbstractWebServer
{
public:
    explicit AbstractWebServer(int port) {};

    virtual void begin() = 0;
    virtual bool authenticate(const char * username, const char * password) = 0;
    virtual void requestAuthentication(HTTPAuthMethod mode, const char* realm, const String& authFailMsg) = 0;
    virtual void requestAuthentication() = 0;
    virtual void send(int code, const char* content_type, const String& content) = 0;
    virtual void on(const Uri &uri, EthernetWebServer::THandlerFunction handler) = 0;
    virtual int args() = 0;
    virtual String arg(int i) = 0;
    virtual String argName(int i) = 0;
    virtual void handleClient() = 0;
};