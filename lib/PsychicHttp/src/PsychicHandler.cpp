#include "PsychicHandler.h"

PsychicHandler::PsychicHandler()
{
}

PsychicHandler::~PsychicHandler()
{
  delete _chain;
  // actual PsychicClient deletion handled by PsychicServer
  // for (PsychicClient *client : _clients)
  //   delete(client);
  _clients.clear();
}

PsychicHandler* PsychicHandler::addFilter(PsychicRequestFilterFunction fn)
{
  _filters.push_back(fn);
  return this;
}

bool PsychicHandler::filter(PsychicRequest* request)
{
  // run through our filter chain.
  for (auto& filter : _filters) {
    if (!filter(request)) {
      ESP_LOGD(PH_TAG, "Request %s refused by filter from handler", request->uri().c_str());
      return false;
    }
  }
  return true;
}

void PsychicHandler::setSubprotocol(const String& subprotocol)
{
  this->_subprotocol = subprotocol;
}
const char* PsychicHandler::getSubprotocol() const
{
  return _subprotocol.c_str();
}

PsychicClient* PsychicHandler::checkForNewClient(PsychicClient* client)
{
  PsychicClient* c = PsychicHandler::getClient(client);
  if (c == NULL) {
    c = client;
    addClient(c);
    c->isNew = true;
  } else
    c->isNew = false;

  return c;
}

void PsychicHandler::checkForClosedClient(PsychicClient* client)
{
  if (hasClient(client)) {
    closeCallback(client);
    removeClient(client);
  }
}

void PsychicHandler::addClient(PsychicClient* client)
{
  _clients.push_back(client);
}

void PsychicHandler::removeClient(PsychicClient* client)
{
  _clients.remove(client);
}

PsychicClient* PsychicHandler::getClient(int socket)
{
  // make sure the server has it too.
  if (!_server->hasClient(socket))
    return NULL;

  // what about us?
  for (PsychicClient* client : _clients)
    if (client->socket() == socket)
      return client;

  // nothing found.
  return NULL;
}

PsychicClient* PsychicHandler::getClient(PsychicClient* client)
{
  return PsychicHandler::getClient(client->socket());
}

bool PsychicHandler::hasClient(PsychicClient* socket)
{
  return PsychicHandler::getClient(socket) != NULL;
}

const std::list<PsychicClient*>& PsychicHandler::getClientList()
{
  return _clients;
}

PsychicHandler* PsychicHandler::addMiddleware(PsychicMiddleware* middleware)
{
  if (!_chain) {
    _chain = new PsychicMiddlewareChain();
  }
  _chain->addMiddleware(middleware);
  return this;
}

PsychicHandler* PsychicHandler::addMiddleware(PsychicMiddlewareCallback fn)
{
  if (!_chain) {
    _chain = new PsychicMiddlewareChain();
  }
  _chain->addMiddleware(fn);
  return this;
}

void PsychicHandler::removeMiddleware(PsychicMiddleware* middleware)
{
  if (_chain) {
    _chain->removeMiddleware(middleware);
  }
}

esp_err_t PsychicHandler::process(PsychicRequest* request)
{
  if (!filter(request)) {
    return HTTPD_404_NOT_FOUND;
  }

  if (!canHandle(request)) {
    ESP_LOGD(PH_TAG, "Request %s refused by handler", request->uri().c_str());
    return HTTPD_404_NOT_FOUND;
  }

  if (_chain) {
    return _chain->runChain(request, [this, request]() {
      return handleRequest(request, request->response());
    });

  } else {
    return handleRequest(request, request->response());
  }
}