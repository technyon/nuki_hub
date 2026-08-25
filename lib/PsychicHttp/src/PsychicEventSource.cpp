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
#include <string.h>

/*****************************************/
// PsychicEventSource - Handler
/*****************************************/

PsychicEventSource::PsychicEventSource() : PsychicHandler(),
                                           _onOpen(NULL),
                                           _onClose(NULL)
{
}

PsychicEventSource::~PsychicEventSource()
{
}

PsychicEventSourceClient* PsychicEventSource::getClient(int socket)
{
  PsychicClient* client = PsychicHandler::getClient(socket);

  if (client == NULL)
    return NULL;

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
  esp_err_t err = response.send();

  // lookup our client
  PsychicClient* client = checkForNewClient(request->client());
  if (client->isNew) {
    // did we get our last id?
    if (request->hasHeader("Last-Event-ID")) {
      PsychicEventSourceClient* buddy = getClient(client);
      buddy->_lastId = atoi(request->header("Last-Event-ID").c_str());
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
  PsychicHandler::removeClient(client);
  delete (PsychicEventSourceClient*)client->_friend;
  client->_friend = NULL;
}

void PsychicEventSource::openCallback(PsychicClient* client)
{
  PsychicEventSourceClient* buddy = getClient(client);
  if (buddy == NULL) {
    return;
  }

  if (_onOpen != NULL)
    _onOpen(buddy);
}

void PsychicEventSource::closeCallback(PsychicClient* client)
{
  PsychicEventSourceClient* buddy = getClient(client);
  if (buddy == NULL) {
    return;
  }

  if (_onClose != NULL)
    _onClose(getClient(buddy));
}

void PsychicEventSource::send(const char* message, const char* event, uint32_t id, uint32_t reconnect)
{
  String ev = generateEventMessage(message, event, id, reconnect);
  for(PsychicClient *c : _clients) {
    if (c && c->_friend) {
      ((PsychicEventSourceClient*)c->_friend)->sendEvent(ev.c_str());
    }  
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

void PsychicEventSourceClient::send(const char* message, const char* event, uint32_t id, uint32_t reconnect)
{
  String ev = generateEventMessage(message, event, id, reconnect);
  sendEvent(ev.c_str());
}

void PsychicEventSourceClient::sendEvent(const char* event)
{
  _sendEventAsync(this->server(), this->socket(), event, strlen(event));
}

esp_err_t PsychicEventSourceClient::_sendEventAsync(httpd_handle_t handle, int socket, const char* event, size_t len)
{
  // create the transfer object
  async_event_transfer_t* transfer = (async_event_transfer_t*)calloc(1, sizeof(async_event_transfer_t));
  if (transfer == NULL) {
    return ESP_ERR_NO_MEM;
  }

  // populate it
  transfer->arg = this;
  transfer->callback = _sendEventSentCallback;
  transfer->handle = handle;
  transfer->socket = socket;
  transfer->len = len;

  // allocate for event text
  transfer->event = (char*)malloc(len);
  if (transfer->event == NULL) {
    free(transfer);
    return ESP_ERR_NO_MEM;
  }

  // copy over the event data
  memcpy(transfer->event, event, len);

  // queue it.
  esp_err_t err = httpd_queue_work(handle, _sendEventWorkCallback, transfer);

  // cleanup
  if (err) {
    free(transfer->event);
    free(transfer);
    return err;
  }

  return ESP_OK;
}

void PsychicEventSourceClient::_sendEventWorkCallback(void* arg)
{
  async_event_transfer_t* trans = (async_event_transfer_t*)arg;

  // omg the error is overloaded with the number of bytes sent!
  esp_err_t err = httpd_socket_send(trans->handle, trans->socket, trans->event, trans->len, 0);
  if (err == trans->len)
    err = ESP_OK;

  if (trans->callback)
    trans->callback(err, trans->socket, trans->arg);

  // free our memory
  free(trans->event);
  free(trans);
}

void PsychicEventSourceClient::_sendEventSentCallback(esp_err_t err, int socket, void* arg)
{
  // PsychicEventSourceClient* client = (PsychicEventSourceClient*)arg;

  if (err == ESP_OK)
    return;
  else if (err == ESP_FAIL)
    ESP_LOGE(PH_TAG, "EventSource: send - socket error (#%d)", socket);
  else if (err == ESP_ERR_INVALID_STATE)
    ESP_LOGE(PH_TAG, "EventSource: Handshake was already done beforehand (#%d)", socket);
  else if (err == ESP_ERR_INVALID_ARG)
    ESP_LOGE(PH_TAG, "EventSource: Argument is invalid (#%d)", socket);
  else if (err == HTTPD_SOCK_ERR_TIMEOUT)
    ESP_LOGE(PH_TAG, "EventSource: Socket timeout (#%d)", socket);
  else if (err == HTTPD_SOCK_ERR_INVALID)
    ESP_LOGE(PH_TAG, "EventSource: Invalid socket (#%d)", socket);
  else if (err == HTTPD_SOCK_ERR_FAIL)
    ESP_LOGE(PH_TAG, "EventSource: Socket fail (#%d)", socket);
  else
    ESP_LOGE(PH_TAG, "EventSource: %#06x %s (#%d)", (int)err, esp_err_to_name(err), socket);
}

/*****************************************/
// PsychicEventSourceResponse
/*****************************************/

PsychicEventSourceResponse::PsychicEventSourceResponse(PsychicResponse* response) : PsychicResponseDelegate(response)
{
}

esp_err_t PsychicEventSourceResponse::send()
{
  _response->addHeader("Content-Type", "text/event-stream");
  _response->addHeader("Cache-Control", "no-cache");
  _response->addHeader("Connection", "keep-alive");

  // build our main header
  String out = String();
  out.concat("HTTP/1.1 200 OK\r\n");

  // get our global headers out of the way first
  for (auto& header : DefaultHeaders::Instance().getHeaders())
    out.concat(header.field + ": " + header.value + "\r\n");

  // now do our individual headers
  for (auto& header : _response->headers())
    out.concat(header.field + ": " + header.value + "\r\n");

  // separator
  out.concat("\r\n");

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

String generateEventMessage(const char* message, const char* event, uint32_t id, uint32_t reconnect)
{
  String ev = "";

  if (reconnect) {
    ev += "retry: ";
    ev += String(reconnect);
    ev += "\r\n";
  }

  if (id) {
    ev += "id: ";
    ev += String(id);
    ev += "\r\n";
  }

  if (event != NULL) {
    ev += "event: ";
    ev += String(event);
    ev += "\r\n";
  }

  if (message != NULL) {
    ev += "data: ";
    ev += String(message);
    ev += "\r\n";
  }
  ev += "\r\n";

  return ev;
}