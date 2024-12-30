#include "PsychicRequest.h"
#include "MultipartProcessor.h"
#include "PsychicHttpServer.h"
#include "http_status.h"

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
    std::regex pattern(_endpoint->uri().c_str());
    std::string s(this->path().c_str());
    if (use_full_uri)
      s = this->uri().c_str();

    return std::regex_search(s, matches, pattern);
  }

  return false;
}
#endif

const String PsychicRequest::getFilename()
{
  // parse the content-disposition header
  if (this->hasHeader("Content-Disposition")) {
    ContentDisposition cd = this->getContentDisposition();
    if (cd.filename != "")
      return cd.filename;
  }

  // fall back to passed in query string
  PsychicWebParameter* param = getParam("_filename");
  if (param != NULL)
    return param->name();

  // fall back to parsing it from url (useful for wildcard uploads)
  String uri = this->uri();
  int filenameStart = uri.lastIndexOf('/') + 1;
  String filename = uri.substring(filenameStart);
  if (filename != "")
    return filename;

  // finally, unknown.
  ESP_LOGE(PH_TAG, "Did not get a valid filename from the upload.");
  return "unknown.txt";
}

const ContentDisposition PsychicRequest::getContentDisposition()
{
  ContentDisposition cd;
  String header = this->header("Content-Disposition");
  int start;
  int end;

  if (header.indexOf("form-data") == 0)
    cd.disposition = FORM_DATA;
  else if (header.indexOf("attachment") == 0)
    cd.disposition = ATTACHMENT;
  else if (header.indexOf("inline") == 0)
    cd.disposition = INLINE;
  else
    cd.disposition = NONE;

  start = header.indexOf("filename=");
  if (start) {
    end = header.indexOf('"', start + 10);
    cd.filename = header.substring(start + 10, end - 1);
  }

  start = header.indexOf("name=");
  if (start) {
    end = header.indexOf('"', start + 6);
    cd.name = header.substring(start + 6, end - 1);
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

  this->_body = String();

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
  this->_body = String(buf);
  free(buf);

  _bodyParsed = ESP_OK;

  return _bodyParsed;
}

http_method PsychicRequest::method()
{
  return (http_method)this->_req->method;
}

const String PsychicRequest::methodStr()
{
  return String(http_method_str((http_method)this->_req->method));
}

const String PsychicRequest::path()
{
  int index = _uri.indexOf("?");
  if (index == -1)
    return _uri;
  else
    return _uri.substring(0, index);
}

const String& PsychicRequest::uri()
{
  return this->_uri;
}

const String& PsychicRequest::query()
{
  return this->_query;
}

// no way to get list of headers yet....
// int PsychicRequest::headers()
// {
// }

const String PsychicRequest::header(const char* name)
{
  size_t header_len = httpd_req_get_hdr_value_len(this->_req, name);

  // if we've got one, allocated it and load it
  if (header_len) {
    char header[header_len + 1];
    httpd_req_get_hdr_value_str(this->_req, name, header, sizeof(header));
    return String(header);
  } else
    return "";
}

bool PsychicRequest::hasHeader(const char* name)
{
  return httpd_req_get_hdr_value_len(this->_req, name) > 0;
}

const String PsychicRequest::host()
{
  return this->header("Host");
}

const String PsychicRequest::contentType()
{
  return header("Content-Type");
}

size_t PsychicRequest::contentLength()
{
  return this->_req->content_len;
}

const String& PsychicRequest::body()
{
  return this->_body;
}

bool PsychicRequest::isMultipart()
{
  const String& type = this->contentType();

  return (this->contentType().indexOf("multipart/form-data") >= 0);
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

String PsychicRequest::getCookie(const char* key)
{
  String cookie = "";

  // how big is our cookie?
  size_t size;
  if (!hasCookie("counter", &size))
    return cookie;

  // allocate cookie buffer... keep it on the stack
  char buf[size];

  // load it up.
  esp_err_t err = getCookie(key, buf, &size);
  if (err == ESP_OK)
    cookie.concat(buf);

  return cookie;
}

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
    if (this->contentType().startsWith("application/x-www-form-urlencoded"))
      _addParams(_body, true);

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
  _uri = String(uri);

  // look for our query separator
  int index = _uri.indexOf('?', 0);
  if (index) {
    // parse them.
    _query = _uri.substring(index + 1);
    _addParams(_query, false);
  }
}

void PsychicRequest::_addParams(const String& params, bool post)
{
  size_t start = 0;
  while (start < params.length()) {
    int end = params.indexOf('&', start);
    if (end < 0)
      end = params.length();
    int equal = params.indexOf('=', start);
    if (equal < 0 || equal > end)
      equal = end;
    String name = params.substring(start, equal);
    String value = equal + 1 < end ? params.substring(equal + 1, end) : String();
    addParam(name, value, true, post);
    start = end + 1;
  }
}

PsychicWebParameter* PsychicRequest::addParam(const String& name, const String& value, bool decode, bool post)
{
  if (decode)
    return addParam(new PsychicWebParameter(urlDecode(name.c_str()), urlDecode(value.c_str()), post));
  else
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
    if (param->name().equals(key))
      return param;

  return NULL;
}

PsychicWebParameter* PsychicRequest::getParam(const char* key, bool isPost, bool isFile)
{
  for (auto* param : _params)
    if (param->name().equals(key) && isPost == param->isPost() && isFile == param->isFile())
      return param;
  return NULL;
}

bool PsychicRequest::hasSessionKey(const String& key)
{
  return this->_session->find(key) != this->_session->end();
}

const String PsychicRequest::getSessionKey(const String& key)
{
  auto it = this->_session->find(key);
  if (it != this->_session->end())
    return it->second;
  else
    return "";
}

void PsychicRequest::setSessionKey(const String& key, const String& value)
{
  this->_session->insert(std::pair<String, String>(key, value));
}

static const String md5str(const String& in)
{
  MD5Builder md5 = MD5Builder();
  md5.begin();
  md5.add(in);
  md5.calculate();
  return md5.toString();
}

bool PsychicRequest::authenticate(const char* username, const char* password)
{
  if (hasHeader("Authorization")) {
    String authReq = header("Authorization");
    if (authReq.startsWith("Basic")) {
      authReq = authReq.substring(6);
      authReq.trim();
      char toencodeLen = strlen(username) + strlen(password) + 1;
      char* toencode = new char[toencodeLen + 1];
      if (toencode == NULL) {
        authReq = "";
        return false;
      }
      char* encoded = new char[base64_encode_expected_len(toencodeLen) + 1];
      if (encoded == NULL) {
        authReq = "";
        delete[] toencode;
        return false;
      }
      sprintf(toencode, "%s:%s", username, password);
      if (base64_encode_chars(toencode, toencodeLen, encoded) > 0 && authReq.equalsConstantTime(encoded)) {
        authReq = "";
        delete[] toencode;
        delete[] encoded;
        return true;
      }
      delete[] toencode;
      delete[] encoded;
    } else if (authReq.startsWith(F("Digest"))) {
      authReq = authReq.substring(7);
      String _username = _extractParam(authReq, F("username=\""), '\"');
      if (!_username.length() || _username != String(username)) {
        authReq = "";
        return false;
      }
      // extracting required parameters for RFC 2069 simpler Digest
      String _realm = _extractParam(authReq, F("realm=\""), '\"');
      String _nonce = _extractParam(authReq, F("nonce=\""), '\"');
      String _url = _extractParam(authReq, F("uri=\""), '\"');
      String _resp = _extractParam(authReq, F("response=\""), '\"');
      String _opaque = _extractParam(authReq, F("opaque=\""), '\"');

      if ((!_realm.length()) || (!_nonce.length()) || (!_url.length()) || (!_resp.length()) || (!_opaque.length())) {
        authReq = "";
        return false;
      }
      if ((_opaque != this->getSessionKey("opaque")) || (_nonce != this->getSessionKey("nonce")) || (_realm != this->getSessionKey("realm"))) {
        authReq = "";
        return false;
      }
      // parameters for the RFC 2617 newer Digest
      String _nc, _cnonce;
      if (authReq.indexOf("qop=auth") != -1 || authReq.indexOf("qop=\"auth\"") != -1) {
        _nc = _extractParam(authReq, F("nc="), ',');
        _cnonce = _extractParam(authReq, F("cnonce=\""), '\"');
      }

      String _H1 = md5str(String(username) + ':' + _realm + ':' + String(password));
      // ESP_LOGD(PH_TAG, "Hash of user:realm:pass=%s", _H1.c_str());

      String _H2 = "";
      if (_method == HTTP_GET) {
        _H2 = md5str(String(F("GET:")) + _url);
      } else if (_method == HTTP_POST) {
        _H2 = md5str(String(F("POST:")) + _url);
      } else if (_method == HTTP_PUT) {
        _H2 = md5str(String(F("PUT:")) + _url);
      } else if (_method == HTTP_DELETE) {
        _H2 = md5str(String(F("DELETE:")) + _url);
      } else {
        _H2 = md5str(String(F("GET:")) + _url);
      }
      // ESP_LOGD(PH_TAG, "Hash of GET:uri=%s", _H2.c_str());

      String _responsecheck = "";
      if (authReq.indexOf("qop=auth") != -1 || authReq.indexOf("qop=\"auth\"") != -1) {
        _responsecheck = md5str(_H1 + ':' + _nonce + ':' + _nc + ':' + _cnonce + F(":auth:") + _H2);
      } else {
        _responsecheck = md5str(_H1 + ':' + _nonce + ':' + _H2);
      }

      // ESP_LOGD(PH_TAG, "The Proper response=%s", _responsecheck.c_str());
      if (_resp == _responsecheck) {
        authReq = "";
        return true;
      }
    }
    authReq = "";
  }
  return false;
}

const String PsychicRequest::_extractParam(const String& authReq, const String& param, const char delimit)
{
  int _begin = authReq.indexOf(param);
  if (_begin == -1)
    return "";
  return authReq.substring(_begin + param.length(), authReq.indexOf(delimit, _begin + param.length()));
}

const String PsychicRequest::_getRandomHexString()
{
  char buffer[33]; // buffer to hold 32 Hex Digit + /0
  int i;
  for (i = 0; i < 4; i++) {
    sprintf(buffer + (i * 8), "%08lx", (unsigned long int)esp_random());
  }
  return String(buffer);
}

esp_err_t PsychicRequest::requestAuthentication(HTTPAuthMethod mode, const char* realm, const char* authFailMsg)
{
  // what is thy realm, sire?
  if (!strcmp(realm, ""))
    this->setSessionKey("realm", "Login Required");
  else
    this->setSessionKey("realm", realm);

  PsychicResponse response(this);
  String authStr;

  // what kind of auth?
  if (mode == BASIC_AUTH) {
    authStr = "Basic realm=\"" + this->getSessionKey("realm") + "\"";
    response.addHeader("WWW-Authenticate", authStr.c_str());
  } else {
    // only make new ones if we havent sent them yet
    if (this->getSessionKey("nonce").isEmpty())
      this->setSessionKey("nonce", _getRandomHexString());
    if (this->getSessionKey("opaque").isEmpty())
      this->setSessionKey("opaque", _getRandomHexString());

    authStr = "Digest realm=\"" + this->getSessionKey("realm") + "\", qop=\"auth\", nonce=\"" + this->getSessionKey("nonce") + "\", opaque=\"" + this->getSessionKey("opaque") + "\"";
    response.addHeader("WWW-Authenticate", authStr.c_str());
  }

  response.setCode(401);
  response.setContentType("text/html");
  response.setContent(authFailMsg);
  return response.send();
}
