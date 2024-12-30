#ifndef PsychicHttpServer_h
#define PsychicHttpServer_h

#include "PsychicClient.h"
#include "PsychicCore.h"
#include "PsychicHandler.h"
#include "PsychicMiddleware.h"
#include "PsychicMiddlewareChain.h"
#include "PsychicRewrite.h"

#ifdef PSY_ENABLE_REGEX
  #include <regex>
#endif

#ifndef HTTP_ANY
  #define HTTP_ANY INT_MAX
#endif

class PsychicEndpoint;
class PsychicHandler;
class PsychicStaticFileHandler;

class PsychicHttpServer
{
  protected:
    std::list<httpd_uri_t> _esp_idf_endpoints;
    std::list<PsychicEndpoint*> _endpoints;
    std::list<PsychicHandler*> _handlers;
    std::list<PsychicClient*> _clients;
    std::list<PsychicRewrite*> _rewrites;
    std::list<PsychicRequestFilterFunction> _filters;

    PsychicClientCallback _onOpen = nullptr;
    PsychicClientCallback _onClose = nullptr;
    PsychicMiddlewareChain* _chain = nullptr;

    esp_err_t _start();
    virtual esp_err_t _startServer();
    virtual esp_err_t _stopServer();
    bool _running = false;
    httpd_uri_match_func_t _uri_match_fn = nullptr;

    bool _rewriteRequest(PsychicRequest* request);
    esp_err_t _process(PsychicRequest* request);
    bool _filter(PsychicRequest* request);

  public:
    PsychicHttpServer(uint16_t port = 80);
    virtual ~PsychicHttpServer();

    // what methods to support
    std::list<http_method> supported_methods = {
      HTTP_GET,
      HTTP_POST,
      HTTP_DELETE,
      HTTP_HEAD,
      HTTP_PUT,
      HTTP_OPTIONS
    };

    // esp-idf specific stuff
    httpd_handle_t server;
    httpd_config_t config;

    // some limits on what we will accept
    unsigned long maxUploadSize;
    unsigned long maxRequestBodySize;

    PsychicEndpoint* defaultEndpoint;

    static void destroy(void* ctx);

    virtual void setPort(uint16_t port);
    virtual uint16_t getPort();

    bool isConnected();
    bool isRunning() { return _running; }
    esp_err_t begin() { return start(); }
    esp_err_t end() { return stop(); }
    esp_err_t start();
    esp_err_t stop();
    void reset();

    httpd_uri_match_func_t getURIMatchFunction();
    void setURIMatchFunction(httpd_uri_match_func_t match_fn);

    PsychicRewrite* addRewrite(PsychicRewrite* rewrite);
    void removeRewrite(PsychicRewrite* rewrite);
    PsychicRewrite* rewrite(const char* from, const char* to);

    PsychicHandler* addHandler(PsychicHandler* handler);
    void removeHandler(PsychicHandler* handler);

    void addClient(PsychicClient* client);
    void removeClient(PsychicClient* client);
    PsychicClient* getClient(int socket);
    PsychicClient* getClient(httpd_req_t* req);
    bool hasClient(int socket);
    int count() { return _clients.size(); };
    const std::list<PsychicClient*>& getClientList();

    PsychicEndpoint* on(const char* uri);
    PsychicEndpoint* on(const char* uri, int method);
    PsychicEndpoint* on(const char* uri, PsychicHandler* handler);
    PsychicEndpoint* on(const char* uri, int method, PsychicHandler* handler);
    PsychicEndpoint* on(const char* uri, PsychicHttpRequestCallback onRequest);
    PsychicEndpoint* on(const char* uri, int method, PsychicHttpRequestCallback onRequest);
    PsychicEndpoint* on(const char* uri, PsychicJsonRequestCallback onRequest);
    PsychicEndpoint* on(const char* uri, int method, PsychicJsonRequestCallback onRequest);

    bool removeEndpoint(const char* uri, int method);
    bool removeEndpoint(PsychicEndpoint* endpoint);

    PsychicHttpServer* addFilter(PsychicRequestFilterFunction fn);

    PsychicHttpServer* addMiddleware(PsychicMiddleware* middleware);
    PsychicHttpServer* addMiddleware(PsychicMiddlewareCallback fn);
    void removeMiddleware(PsychicMiddleware *middleware);

    static esp_err_t requestHandler(httpd_req_t* req);
    static esp_err_t notFoundHandler(httpd_req_t* req, httpd_err_code_t err);
    static esp_err_t defaultNotFoundHandler(PsychicRequest* request, PsychicResponse* response);
    static esp_err_t openCallback(httpd_handle_t hd, int sockfd);
    static void closeCallback(httpd_handle_t hd, int sockfd);

    void onNotFound(PsychicHttpRequestCallback fn);
    void onOpen(PsychicClientCallback handler);
    void onClose(PsychicClientCallback handler);

    PsychicStaticFileHandler* serveStatic(const char* uri, fs::FS& fs, const char* path, const char* cache_control = NULL);
};

bool ON_STA_FILTER(PsychicRequest* request);
bool ON_AP_FILTER(PsychicRequest* request);

// URI matching functions
bool psychic_uri_match_simple(const char* uri1, const char* uri2, size_t len2);
#define MATCH_SIMPLE   psychic_uri_match_simple
#define MATCH_WILDCARD httpd_uri_match_wildcard

#ifdef PSY_ENABLE_REGEX
bool psychic_uri_match_regex(const char* uri1, const char* uri2, size_t len2);
  #define MATCH_REGEX psychic_uri_match_regex
#endif

#endif // PsychicHttpServer_h