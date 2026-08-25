/*
  Asynchronous WebServer library for Espressif MCUs

  Copyright (c) 2016 Hristo Gochkov. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "PsychicEventSource.h"
#include "esp_log.h"
#include <vector>

/*****************************************/
// PsychicEventSource - Handler
/*****************************************/

PsychicEventSource::PsychicEventSource() : PsychicHandler(),
                                           _onOpen(nullptr),
                                           _onClose(nullptr)
{
}

PsychicEventSource::~PsychicEventSource()
{
}

PsychicEventSourceClient* PsychicEventSource::getClient(int socket)
{
  PsychicClient* client = PsychicHandler::getClient(socket);

  if (client == nullptr)
    return nullptr;

  return (PsychicEventSourceClient*)client->_friend;
}

PsychicEventSourceClient* PsychicEventSource::getClient(PsychicClient* client)
{
  return getClient(client->socket());
}

esp_err_t PsychicEventSource::handleRequest(PsychicRequest* request, PsychicResponse* resp)
{
  // start our open ended HTTP response
  PsychicEventSourceResponse response(resp);
  response.addHeader("Content-Type", "text/event-stream");
  response.addHeader("Cache-Control", "no-cache");
  response.addHeader("Connection", "keep-alive");
  esp_err_t err = response.send();

  // lookup our client
  PsychicClient* client = checkForNewClient(request->client());
  if (client->isNew) {
    // did we get our last id?
    if (request->hasHeader("Last-Event-ID")) {
      PsychicEventSourceClient* buddy = getClient(client);
      buddy->_lastId = atoi(request->headerCStr("Last-Event-ID"));
    }

    // let our handler know.
    openCallback(client);
  }

  return err;
}

PsychicEventSource* PsychicEventSource::onOpen(PsychicEventSourceClientCallback fn)
{
  _onOpen = fn;
  return this;
}

PsychicEventSource* PsychicEventSource::onClose(PsychicEventSourceClientCallback fn)
{
  _onClose = fn;
  return this;
}

void PsychicEventSource::addClient(PsychicClient* client)
{
  client->_friend = new PsychicEventSourceClient(client);
  PsychicHandler::addClient(client);
}

void PsychicEventSource::removeClient(PsychicClient* client)
{
  auto buddy = static_cast<PsychicEventSourceClient*>(client->_friend);
  if (buddy) {
    delete buddy;
    client->_friend = nullptr;
  }
  PsychicHandler::removeClient(client);
}

void PsychicEventSource::openCallback(PsychicClient* client)
{
  PsychicEventSourceClient* buddy = getClient(client);
  if (buddy == nullptr) {
    return;
  }

  if (_onOpen != nullptr)
    _onOpen(buddy);
}

void PsychicEventSource::closeCallback(PsychicClient* client)
{
  PsychicEventSourceClient* buddy = getClient(client);
  if (buddy == nullptr) {
    return;
  }

  if (_onClose != nullptr)
    _onClose(getClient(buddy));
}

/**
 * @brief Sends an event to all connected clients.
 * * This function now safely handles client disconnections.
 * It iterates through all clients, attempts to send the event, and collects
 * any clients for whom the send fails. It then properly removes these
 * disconnected clients after the loop, preventing a crash from using a stale handle.
 */
void PsychicEventSource::send(const char* message, const char* event, uint32_t id, uint32_t reconnect)
{
  auto ev = generateEventMessage(message, event, id, reconnect);
  std::vector<PsychicClient*> clientsToRemove;

  // First, iterate and send, collecting disconnected clients
  for (PsychicClient* c : _clients) {
    if (!((PsychicEventSourceClient*)c->_friend)->sendEvent(ev.c_str())) {
      clientsToRemove.push_back(c);
    }
  }

  // Second, iterate through the disconnected clients and clean them up
  for (PsychicClient* c : clientsToRemove) {
    closeCallback(c); // Let the user application know
    removeClient(c);  // Remove from handler and clean up memory
  }
}

/*****************************************/
// PsychicEventSourceClient
/*****************************************/

PsychicEventSourceClient::PsychicEventSourceClient(PsychicClient* client) : PsychicClient(client->server(), client->socket()),
                                                                            _lastId(0)
{
}

PsychicEventSourceClient::~PsychicEventSourceClient()
{
}

/**
 * @brief Returns a boolean indicating send success.
 */
bool PsychicEventSourceClient::send(const char* message, const char* event, uint32_t id, uint32_t reconnect)
{
  auto ev = generateEventMessage(message, event, id, reconnect);
  return sendEvent(ev.c_str());
}

/**
 * @brief Sends data and returns true on success, false on failure.
 * This prevents a crash by detecting if the underlying socket is closed.
 */
bool PsychicEventSourceClient::sendEvent(const char* event)
{
  int result;
  do {
    result = httpd_socket_send(this->server(), this->socket(), event, strlen(event), 0);
  } while (result == HTTPD_SOCK_ERR_TIMEOUT);

  if (result < 0) {
    ESP_LOGD(PH_TAG, "sendEvent to socket %d failed. Client likely disconnected.", this->socket());
    return false;
  }
  return true;
}

/*****************************************/
// PsychicEventSourceResponse
/*****************************************/

PsychicEventSourceResponse::PsychicEventSourceResponse(PsychicResponse* response) : PsychicResponseDelegate(response)
{
}

esp_err_t PsychicEventSourceResponse::send()
{
  std::string out = "HTTP/1.1 200 OK\r\n";

  // now do our individual headers
  for (auto& header : _response->headers()) {
    out += header.field.c_str();
    out += ": ";
    out += header.value.c_str();
    out += "\r\n";
  }

  // separator
  out += "\r\n";

  int result;
  do {
    result = httpd_send(request(), out.c_str(), out.length());
  } while (result == HTTPD_SOCK_ERR_TIMEOUT);

  if (result < 0)
    ESP_LOGE(PH_TAG, "EventSource send failed with %s", esp_err_to_name(result));

  if (result > 0)
    return ESP_OK;
  else
    return ESP_ERR_HTTPD_RESP_SEND;
}

/*****************************************/
// Event Message Generator
/*****************************************/

static std::string _generateEventMessage_impl(const char* message, const char* event, uint32_t id, uint32_t reconnect)
{
  std::string ev;

  if (reconnect) {
    ev += "retry: ";
    ev += std::to_string(reconnect);
    ev += "\r\n";
  }

  if (id) {
    ev += "id: ";
    ev += std::to_string(id);
    ev += "\r\n";
  }

  if (event != NULL) {
    ev += "event: ";
    ev += event;
    ev += "\r\n";
  }

  if (message != NULL) {
    ev += "data: ";
    ev += message;
    ev += "\r\n";
  }
  ev += "\r\n";
  return ev;
}

#ifdef ARDUINO
String generateEventMessage(const char* message, const char* event, uint32_t id, uint32_t reconnect)
{
  return _generateEventMessage_impl(message, event, id, reconnect).c_str();
}
#else
std::string generateEventMessage(const char* message, const char* event, uint32_t id, uint32_t reconnect)
{
  return _generateEventMessage_impl(message, event, id, reconnect);
}
#endif
