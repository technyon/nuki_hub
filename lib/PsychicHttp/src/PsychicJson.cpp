#include "PsychicJson.h"

#ifdef ARDUINOJSON_6_COMPATIBILITY
  PsychicJsonResponse::PsychicJsonResponse(PsychicRequest *request, bool isArray, size_t maxJsonBufferSize) :
    PsychicResponse(request),
    _jsonBuffer(maxJsonBufferSize)
  {
    setContentType(JSON_MIMETYPE);
    if (isArray)
      _root = _jsonBuffer.createNestedArray();
    else
      _root = _jsonBuffer.createNestedObject();
  }
#else
  PsychicJsonResponse::PsychicJsonResponse(PsychicRequest *request, bool isArray) : PsychicResponse(request)
  {
    setContentType(JSON_MIMETYPE);
    if (isArray)
      _root = _jsonBuffer.add<JsonArray>();
    else
      _root = _jsonBuffer.add<JsonObject>();
  }
#endif

JsonVariant &PsychicJsonResponse::getRoot() { return _root; }

size_t PsychicJsonResponse::getLength()
{
  return measureJson(_root);
}

esp_err_t PsychicJsonResponse::send()
{
  esp_err_t err = ESP_OK;
  size_t length = getLength();
  size_t buffer_size;
  char *buffer;

  //how big of a buffer do we want?
  if (length < JSON_BUFFER_SIZE)
    buffer_size = length+1;
  else
    buffer_size = JSON_BUFFER_SIZE;

  buffer = (char *)malloc(buffer_size);
  if (buffer == NULL) {
    httpd_resp_send_err(this->_request->request(), HTTPD_500_INTERNAL_SERVER_ERROR, "Unable to allocate memory.");
    return ESP_FAIL;
  }

  //send it in one shot or no?
  if (length < JSON_BUFFER_SIZE)
  {
    serializeJson(_root, buffer, buffer_size);

    this->setContent((uint8_t *)buffer, length);
    this->setContentType(JSON_MIMETYPE);

    err = PsychicResponse::send();
  }
  else
  {
    //helper class that acts as a stream to print chunked responses
    ChunkPrinter dest(this, (uint8_t *)buffer, buffer_size);

    //keep our headers
    this->sendHeaders();

    serializeJson(_root, dest);

    //send the last bits
    dest.flush();

    //done with our chunked response too
    err = this->finishChunking();
  }

  //let the buffer go
  free(buffer);

  return err;
}

#ifdef ARDUINOJSON_6_COMPATIBILITY
  PsychicJsonHandler::PsychicJsonHandler(size_t maxJsonBufferSize) :
    _onRequest(NULL),
    _maxJsonBufferSize(maxJsonBufferSize)
  {};

  PsychicJsonHandler::PsychicJsonHandler(PsychicJsonRequestCallback onRequest, size_t maxJsonBufferSize) :
    _onRequest(onRequest),
    _maxJsonBufferSize(maxJsonBufferSize)
  {}
#else
  PsychicJsonHandler::PsychicJsonHandler() :
    _onRequest(NULL)
  {};

  PsychicJsonHandler::PsychicJsonHandler(PsychicJsonRequestCallback onRequest) :
    _onRequest(onRequest)
  {}
#endif

void PsychicJsonHandler::onRequest(PsychicJsonRequestCallback fn) { _onRequest = fn; }

esp_err_t PsychicJsonHandler::handleRequest(PsychicRequest *request)
{
  //process basic stuff
  PsychicWebHandler::handleRequest(request);

  if (_onRequest)
  {
    #ifdef ARDUINOJSON_6_COMPATIBILITY
      DynamicJsonDocument jsonBuffer(this->_maxJsonBufferSize);
      DeserializationError error = deserializeJson(jsonBuffer, request->body());
      if (error)
        return request->reply(400);

      JsonVariant json = jsonBuffer.as<JsonVariant>();
    #else
      JsonDocument jsonBuffer;
      DeserializationError error = deserializeJson(jsonBuffer, request->body());
      if (error)
        return request->reply(400);

      JsonVariant json = jsonBuffer.as<JsonVariant>();
    #endif

    return _onRequest(request, json);
  }
  else
    return request->reply(500);
}