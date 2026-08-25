#include "PsychicMiddlewares.h"

#ifdef ARDUINO
void LoggingMiddleware::setOutput(Print& output)
{
  _out = &output;
}

esp_err_t LoggingMiddleware::run(PsychicRequest* request, PsychicResponse* response, PsychicMiddlewareNext next)
{
  _out->print("* Connection from ");
  _out->print(request->client()->remoteIP().toString());
  _out->print(":");
  _out->println(request->client()->remotePort());

  _out->print("> ");
  _out->print(request->methodStr());
  _out->print(" ");
  _out->print(request->uri());
  _out->print(" ");
  _out->println(request->version());

  // TODO: find a way to collect all headers
  // int n = request->headerCount();
  //  for (int i = 0; i < n; i++) {
  //    String v = server.header(i);
  //    if (!v.isEmpty()) {
  //      // because these 2 are always there, eventually empty: "Authorization", "If-None-Match"
  //      _out->print("< ");
  //      _out->print(server.headerName(i));
  //      _out->print(": ");
  //      _out->println(server.header(i));
  //    }
  //  }

  _out->println(">");

  esp_err_t ret = next();

  if (ret != HTTPD_404_NOT_FOUND) {
    _out->println("* Processed!");

    _out->print("< ");
    _out->print(response->version());
    _out->print(" ");
    _out->print(response->getCode());
    _out->print(" ");
    _out->println(http_status_reason(response->getCode()));

    // iterate over response->headers()
    std::list<HTTPHeader>::iterator it = response->headers().begin();
    while (it != response->headers().end()) {
      HTTPHeader h = *it;
      _out->print("< ");
      _out->print(h.field.c_str());
      _out->print(": ");
      _out->println(h.value.c_str());
      it++;
    }

    _out->println("<");

  } else {
    _out->println("* Not processed!");
  }

  return ret;
}
#else
esp_err_t LoggingMiddleware::run(PsychicRequest* request, PsychicResponse* response, PsychicMiddlewareNext next)
{
  esp_ip4_addr_t ip = request->client()->remoteIP();
  char ipstr[16];
  snprintf(ipstr, sizeof(ipstr), "%d.%d.%d.%d", (int)(ip.addr & 0xFF), (int)((ip.addr >> 8) & 0xFF), (int)((ip.addr >> 16) & 0xFF), (int)((ip.addr >> 24) & 0xFF));
  ESP_LOGI(PH_TAG, "* Connection from %s:%u", ipstr, (unsigned)request->client()->remotePort());
  ESP_LOGI(PH_TAG, "> %s %s %s", request->methodStr(), request->uri(), request->version());

  esp_err_t ret = next();

  if (ret != HTTPD_404_NOT_FOUND) {
    ESP_LOGI(PH_TAG, "* Processed!");
    ESP_LOGI(PH_TAG, "< %s %d %s", response->version(), response->getCode(), http_status_reason(response->getCode()));
    std::list<HTTPHeader>::iterator it = response->headers().begin();
    while (it != response->headers().end()) {
      HTTPHeader h = *it;
      ESP_LOGI(PH_TAG, "< %s: %s", h.field.c_str(), h.value.c_str());
      it++;
    }
  } else {
    ESP_LOGI(PH_TAG, "* Not processed!");
  }

  return ret;
}
#endif

AuthenticationMiddleware& AuthenticationMiddleware::setUsername(const char* username)
{
  _username = username;
  return *this;
}

AuthenticationMiddleware& AuthenticationMiddleware::setPassword(const char* password)
{
  _password = password;
  return *this;
}

AuthenticationMiddleware& AuthenticationMiddleware::setRealm(const char* realm)
{
  _realm = realm;
  return *this;
}

AuthenticationMiddleware& AuthenticationMiddleware::setAuthMethod(HTTPAuthMethod method)
{
  _method = method;
  return *this;
}

AuthenticationMiddleware& AuthenticationMiddleware::setAuthFailureMessage(const char* message)
{
  _authFailMsg = message;
  return *this;
}

bool AuthenticationMiddleware::isAllowed(PsychicRequest* request) const
{
  if (!_username.empty() && !_password.empty()) {
    return request->authenticate(_username.c_str(), _password.c_str());
  }

  return true;
}

esp_err_t AuthenticationMiddleware::run(PsychicRequest* request, PsychicResponse* response, PsychicMiddlewareNext next)
{
  bool authenticationRequired = false;

  if (!_username.empty() && !_password.empty()) {
    authenticationRequired = !request->authenticate(_username.c_str(), _password.c_str());
  }

  if (authenticationRequired) {
    return request->requestAuthentication(_method, _realm.c_str(), _authFailMsg.c_str());
  } else {
    return next();
  }
}

CorsMiddleware& CorsMiddleware::setOrigin(const char* origin)
{
  _origin = origin;
  return *this;
}

CorsMiddleware& CorsMiddleware::setMethods(const char* methods)
{
  _methods = methods;
  return *this;
}

CorsMiddleware& CorsMiddleware::setHeaders(const char* headers)
{
  _headers = headers;
  return *this;
}

CorsMiddleware& CorsMiddleware::setAllowCredentials(bool credentials)
{
  _credentials = credentials;
  return *this;
}

CorsMiddleware& CorsMiddleware::setMaxAge(uint32_t seconds)
{
  _maxAge = seconds;
  return *this;
}

void CorsMiddleware::addCORSHeaders(PsychicResponse* response)
{
  response->addHeader("Access-Control-Allow-Origin", _origin.c_str());
  response->addHeader("Access-Control-Allow-Methods", _methods.c_str());
  response->addHeader("Access-Control-Allow-Headers", _headers.c_str());
  response->addHeader("Access-Control-Allow-Credentials", _credentials ? "true" : "false");
  response->addHeader("Access-Control-Max-Age", std::to_string(_maxAge).c_str());
}

esp_err_t CorsMiddleware::run(PsychicRequest* request, PsychicResponse* response, PsychicMiddlewareNext next)
{
  if (request->hasHeader("Origin")) {
    addCORSHeaders(response);
    if (request->method() == HTTP_OPTIONS) {
      return response->send(200);
    }
  }
  return next();
}
