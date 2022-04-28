#pragma once

#include <WebServer.h>
#include "AbstractWebServer.h"

class WifiWebServer : public AbstractWebServer
{
public:
    explicit WifiWebServer(int port);

    virtual void begin();
    virtual bool authenticate(const char *username, const char *password);
    virtual void requestAuthentication(int mode, const char *realm, const String &authFailMsg);
    virtual void requestAuthentication();
    virtual void send(int code, const char *content_type, const String &content);
    virtual void on(const Uri &uri, WebServer::THandlerFunction handler);
    virtual int args();
    virtual String arg(int i);
    virtual String argName(int i);
    virtual void handleClient();

private:
    WebServer _server;
};