#include "PsychicRequest.h"
#include "MultipartProcessor.h"
#include "PsychicHttpServer.h"
#include "http_status.h"
#include <mbedtls/version.h>

PsychicRequest::PsychicRequest(PsychicHttpServer* server, httpd_req_t* req) : _server(server),
                                                                              _req(req),
                                                                              _endpoint(nullptr),
                                                                              _method(HTTP_GET),
                                                                              _uri(""),
                                                                              _query(""),
                                                                              _body(""),
                                                                              _tempObject(nullptr)
{
  // load up our client.
  this->_client = server->getClient(req);

  // handle our session data
  if (req->sess_ctx != NULL)
    this->_session = (SessionData*)req->sess_ctx;
  else {
    this->_session = new SessionData();
    req->sess_ctx = this->_session;
  }

  // callback for freeing the session later
  req->free_ctx = this->freeSession;

  // load and parse our uri.
  this->_setUri(this->_req->uri);

  _response = new PsychicResponse(this);
}

PsychicRequest::~PsychicRequest()
{
  // temorary user object
  if (_tempObject != NULL)
    free(_tempObject);

  // our web parameters
  for (auto* param : _params)
    delete (param);
  _params.clear();

  delete _response;
}

void PsychicRequest::freeSession(void* ctx)
{
  if (ctx != NULL) {
    SessionData* session = (SessionData*)ctx;
    delete session;
  }
}

PsychicHttpServer* PsychicRequest::server()
{
  return _server;
}

httpd_req_t* PsychicRequest::request()
{
  return _req;
}

PsychicClient* PsychicRequest::client()
{
  return _client;
}

PsychicEndpoint* PsychicRequest::endpoint()
{
  return _endpoint;
}

void PsychicRequest::setEndpoint(PsychicEndpoint* endpoint)
{
  _endpoint = endpoint;
}

#ifdef PSY_ENABLE_REGEX
bool PsychicRequest::getRegexMatches(std::smatch& matches, bool use_full_uri)
{
  if (_endpoint != nullptr) {
    std::regex pattern(_endpoint->uriCStr());
    std::string s(this->pathCStr());
    if (use_full_uri)
      s = this->uriCStr();

    return std::regex_search(s, matches, pattern);
  }

  return false;
}
#endif

#ifdef ARDUINO
String PsychicRequest::getFilename()
{
  // parse the content-disposition header
  if (this->hasHeader("Content-Disposition")) {
    ContentDisposition cd = this->getContentDisposition();
    if (cd.filename.length() > 0)
      return cd.filename;
  }

  // fall back to passed in query string
  PsychicWebParameter* param = getParam("_filename");
  if (param != NULL)
    return param->name();

  // fall back to parsing it from url (useful for wildcard uploads)
  const char* u = _uri.c_str();
  const char* slash = strrchr(u, '/');
  std::string fname = slash ? slash + 1 : u;
  if (!fname.empty())
    return String(fname.c_str());

  // finally, unknown.
  ESP_LOGE(PH_TAG, "Did not get a valid filename from the upload.");
  return String("unknown.txt");
}
#else
const char* PsychicRequest::getFilename()
{
  // parse the content-disposition header
  if (this->hasHeader("Content-Disposition")) {
    ContentDisposition cd = this->getContentDisposition();
    if (!cd.filename.empty()) {
      _filename = cd.filename;
      return _filename.c_str();
    }
  }

  // fall back to passed in query string
  PsychicWebParameter* param = getParam("_filename");
  if (param != NULL)
    return param->name();

  // fall back to parsing it from url (useful for wildcard uploads)
  const char* u = _uri.c_str();
  const char* slash = strrchr(u, '/');
  _filename = slash ? slash + 1 : u;
  if (!_filename.empty())
    return _filename.c_str();

  // finally, unknown.
  ESP_LOGE(PH_TAG, "Did not get a valid filename from the upload.");
  return "unknown.txt";
}
#endif

const char* PsychicRequest::getFilenameCStr()
{
  // parse the content-disposition header
  if (this->hasHeader("Content-Disposition")) {
    ContentDisposition cd = this->getContentDisposition();
    if (cd.filename.length() > 0) {
      _filename = cd.filename.c_str();
      return _filename.c_str();
    }
  }

  // fall back to passed in query string
  PsychicWebParameter* param = getParam("_filename");
  if (param != NULL)
    return param->nameCStr();

  // fall back to parsing it from url (useful for wildcard uploads)
  const char* u = _uri.c_str();
  const char* slash = strrchr(u, '/');
  _filename = slash ? slash + 1 : u;
  if (!_filename.empty())
    return _filename.c_str();

  // finally, unknown.
  ESP_LOGE(PH_TAG, "Did not get a valid filename from the upload.");
  return "unknown.txt";
}

const ContentDisposition PsychicRequest::getContentDisposition()
{
  ContentDisposition cd;
  std::string hdr(this->_getHeader("Content-Disposition"));
  size_t start, end;

  if (hdr.compare(0, 9, "form-data") == 0)
    cd.disposition = FORM_DATA;
  else if (hdr.compare(0, 10, "attachment") == 0)
    cd.disposition = ATTACHMENT;
  else if (hdr.compare(0, 6, "inline") == 0)
    cd.disposition = INLINE;
  else
    cd.disposition = NONE;

  start = hdr.find("filename=");
  if (start != std::string::npos) {
    end = hdr.find('"', start + 10);
    if (end != std::string::npos && end > start + 10)
#ifdef ARDUINO
      cd.filename = psychicSubstr(hdr, start + 10, end - start - 11).c_str();
#else
      cd.filename = psychicSubstr(hdr, start + 10, end - start - 11);
#endif
  }

  start = hdr.find("name=");
  if (start != std::string::npos) {
    end = hdr.find('"', start + 6);
    if (end != std::string::npos && end > start + 6)
#ifdef ARDUINO
      cd.name = psychicSubstr(hdr, start + 6, end - start - 7).c_str();
#else
      cd.name = psychicSubstr(hdr, start + 6, end - start - 7);
#endif
  }

  return cd;
}

esp_err_t PsychicRequest::loadBody()
{
  if (_bodyParsed != ESP_ERR_NOT_FINISHED)
    return _bodyParsed;

  // quick size check.
  if (contentLength() > server()->maxRequestBodySize) {
    ESP_LOGE(PH_TAG, "Body size larger than maxRequestBodySize");
    return _bodyParsed = ESP_ERR_INVALID_SIZE;
  }

  this->_body.clear();

  size_t remaining = this->_req->content_len;
  size_t actuallyReceived = 0;
  char* buf = (char*)malloc(remaining + 1);
  if (buf == NULL) {
    ESP_LOGE(PH_TAG, "Failed to allocate memory for body");
    return _bodyParsed = ESP_FAIL;
  }

  while (remaining > 0) {
    int received = httpd_req_recv(this->_req, buf + actuallyReceived, remaining);

    if (received == HTTPD_SOCK_ERR_TIMEOUT) {
      continue;
    } else if (received == HTTPD_SOCK_ERR_FAIL) {
      ESP_LOGE(PH_TAG, "Failed to receive data.");
      _bodyParsed = ESP_FAIL;
      break;
    }

    remaining -= received;
    actuallyReceived += received;
  }

  buf[actuallyReceived] = '\0';
  this->_body = buf;
  free(buf);

  _bodyParsed = ESP_OK;

  return _bodyParsed;
}

http_method PsychicRequest::method()
{
  return (http_method)this->_req->method;
}

#ifdef ARDUINO
String PsychicRequest::methodStr()
{
  return String(http_method_str((http_method)this->_req->method));
}

String PsychicRequest::path()
{
  size_t index = _uri.find('?');
  if (index == std::string::npos)
    return String(_uri.c_str());
  return String(_uri.substr(0, index).c_str());
}

String PsychicRequest::uri()
{
  return String(_uri.c_str());
}

String PsychicRequest::query()
{
  return String(_query.c_str());
}
#else
const char* PsychicRequest::methodStr()
{
  _tmp = http_method_str((http_method)this->_req->method);
  return _tmp.c_str();
}

const char* PsychicRequest::path()
{
  size_t index = _uri.find('?');
  if (index == std::string::npos)
    _tmp = _uri;
  else
    _tmp = _uri.substr(0, index);
  return _tmp.c_str();
}

const char* PsychicRequest::uri()
{
  return _uri.c_str();
}

const char* PsychicRequest::query()
{
  return _query.c_str();
}
#endif

const char* PsychicRequest::methodStrCStr()
{
  _tmp = http_method_str((http_method)this->_req->method);
  return _tmp.c_str();
}

const char* PsychicRequest::pathCStr()
{
  size_t index = _uri.find('?');
  if (index == std::string::npos)
    _tmp = _uri;
  else
    _tmp = _uri.substr(0, index);
  return _tmp.c_str();
}

const char* PsychicRequest::uriCStr()
{
  return _uri.c_str();
}

const char* PsychicRequest::queryCStr()
{
  return _query.c_str();
}

// no way to get list of headers yet....
// int PsychicRequest::headers()
// {
// }

const char* PsychicRequest::_getHeader(const char* name)
{
  size_t header_len = httpd_req_get_hdr_value_len(this->_req, name);

  // if we've got one, allocated it and load it
  if (header_len) {
    _tmp.resize(header_len + 1);
    httpd_req_get_hdr_value_str(this->_req, name, &_tmp[0], header_len + 1);
    _tmp.resize(header_len);
  } else {
    _tmp.clear();
  }
  return _tmp.c_str();
}

#ifdef ARDUINO
String PsychicRequest::header(const char* name)
{
  return String(_getHeader(name));
}
#else
const char* PsychicRequest::header(const char* name)
{
  return _getHeader(name);
}
#endif

const char* PsychicRequest::headerCStr(const char* name)
{
  return _getHeader(name);
}

bool PsychicRequest::hasHeader(const char* name)
{
  return httpd_req_get_hdr_value_len(this->_req, name) > 0;
}

#ifdef ARDUINO
String PsychicRequest::host()
{
  return String(_getHeader("Host"));
}

String PsychicRequest::contentType()
{
  return String(_getHeader("Content-Type"));
}
#else
const char* PsychicRequest::host()
{
  return _getHeader("Host");
}

const char* PsychicRequest::contentType()
{
  return _getHeader("Content-Type");
}
#endif

size_t PsychicRequest::contentLength()
{
  return this->_req->content_len;
}

#ifdef ARDUINO
String PsychicRequest::body()
{
  return String(_body.c_str());
}
#else
const char* PsychicRequest::body()
{
  return _body.c_str();
}
#endif

const char* PsychicRequest::bodyCStr()
{
  return _body.c_str();
}

bool PsychicRequest::isMultipart()
{
  return strstr(this->_getHeader("Content-Type"), "multipart/form-data") != nullptr;
}

bool PsychicRequest::hasCookie(const char* key, size_t* size)
{
  char buffer;

  // this keeps our size for the user.
  if (size != nullptr) {
    *size = 1;
    return getCookie(key, &buffer, size) != ESP_ERR_NOT_FOUND;
  }
  // this just checks that it exists.
  else {
    size_t mysize = 1;
    return getCookie(key, &buffer, &mysize) != ESP_ERR_NOT_FOUND;
  }
}

esp_err_t PsychicRequest::getCookie(const char* key, char* buffer, size_t* size)
{
  return httpd_req_get_cookie_val(this->_req, key, buffer, size);
}

#ifdef ARDUINO
String PsychicRequest::getCookie(const char* key)
{
  size_t size;
  if (!hasCookie(key, &size))
    return String();

  std::string tmp;
  tmp.resize(size + 1);
  esp_err_t err = getCookie(key, &tmp[0], &size);
  if (err == ESP_OK)
    tmp.resize(size);
  else
    tmp.clear();

  return String(tmp.c_str());
}
#else
const char* PsychicRequest::getCookie(const char* key)
{
  _tmp.clear();

  size_t size;
  if (!hasCookie(key, &size))
    return _tmp.c_str();

  _tmp.resize(size + 1);
  esp_err_t err = getCookie(key, &_tmp[0], &size);
  if (err == ESP_OK)
    _tmp.resize(size);
  else
    _tmp.clear();

  return _tmp.c_str();
}
#endif

void PsychicRequest::replaceResponse(PsychicResponse* response)
{
  delete _response;
  _response = response;
}

void PsychicRequest::addResponseHeader(const char* key, const char* value)
{
  _response->addHeader(key, value);
}

std::list<HTTPHeader>& PsychicRequest::getResponseHeaders()
{
  return _response->headers();
}

void PsychicRequest::loadParams()
{
  if (_paramsParsed != ESP_ERR_NOT_FINISHED)
    return;

  // convenience shortcut to allow calling loadParams()
  if (_bodyParsed == ESP_ERR_NOT_FINISHED)
    loadBody();

  // various form data as parameters
  if (this->method() == HTTP_POST) {
    if (strncmp(this->_getHeader("Content-Type"), "application/x-www-form-urlencoded", 33) == 0)
      _addParams(_body.c_str(), true);

    if (this->isMultipart()) {
      MultipartProcessor mpp(this);
      _paramsParsed = mpp.process(_body.c_str());
      return;
    }
  }

  _paramsParsed = ESP_OK;
}

void PsychicRequest::_setUri(const char* uri)
{
  // save it
  _uri = uri;

  // look for our query separator
  size_t index = _uri.find('?', 0);
  if (index != std::string::npos) {
    // parse them.
    _query = _uri.substr(index + 1);
    _addParams(_query.c_str(), false);
  }
}

void PsychicRequest::_addParams(const char* params, bool post)
{
  const char* p = params;
  const char* pend = p + strlen(params);
  while (p < pend) {
    const char* amp = (const char*)memchr(p, '&', pend - p);
    if (amp == nullptr)
      amp = pend;
    const char* eq = (const char*)memchr(p, '=', amp - p);
    if (eq == nullptr)
      eq = amp;
    addParam(std::string(p, eq - p).c_str(), eq < amp ? std::string(eq + 1, amp - eq - 1).c_str() : "", true, post);
    p = amp + 1;
  }
}

PsychicWebParameter* PsychicRequest::addParam(const char* name, const char* value, bool decode, bool post)
{
  if (decode) {
    auto dn = urlDecode(name);
    auto dv = urlDecode(value);
    return addParam(new PsychicWebParameter(dn.c_str(), dv.c_str(), post));
  }
  return addParam(new PsychicWebParameter(name, value, post));
}

PsychicWebParameter* PsychicRequest::addParam(PsychicWebParameter* param)
{
  // ESP_LOGD(PH_TAG, "Adding param: '%s' = '%s'", param->name().c_str(), param->value().c_str());
  _params.push_back(param);
  return param;
}

bool PsychicRequest::hasParam(const char* key)
{
  return getParam(key) != NULL;
}

bool PsychicRequest::hasParam(const char* key, bool isPost, bool isFile)
{
  return getParam(key, isPost, isFile) != NULL;
}

PsychicWebParameter* PsychicRequest::getParam(const char* key)
{
  for (auto* param : _params)
    if (strcmp(param->nameCStr(), key) == 0)
      return param;

  return NULL;
}

PsychicWebParameter* PsychicRequest::getParam(const char* key, bool isPost, bool isFile)
{
  for (auto* param : _params)
    if (strcmp(param->nameCStr(), key) == 0 && isPost == param->isPost() && isFile == param->isFile())
      return param;
  return NULL;
}

#ifdef ARDUINO
String PsychicRequest::getParam(const char* key, const char* defaultValue)
{
  PsychicWebParameter* param = getParam(key);
  return param ? String(param->valueCStr()) : String(defaultValue);
}
#else
const char* PsychicRequest::getParam(const char* key, const char* defaultValue)
{
  PsychicWebParameter* param = getParam(key);
  return param ? param->valueCStr() : defaultValue;
}
#endif

bool PsychicRequest::hasSessionKey(const char* key)
{
  return this->_session->find(key) != this->_session->end();
}

#ifdef ARDUINO
String PsychicRequest::getSessionKey(const char* key)
{
  auto it = this->_session->find(key);
  if (it != this->_session->end())
    return String(it->second.c_str());
  else
    return String();
}
#else
const char* PsychicRequest::getSessionKey(const char* key)
{
  auto it = this->_session->find(key);
  if (it != this->_session->end())
    return it->second.c_str();
  else
    return "";
}
#endif

const char* PsychicRequest::getSessionKeyCStr(const char* key)
{
  auto it = this->_session->find(key);
  if (it != this->_session->end())
    return it->second.c_str();
  else
    return "";
}

void PsychicRequest::setSessionKey(const char* key, const char* value)
{
  (*this->_session)[key] = value;
}

static std::string md5str(const std::string& in)
{
  uint8_t digest[16];
#if ESP_IDF_VERSION_MAJOR >= 6
  md5_context_t ctx;
  esp_rom_md5_init(&ctx);
  esp_rom_md5_update(&ctx, (const uint8_t*)in.c_str(), in.length());
  esp_rom_md5_final(digest, &ctx);
#else
  mbedtls_md5_context ctx;
  mbedtls_md5_init(&ctx);
  #if MBEDTLS_VERSION_MAJOR >= 3
  mbedtls_md5_starts(&ctx);
  mbedtls_md5_update(&ctx, (const uint8_t*)in.c_str(), in.length());
  mbedtls_md5_finish(&ctx, digest);
  #else
  mbedtls_md5_starts_ret(&ctx);
  mbedtls_md5_update_ret(&ctx, (const uint8_t*)in.c_str(), in.length());
  mbedtls_md5_finish_ret(&ctx, digest);
  #endif
  mbedtls_md5_free(&ctx);
#endif

  static const char hexchars[] = "0123456789abcdef";
  char hex[33];
  for (int i = 0; i < 16; i++) {
    hex[i * 2] = hexchars[digest[i] >> 4];
    hex[i * 2 + 1] = hexchars[digest[i] & 0x0F];
  }
  hex[32] = '\0';
  return std::string(hex, 32);
}

bool PsychicRequest::authenticate(const char* username, const char* password, bool passwordIsHashed)
{
  if (hasHeader("Authorization")) {
    std::string authReq = _getHeader("Authorization");
    if (authReq.compare(0, 5, "Basic") == 0) {
      authReq = psychicSubstr(authReq, 6);
      authReq.erase(0, authReq.find_first_not_of(" \t\r\n\f\v"));
      authReq.erase(authReq.find_last_not_of(" \t\r\n\f\v") + 1);
      size_t toencodeLen = strlen(username) + strlen(password) + 1;
      char* toencode = new char[toencodeLen + 1];
      if (toencode == NULL) {
        authReq.clear();
        return false;
      }
      char* encoded = new char[4 * ((toencodeLen + 2) / 3) + 1];
      if (encoded == NULL) {
        authReq.clear();
        delete[] toencode;
        return false;
      }
      sprintf(toencode, "%s:%s", username, password);
      size_t encodedLen = 0;
      mbedtls_base64_encode((unsigned char*)encoded, 4 * ((toencodeLen + 2) / 3) + 1, &encodedLen, (unsigned char*)toencode, toencodeLen);
      if (encodedLen > 0) {
        // constant-time comparison to prevent timing attacks
        uint8_t ct_result = (authReq.size() != strlen(encoded));
        for (size_t i = 0; i < authReq.size() && i < strlen(encoded); i++)
          ct_result |= (uint8_t)authReq[i] ^ (uint8_t)encoded[i];
        if (ct_result == 0) {
          authReq.clear();
          delete[] toencode;
          delete[] encoded;
          return true;
        }
      }
      delete[] toencode;
      delete[] encoded;
    } else if (authReq.compare(0, 6, "Digest") == 0) {
      authReq = psychicSubstr(authReq, 7);
      std::string _username = _extractParam(authReq.c_str(), "username=\"", '\"');
      if (_username.empty() || _username != username) {
        authReq.clear();
        return false;
      }
      // extracting required parameters for RFC 2069 simpler Digest
      std::string _realm = _extractParam(authReq.c_str(), "realm=\"", '\"');
      std::string _nonce = _extractParam(authReq.c_str(), "nonce=\"", '\"');
      std::string _url = _extractParam(authReq.c_str(), "uri=\"", '\"');
      std::string _resp = _extractParam(authReq.c_str(), "response=\"", '\"');
      std::string _opaque = _extractParam(authReq.c_str(), "opaque=\"", '\"');

      if (_realm.empty() || _nonce.empty() || _url.empty() || _resp.empty() || _opaque.empty()) {
        authReq.clear();
        return false;
      }
      if ((_opaque != this->getSessionKeyCStr("opaque")) || (_nonce != this->getSessionKeyCStr("nonce")) || (_realm != this->getSessionKeyCStr("realm"))) {
        authReq.clear();
        return false;
      }
      // parameters for the RFC 2617 newer Digest
      // cache qop flag: avoids scanning the header string four times
      const bool _hasQop = authReq.find("qop=auth") != std::string::npos || authReq.find("qop=\"auth\"") != std::string::npos;
      std::string _nc, _cnonce;
      if (_hasQop) {
        _nc = _extractParam(authReq.c_str(), "nc=", ',');
        _cnonce = _extractParam(authReq.c_str(), "cnonce=\"", '\"');
      }

      std::string _H1 = passwordIsHashed ? std::string(password) : md5str(std::string(username) + ':' + _realm + ':' + password);
      // ESP_LOGD(PH_TAG, "Hash of user:realm:pass=%s", _H1.c_str());

      std::string _H2;
      switch (method()) {
        case HTTP_GET:
          _H2 = md5str(std::string("GET:") + _uri);
          break;
        case HTTP_POST:
          _H2 = md5str(std::string("POST:") + _uri);
          break;
        case HTTP_PUT:
          _H2 = md5str(std::string("PUT:") + _uri);
          break;
        case HTTP_DELETE:
          _H2 = md5str(std::string("DELETE:") + _uri);
          break;
        default:
          _H2 = md5str(std::string("GET:") + _uri);
          break;
      }

      // ESP_LOGD(PH_TAG, "Hash of GET:uri=%s", _H2.c_str());

      std::string _responsecheck;
      if (_hasQop) {
        _responsecheck = md5str(_H1 + ':' + _nonce + ':' + _nc + ':' + _cnonce + ":auth:" + _H2);
      } else {
        _responsecheck = md5str(_H1 + ':' + _nonce + ':' + _H2);
      }

      // ESP_LOGD(PH_TAG, "The Proper response=%s", _responsecheck.c_str());
      if (_resp == _responsecheck) {
        authReq.clear();
        return true;
      }
    }
    authReq.clear();
  }
  return false;
}

std::string PsychicRequest::_extractParam(const char* authReq, const char* param, const char delimit)
{
  std::string req(authReq);
  std::string par(param);
  size_t _begin = req.find(par);
  if (_begin == std::string::npos)
    return "";
  size_t _delim = req.find(delimit, _begin + par.length());
  if (_delim == std::string::npos)
    return req.substr(_begin + par.length());
  return req.substr(_begin + par.length(), _delim - _begin - par.length());
}

std::string PsychicRequest::_getRandomHexString()
{
  char buffer[33]; // buffer to hold 32 Hex Digit + /0
  int i;
  for (i = 0; i < 4; i++) {
    sprintf(buffer + (i * 8), "%08lx", (unsigned long int)esp_random());
  }
  return std::string(buffer);
}

esp_err_t PsychicRequest::requestAuthentication(HTTPAuthMethod mode, const char* realm, const char* authFailMsg)
{
  // what is thy realm, sire?
  if (!strcmp(realm, ""))
    this->setSessionKey("realm", "Login Required");
  else
    this->setSessionKey("realm", realm);

  PsychicResponse response(this);
  std::string authStr;

  // what kind of auth?
  if (mode == BASIC_AUTH) {
    authStr = "Basic realm=\"";
    authStr += this->getSessionKeyCStr("realm");
    authStr += "\"";
    response.addHeader("WWW-Authenticate", authStr.c_str());
  } else {
    // only make new ones if we havent sent them yet
    if (!strlen(this->getSessionKeyCStr("nonce")))
      this->setSessionKey("nonce", _getRandomHexString().c_str());
    if (!strlen(this->getSessionKeyCStr("opaque")))
      this->setSessionKey("opaque", _getRandomHexString().c_str());

    authStr = "Digest realm=\"";
    authStr += this->getSessionKeyCStr("realm");
    authStr += "\", qop=\"auth\", nonce=\"";
    authStr += this->getSessionKeyCStr("nonce");
    authStr += "\", opaque=\"";
    authStr += this->getSessionKeyCStr("opaque");
    authStr += "\"";
    response.addHeader("WWW-Authenticate", authStr.c_str());
  }

  response.setCode(401);
  response.setContentType("text/html");
  response.setContent(authFailMsg);
  return response.send();
}
