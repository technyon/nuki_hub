#include "EthWebServer.h"


EthWebServer::EthWebServer(int port)
: AbstractWebServer(port),
  _server(port)
{}


void EthWebServer::begin()
{
    _server.begin();
}

bool EthWebServer::authenticate(const char *username, const char *password)
{
    return _server.authenticate(username, password);
}

void EthWebServer::requestAuthentication(HTTPAuthMethod mode, const char *realm, const String &authFailMsg)
{
    return _server.requestAuthentication(mode, realm, authFailMsg);
}

void EthWebServer::send(int code, const char *content_type, const String &content)
{
    _server.send(code, content_type, content);
}

void EthWebServer::on(const Uri &uri, EthernetWebServer::THandlerFunction handler)
{
    _server.on(uri, handler);
}

void EthWebServer::requestAuthentication()
{

}

int EthWebServer::args()
{
    return _server.args();
}

String EthWebServer::arg(int i)
{
    return _server.arg(i);
}

String EthWebServer::argName(int i)
{
    return _server.argName(i);
}

void EthWebServer::handleClient()
{
    _server.handleClient();
}
