#include "PsychicResponse.h"
#include "PsychicRequest.h"
#include <http_status.h>
#include <strings.h>

PsychicResponse::PsychicResponse(PsychicRequest* request) : _request(request),
                                                            _code(200),
                                                            _status(""),
                                                            _contentType(""),
                                                            _contentLength(0),
                                                            _body("")
{
  // get our global headers out of the way
  for (auto& header : DefaultHeaders::Instance().getHeaders())
    addHeader(header.field.c_str(), header.value.c_str());
}

PsychicResponse::~PsychicResponse()
{
  _headers.clear();
}

void PsychicResponse::addHeader(const char* field, const char* value)
{
  // erase any existing ones.
  for (auto itr = _headers.begin(); itr != _headers.end();) {
    if (strcasecmp(itr->field.c_str(), field) == 0)
      itr = _headers.erase(itr);
    else
      itr++;
  }

  // now add it.
  _headers.push_back({field, value});
}

void PsychicResponse::setCookie(const char* name, const char* value, unsigned long secondsFromNow, const char* extras)
{
  time_t now = time(nullptr);

  std::string output;
  output = std::string(urlEncode(name).c_str()) + "=" + urlEncode(value).c_str();

  // if current time isn't modern, default to using max age
  if (now < 1700000000) {
    output += "; Max-Age=";
    output += std::to_string(secondsFromNow);
  }
  // otherwise, set an expiration date
  else {
    time_t expirationTimestamp = now + secondsFromNow;

    // Convert the expiration timestamp to a formatted string for the "expires" attribute
    struct tm* tmInfo = gmtime(&expirationTimestamp);
    char expires[30];
    strftime(expires, sizeof(expires), "%a, %d %b %Y %H:%M:%S GMT", tmInfo);
    output += "; Expires=";
    output += expires;
  }

  // did we get any extras?
  if (strlen(extras)) {
    output += "; ";
    output += extras;
  }

  // okay, add it in.
  addHeader("Set-Cookie", output.c_str());
}

void PsychicResponse::setCode(int code)
{
  _code = code;
}

void PsychicResponse::setContentType(const char* contentType)
{
  _contentType = contentType;
}

void PsychicResponse::setContent(const char* content)
{
  _body = content;
  setContentLength(strlen(content));
}

void PsychicResponse::setContent(const uint8_t* content, size_t len)
{
  _body = (char*)content;
  setContentLength(len);
}

const char* PsychicResponse::getContent()
{
  return _body;
}

size_t PsychicResponse::getContentLength()
{
  return _contentLength;
}

esp_err_t PsychicResponse::send()
{
  if (!_code)
    setCode(200);
  // our headers too
  this->sendHeaders();

  // now send it off
  esp_err_t err = httpd_resp_send(_request->request(), getContent(), getContentLength());

  // did something happen?
  if (err != ESP_OK)
    ESP_LOGE(PH_TAG, "Send response failed (%s)", esp_err_to_name(err));

  return err;
}

void PsychicResponse::sendHeaders()
{
  // esp-idf makes you set the whole status.
  sprintf(_status, "%d %s", _code, http_status_reason(_code));
  httpd_resp_set_status(_request->request(), _status);

  // set the content type
  httpd_resp_set_type(_request->request(), _contentType.c_str());

  // now do our individual headers
  for (auto& header : _headers)
    httpd_resp_set_hdr(this->_request->request(), header.field.c_str(), header.value.c_str());
}

esp_err_t PsychicResponse::sendChunk(uint8_t* chunk, size_t chunksize)
{
  /* Send the buffer contents as HTTP response chunk */
  ESP_LOGD(PH_TAG, "Sending chunk: %d", chunksize);
  esp_err_t err = httpd_resp_send_chunk(request(), (char*)chunk, chunksize);
  if (err != ESP_OK) {
    ESP_LOGE(PH_TAG, "File sending failed (%s)", esp_err_to_name(err));

    /* Abort sending file */
    httpd_resp_sendstr_chunk(this->_request->request(), NULL);
  }

  return err;
}

esp_err_t PsychicResponse::finishChunking()
{
  /* Respond with an empty chunk to signal HTTP response completion */
  return httpd_resp_send_chunk(this->_request->request(), NULL, 0);
}

esp_err_t PsychicResponse::redirect(const char* url)
{
  // _code defaults to 200, so treat both 200 and an explicitly-cleared 0 as
  // "no explicit code chosen" and use 301. (Otherwise send() would emit a 200
  // with a Location header, which browsers do not follow.)
  if (!_code || _code == 200)
    setCode(301);
  addHeader("Location", url);
  return send();
}

esp_err_t PsychicResponse::send(int code)
{
  setCode(code);
  return send();
}

esp_err_t PsychicResponse::send(const char* content)
{
  if (!_code)
    setCode(200);
  if (_contentType.empty())
    setContentType("text/html");
  setContent(content);
  return send();
}

esp_err_t PsychicResponse::send(const char* contentType, const char* content)
{
  if (!_code)
    setCode(200);
  setContentType(contentType);
  setContent(content);
  return send();
}

esp_err_t PsychicResponse::send(int code, const char* contentType, const char* content)
{
  setCode(code);
  setContentType(contentType);
  setContent(content);
  return send();
}

esp_err_t PsychicResponse::send(int code, const char* contentType, const uint8_t* content, size_t len)
{
  setCode(code);
  setContentType(contentType);
  setContent(content, len);
  return send();
}

esp_err_t PsychicResponse::error(httpd_err_code_t code, const char* message)
{
  return httpd_resp_send_err(_request->_req, code, message);
}

httpd_req_t* PsychicResponse::request()
{
  return _request->_req;
}
