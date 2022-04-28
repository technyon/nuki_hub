#include "WifiWebServer.h"

WifiWebServer::WifiWebServer(int port)
        : AbstractWebServer(port),
          _server(port)
{}


void WifiWebServer::begin()
{
    _server.begin();
}

bool WifiWebServer::authenticate(const char *username, const char *password)
{
    return _server.authenticate(username, password);
}

void WifiWebServer::requestAuthentication(int mode, const char *realm, const String &authFailMsg)
{
    return _server.requestAuthentication((HTTPAuthMethod)mode, realm, authFailMsg);
}

void WifiWebServer::send(int code, const char *content_type, const String &content)
{
    _server.send(code, content_type, content);
}

void WifiWebServer::on(const Uri &uri, WebServer::THandlerFunction handler)
{
    _server.on(uri, handler);
}

void WifiWebServer::requestAuthentication()
{

}

int WifiWebServer::args()
{
    return _server.args();
}

String WifiWebServer::arg(int i)
{
    return _server.arg(i);
}

String WifiWebServer::argName(int i)
{
    return _server.argName(i);
}

void WifiWebServer::handleClient()
{
    _server.handleClient();
}
