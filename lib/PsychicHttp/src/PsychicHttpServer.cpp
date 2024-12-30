#include "PsychicHttpServer.h"
#include "PsychicEndpoint.h"
#include "PsychicHandler.h"
#include "PsychicJson.h"
#include "PsychicStaticFileHandler.h"
#include "PsychicWebHandler.h"
#include "PsychicWebSocket.h"
#include "WiFi.h"
#ifdef PSY_ENABLE_ETHERNET
  #include "ETH.h"
#endif

PsychicHttpServer::PsychicHttpServer(uint16_t port)
{
  maxRequestBodySize = MAX_REQUEST_BODY_SIZE;
  maxUploadSize = MAX_UPLOAD_SIZE;

  defaultEndpoint = new PsychicEndpoint(this, HTTP_GET, "");
  onNotFound(PsychicHttpServer::defaultNotFoundHandler);

  // for a regular server
  config = HTTPD_DEFAULT_CONFIG();
  config.open_fn = PsychicHttpServer::openCallback;
  config.close_fn = PsychicHttpServer::closeCallback;
  config.global_user_ctx = this;
  config.global_user_ctx_free_fn = PsychicHttpServer::destroy;
  config.uri_match_fn = MATCH_WILDCARD; // new internal endpoint matching - do not change this!!!
  config.stack_size = 4608;             // default stack is just a little bit too small.

  // our internal matching function for endpoints
  _uri_match_fn = MATCH_WILDCARD; // use this change the endpoint matching function.

#ifdef ENABLE_ASYNC
  // It is advisable that httpd_config_t->max_open_sockets > MAX_ASYNC_REQUESTS
  // Why? This leaves at least one socket still available to handle
  // quick synchronous requests. Otherwise, all the sockets will
  // get taken by the long async handlers, and your server will no
  // longer be responsive.
  config.max_open_sockets = ASYNC_WORKER_COUNT + 1;
  config.lru_purge_enable = true;
#endif

  setPort(port);
}

PsychicHttpServer::~PsychicHttpServer()
{
  _esp_idf_endpoints.clear();

  for (auto* client : _clients)
    delete (client);
  _clients.clear();

  for (auto* endpoint : _endpoints)
    delete (endpoint);
  _endpoints.clear();

  for (auto* handler : _handlers)
    delete (handler);
  _handlers.clear();

  for (auto* rewrite : _rewrites)
    delete (rewrite);
  _rewrites.clear();

  delete defaultEndpoint;
  delete _chain;
}

void PsychicHttpServer::destroy(void* ctx)
{
  // do not release any resource for PsychicHttpServer in order to be able to restart it after stopping
}

void PsychicHttpServer::setPort(uint16_t port)
{
  this->config.server_port = port;
}

uint16_t PsychicHttpServer::getPort()
{
  return this->config.server_port;
}

bool PsychicHttpServer::isConnected()
{
  if (WiFi.softAPIP())
    return true;
  if (WiFi.localIP())
    return true;

#ifdef PSY_ENABLE_ETHERNET
  if (ETH.localIP())
    return true;
#endif

  return false;
}

esp_err_t PsychicHttpServer::start()
{
  if (_running)
    return ESP_OK;

  // starting without network will crash us.
  if (!isConnected()) {
    ESP_LOGE(PH_TAG, "Server start failed - no network.");
    return ESP_FAIL;
  }

  esp_err_t ret;

#ifdef ENABLE_ASYNC
  // start workers
  start_async_req_workers();
#endif

  // one URI handler for each http_method
  config.max_uri_handlers = supported_methods.size() + _esp_idf_endpoints.size();

  // fire it up.
  ret = _startServer();
  if (ret != ESP_OK) {
    ESP_LOGE(PH_TAG, "Server start failed (%s)", esp_err_to_name(ret));
    return ret;
  }

  // some handlers (aka websockets) need actual endpoints in esp-idf http_server
  for (auto& endpoint : _esp_idf_endpoints) {
    ESP_LOGD(PH_TAG, "Adding endpoint %s | %s", endpoint.uri, http_method_str((http_method)endpoint.method));

    // Register endpoint with ESP-IDF server
    esp_err_t ret = httpd_register_uri_handler(this->server, &endpoint);
    if (ret != ESP_OK)
      ESP_LOGE(PH_TAG, "Add endpoint failed (%s)", esp_err_to_name(ret));
  }

  // Register a handler for each http_method method - it will match all requests with that URI/method
  for (auto& method : supported_methods) {
    ESP_LOGD(PH_TAG, "Adding %s meta endpoint", http_method_str((http_method)method));

    httpd_uri_t my_uri;
    my_uri.uri = "*";
    my_uri.method = method;
    my_uri.handler = PsychicHttpServer::requestHandler;
    my_uri.is_websocket = false;
    my_uri.supported_subprotocol = "";

    // Register endpoint with ESP-IDF server
    esp_err_t ret = httpd_register_uri_handler(this->server, &my_uri);
    if (ret != ESP_OK)
      ESP_LOGE(PH_TAG, "Add endpoint failed (%s)", esp_err_to_name(ret));
  }

  // Register handler
  ret = httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, PsychicHttpServer::notFoundHandler);
  if (ret != ESP_OK)
    ESP_LOGE(PH_TAG, "Add 404 handler failed (%s)", esp_err_to_name(ret));

  ESP_LOGI(PH_TAG, "Server started on port %" PRIu16, getPort());

  _running = true;
  return ret;
}

esp_err_t PsychicHttpServer::_startServer()
{
  return httpd_start(&this->server, &this->config);
}

esp_err_t PsychicHttpServer::stop()
{
  if (!_running)
    return ESP_OK;

  // some handlers (aka websockets) need actual endpoints in esp-idf http_server
  for (auto& endpoint : _esp_idf_endpoints) {
    ESP_LOGD(PH_TAG, "Removing endpoint %s | %s", endpoint.uri, http_method_str((http_method)endpoint.method));

    // Unregister endpoint with ESP-IDF server
    esp_err_t ret = httpd_unregister_uri_handler(this->server, endpoint.uri, endpoint.method);
    if (ret != ESP_OK)
      ESP_LOGE(PH_TAG, "Removal of endpoint failed (%s)", esp_err_to_name(ret));
  }

  // Unregister a handler for each http_method method - it will match all requests with that URI/method
  for (auto& method : supported_methods) {
    ESP_LOGD(PH_TAG, "Removing %s meta endpoint", http_method_str((http_method)method));

    // Unregister endpoint with ESP-IDF server
    esp_err_t ret = httpd_unregister_uri_handler(this->server, "*", method);
    if (ret != ESP_OK)
      ESP_LOGE(PH_TAG, "Removal of endpoint failed (%s)", esp_err_to_name(ret));
  }

  esp_err_t ret = _stopServer();
  if (ret != ESP_OK) {
    ESP_LOGE(PH_TAG, "Server stop failed (%s)", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(PH_TAG, "Server stopped");
  _running = false;
  return ret;
}

esp_err_t PsychicHttpServer::_stopServer()
{
  return httpd_stop(this->server);
}

void PsychicHttpServer::reset()
{
  if (_running)
    stop();

  for (auto* client : _clients)
    delete (client);
  _clients.clear();

  for (auto* endpoint : _endpoints)
    delete (endpoint);
  _endpoints.clear();

  for (auto* handler : _handlers)
    delete (handler);
  _handlers.clear();

  for (auto* rewrite : _rewrites)
    delete (rewrite);
  _rewrites.clear();

  _esp_idf_endpoints.clear();

  onNotFound(PsychicHttpServer::defaultNotFoundHandler);
  _onOpen = nullptr;
  _onClose = nullptr;
}

httpd_uri_match_func_t PsychicHttpServer::getURIMatchFunction()
{
  return _uri_match_fn;
}

void PsychicHttpServer::setURIMatchFunction(httpd_uri_match_func_t match_fn)
{
  _uri_match_fn = match_fn;
}

PsychicHandler* PsychicHttpServer::addHandler(PsychicHandler* handler)
{
  _handlers.push_back(handler);
  return handler;
}

void PsychicHttpServer::removeHandler(PsychicHandler* handler)
{
  _handlers.remove(handler);
}

PsychicRewrite* PsychicHttpServer::addRewrite(PsychicRewrite* rewrite)
{
  _rewrites.push_back(rewrite);
  return rewrite;
}

void PsychicHttpServer::removeRewrite(PsychicRewrite* rewrite)
{
  _rewrites.remove(rewrite);
}

PsychicRewrite* PsychicHttpServer::rewrite(const char* from, const char* to)
{
  return addRewrite(new PsychicRewrite(from, to));
}

PsychicEndpoint* PsychicHttpServer::on(const char* uri)
{
  return on(uri, HTTP_GET);
}

PsychicEndpoint* PsychicHttpServer::on(const char* uri, int method)
{
  PsychicWebHandler* handler = new PsychicWebHandler();

  return on(uri, method, handler);
}

PsychicEndpoint* PsychicHttpServer::on(const char* uri, PsychicHandler* handler)
{
  return on(uri, HTTP_GET, handler);
}

PsychicEndpoint* PsychicHttpServer::on(const char* uri, int method, PsychicHandler* handler)
{
  // make our endpoint
  PsychicEndpoint* endpoint = new PsychicEndpoint(this, method, uri);

  // set our handler
  endpoint->setHandler(handler);

  // websockets need a real endpoint in esp-idf
  if (handler->isWebSocket()) {
    // URI handler structure
    httpd_uri_t my_uri;
    my_uri.uri = uri;
    my_uri.method = HTTP_GET;
    my_uri.handler = PsychicEndpoint::requestCallback;
    my_uri.user_ctx = endpoint;
    my_uri.is_websocket = handler->isWebSocket();
    my_uri.supported_subprotocol = handler->getSubprotocol();

    // save it to our 'real' handlers for later.
    _esp_idf_endpoints.push_back(my_uri);
  }

  // if this is a method we haven't added yet, do it.
  if (method != HTTP_ANY) {
    if (!(std::find(supported_methods.begin(), supported_methods.end(), (http_method)method) != supported_methods.end())) {
      ESP_LOGD(PH_TAG, "Adding %s to server.supported_methods", http_method_str((http_method)method));
      supported_methods.push_back((http_method)method);
    }
  }

  // add it to our meta endpoints
  _endpoints.push_back(endpoint);

  return endpoint;
}

PsychicEndpoint* PsychicHttpServer::on(const char* uri, PsychicHttpRequestCallback fn)
{
  return on(uri, HTTP_GET, fn);
}

PsychicEndpoint* PsychicHttpServer::on(const char* uri, int method, PsychicHttpRequestCallback fn)
{
  // these basic requests need a basic web handler
  PsychicWebHandler* handler = new PsychicWebHandler();
  handler->onRequest(fn);

  return on(uri, method, handler);
}

PsychicEndpoint* PsychicHttpServer::on(const char* uri, PsychicJsonRequestCallback fn)
{
  return on(uri, HTTP_GET, fn);
}

PsychicEndpoint* PsychicHttpServer::on(const char* uri, int method, PsychicJsonRequestCallback fn)
{
  // these basic requests need a basic web handler
  PsychicJsonHandler* handler = new PsychicJsonHandler();
  handler->onRequest(fn);

  return on(uri, method, handler);
}

bool PsychicHttpServer::removeEndpoint(const char* uri, int method)
{
  // some handlers (aka websockets) need actual endpoints in esp-idf http_server
  // don't return from here, because its added to the _endpoints list too.
  for (auto& endpoint : _esp_idf_endpoints) {
    if (!strcmp(endpoint.uri, uri) && method == endpoint.method) {
      ESP_LOGD(PH_TAG, "Unregistering endpoint %s | %s", endpoint.uri, http_method_str((http_method)endpoint.method));

      // Register endpoint with ESP-IDF server
      esp_err_t ret = httpd_register_uri_handler(this->server, &endpoint);
      if (ret != ESP_OK)
        ESP_LOGE(PH_TAG, "Add endpoint failed (%s)", esp_err_to_name(ret));
    }
  }

  // loop through our endpoints and see if anyone matches
  for (auto* endpoint : _endpoints) {
    if (endpoint->uri().equals(uri) && method == endpoint->_method)
      return removeEndpoint(endpoint);
  }

  return false;
}

bool PsychicHttpServer::removeEndpoint(PsychicEndpoint* endpoint)
{
  _endpoints.remove(endpoint);
  return true;
}

PsychicHttpServer* PsychicHttpServer::addFilter(PsychicRequestFilterFunction fn)
{
  _filters.push_back(fn);

  return this;
}

bool PsychicHttpServer::_filter(PsychicRequest* request)
{
  // run through our filter chain.
  for (auto& filter : _filters) {
    if (!filter(request))
      return false;
  }

  return true;
}

PsychicHttpServer* PsychicHttpServer::addMiddleware(PsychicMiddleware* middleware)
{
  if (!_chain) {
    _chain = new PsychicMiddlewareChain();
  }
  _chain->addMiddleware(middleware);
  return this;
}

PsychicHttpServer* PsychicHttpServer::addMiddleware(PsychicMiddlewareCallback fn)
{
  if (!_chain) {
    _chain = new PsychicMiddlewareChain();
  }
  _chain->addMiddleware(fn);
  return this;
}

void PsychicHttpServer::removeMiddleware(PsychicMiddleware* middleware)
{
  if (_chain) {
    _chain->removeMiddleware(middleware);
  }
}

void PsychicHttpServer::onNotFound(PsychicHttpRequestCallback fn)
{
  PsychicWebHandler* handler = new PsychicWebHandler();
  handler->onRequest(fn == nullptr ? PsychicHttpServer::defaultNotFoundHandler : fn);

  this->defaultEndpoint->setHandler(handler);
}

bool PsychicHttpServer::_rewriteRequest(PsychicRequest* request)
{
  for (auto* r : _rewrites) {
    if (r->match(request)) {
      request->_setUri(r->toUrl().c_str());
      return true;
    }
  }

  return false;
}

esp_err_t PsychicHttpServer::requestHandler(httpd_req_t* req)
{
  PsychicHttpServer* server = (PsychicHttpServer*)httpd_get_global_user_ctx(req->handle);
  PsychicRequest request(server, req);

  // process any URL rewrites
  server->_rewriteRequest(&request);

  // run it through our global server filter list
  if (!server->_filter(&request)) {
    ESP_LOGD(PH_TAG, "Request %s refused by global filter", request.uri().c_str());
    return request.response()->send(400);
  }

  // then runs the request through the filter chain
  esp_err_t ret;
  if (server->_chain) {
    ret = server->_chain->runChain(&request, [server, &request]() {
      return server->_process(&request);
    });
  } else {
    ret = server->_process(&request);
  }
  ESP_LOGD(PH_TAG, "Request %s processed by global middleware: %s", request.uri().c_str(), esp_err_to_name(ret));

  if (ret == HTTPD_404_NOT_FOUND) {
    return PsychicHttpServer::notFoundHandler(req, HTTPD_404_NOT_FOUND);
  }

  return ret;
}

esp_err_t PsychicHttpServer::_process(PsychicRequest* request)
{
  // loop through our endpoints and see if anyone wants it.
  for (auto* endpoint : _endpoints) {
    if (endpoint->matches(request->uri().c_str())) {
      if (endpoint->_method == request->method() || endpoint->_method == HTTP_ANY) {
        request->setEndpoint(endpoint);
        return endpoint->process(request);
      }
    }
  }

  // loop through our global handlers and see if anyone wants it
  for (auto* handler : _handlers) {
    esp_err_t ret = handler->process(request);
    if (ret != HTTPD_404_NOT_FOUND)
      return ret;
  }

  return HTTPD_404_NOT_FOUND;
}

esp_err_t PsychicHttpServer::notFoundHandler(httpd_req_t* req, httpd_err_code_t err)
{
  PsychicHttpServer* server = (PsychicHttpServer*)httpd_get_global_user_ctx(req->handle);
  PsychicRequest request(server, req);

  // pull up our default handler / endpoint
  PsychicHandler* handler = server->defaultEndpoint->handler();
  if (!handler)
    return request.response()->send(404);

  esp_err_t ret = handler->process(&request);
  if (ret != HTTPD_404_NOT_FOUND)
    return ret;

  // not sure how we got this far.
  return request.response()->send(404);
}

esp_err_t PsychicHttpServer::defaultNotFoundHandler(PsychicRequest* request, PsychicResponse* response)
{
  return response->send(404, "text/html", "That URI does not exist.");
}

void PsychicHttpServer::onOpen(PsychicClientCallback handler)
{
  this->_onOpen = handler;
}

esp_err_t PsychicHttpServer::openCallback(httpd_handle_t hd, int sockfd)
{
  ESP_LOGD(PH_TAG, "New client connected %d", sockfd);

  // get our global server reference
  PsychicHttpServer* server = (PsychicHttpServer*)httpd_get_global_user_ctx(hd);

  // lookup our client
  PsychicClient* client = server->getClient(sockfd);
  if (client == NULL) {
    client = new PsychicClient(hd, sockfd);
    server->addClient(client);
  }

  // user callback
  if (server->_onOpen != NULL)
    server->_onOpen(client);

  return ESP_OK;
}

void PsychicHttpServer::onClose(PsychicClientCallback handler)
{
  this->_onClose = handler;
}

void PsychicHttpServer::closeCallback(httpd_handle_t hd, int sockfd)
{
  ESP_LOGD(PH_TAG, "Client disconnected %d", sockfd);

  PsychicHttpServer* server = (PsychicHttpServer*)httpd_get_global_user_ctx(hd);

  // lookup our client
  PsychicClient* client = server->getClient(sockfd);
  if (client != NULL) {
    // give our handlers a chance to handle a disconnect first
    for (PsychicEndpoint* endpoint : server->_endpoints) {
      PsychicHandler* handler = endpoint->handler();
      handler->checkForClosedClient(client);
    }

    // do we have a callback attached?
    if (server->_onClose != NULL)
      server->_onClose(client);

    // remove it from our list
    server->removeClient(client);
  } else
    ESP_LOGE(PH_TAG, "No client record %d", sockfd);

  // finally close it out.
  close(sockfd);
}

PsychicStaticFileHandler* PsychicHttpServer::serveStatic(const char* uri, fs::FS& fs, const char* path, const char* cache_control)
{
  PsychicStaticFileHandler* handler = new PsychicStaticFileHandler(uri, fs, path, cache_control);
  this->addHandler(handler);

  return handler;
}

void PsychicHttpServer::addClient(PsychicClient* client)
{
  _clients.push_back(client);
}

void PsychicHttpServer::removeClient(PsychicClient* client)
{
  _clients.remove(client);
  delete client;
}

PsychicClient* PsychicHttpServer::getClient(int socket)
{
  for (PsychicClient* client : _clients)
    if (client->socket() == socket)
      return client;

  return NULL;
}

PsychicClient* PsychicHttpServer::getClient(httpd_req_t* req)
{
  return getClient(httpd_req_to_sockfd(req));
}

bool PsychicHttpServer::hasClient(int socket)
{
  return getClient(socket) != NULL;
}

const std::list<PsychicClient*>& PsychicHttpServer::getClientList()
{
  return _clients;
}

bool ON_STA_FILTER(PsychicRequest* request)
{
  return WiFi.localIP() == request->client()->localIP();
}

bool ON_AP_FILTER(PsychicRequest* request)
{
  return WiFi.softAPIP() == request->client()->localIP();
}

String urlDecode(const char* encoded)
{
  size_t length = strlen(encoded);
  char* decoded = (char*)malloc(length + 1);
  if (!decoded) {
    return "";
  }

  size_t i, j = 0;
  for (i = 0; i < length; ++i) {
    if (encoded[i] == '%' && isxdigit(encoded[i + 1]) && isxdigit(encoded[i + 2])) {
      // Valid percent-encoded sequence
      int hex;
      sscanf(encoded + i + 1, "%2x", &hex);
      decoded[j++] = (char)hex;
      i += 2; // Skip the two hexadecimal characters
    } else if (encoded[i] == '+') {
      // Convert '+' to space
      decoded[j++] = ' ';
    } else {
      // Copy other characters as they are
      decoded[j++] = encoded[i];
    }
  }

  decoded[j] = '\0'; // Null-terminate the decoded string

  String output(decoded);
  free(decoded);

  return output;
}

bool psychic_uri_match_simple(const char* uri1, const char* uri2, size_t len2)
{
  return strlen(uri1) == len2 &&           // First match lengths
         (strncmp(uri1, uri2, len2) == 0); // Then match actual URIs
}

#ifdef PSY_ENABLE_REGEX
bool psychic_uri_match_regex(const char* uri1, const char* uri2, size_t len2)
{
  std::regex pattern(uri1);
  std::smatch matches;
  std::string s(uri2);

  // len2 is passed in to tell us to match up to a point.
  if (s.length() > len2)
    s = s.substr(0, len2);

  return std::regex_search(s, matches, pattern);
}
#endif