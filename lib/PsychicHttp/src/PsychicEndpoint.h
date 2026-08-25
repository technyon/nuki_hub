#ifndef PsychicEndpoint_h
#define PsychicEndpoint_h

#include "PsychicCore.h"

class PsychicHandler;
class PsychicMiddleware;

#ifdef ENABLE_ASYNC
  #include "async_worker.h"
#endif

class PsychicEndpoint
{
    friend PsychicHttpServer;

  private:
    PsychicHttpServer* _server;
    std::string _uri;
    int _method;
    PsychicHandler* _handler;
    httpd_uri_match_func_t _uri_match_fn = nullptr; // use this change the endpoint matching function.

  public:
    PsychicEndpoint();
    PsychicEndpoint(PsychicHttpServer* server, int method, const char* uri);
    ~PsychicEndpoint();

    PsychicEndpoint* setHandler(PsychicHandler* handler);
    PsychicHandler* handler();

    httpd_uri_match_func_t getURIMatchFunction();
    void setURIMatchFunction(httpd_uri_match_func_t match_fn);

    bool matches(const char* uri);

    // called to process this endpoint with its middleware chain
    esp_err_t process(PsychicRequest* request);

    PsychicEndpoint* addFilter(PsychicRequestFilterFunction fn);

    PsychicEndpoint* addMiddleware(PsychicMiddleware* middleware);
    PsychicEndpoint* addMiddleware(PsychicMiddlewareCallback fn);
    void removeMiddleware(PsychicMiddleware* middleware);

#ifdef ARDUINO
    String uri();
#else
    const char* uri();
#endif
    // Always returns const char* regardless of platform — use in library internals.
    const char* uriCStr();

    static esp_err_t requestCallback(httpd_req_t* req);
};

#endif // PsychicEndpoint_h