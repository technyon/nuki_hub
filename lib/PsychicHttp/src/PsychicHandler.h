#ifndef PsychicHandler_h
#define PsychicHandler_h

#include "PsychicCore.h"
#include "PsychicRequest.h"

class PsychicEndpoint;
class PsychicHttpServer;
class PsychicMiddleware;
class PsychicMiddlewareChain;

/*
 * HANDLER :: Can be attached to any endpoint or as a generic request handler.
 */

class PsychicHandler
{
    friend PsychicEndpoint;

  protected:
    PsychicHttpServer* _server = nullptr;
    PsychicMiddlewareChain* _chain = nullptr;
    std::list<PsychicRequestFilterFunction> _filters;

    String _subprotocol;

    std::list<PsychicClient*> _clients;

  public:
    PsychicHandler();
    virtual ~PsychicHandler();

    virtual bool isWebSocket() { return false; };

    void setSubprotocol(const String& subprotocol);
    const char* getSubprotocol() const;

    PsychicClient* checkForNewClient(PsychicClient* client);
    void checkForClosedClient(PsychicClient* client);

    virtual void addClient(PsychicClient* client);
    virtual void removeClient(PsychicClient* client);
    virtual PsychicClient* getClient(int socket);
    virtual PsychicClient* getClient(PsychicClient* client);
    virtual void openCallback(PsychicClient* client) {};
    virtual void closeCallback(PsychicClient* client) {};

    bool hasClient(PsychicClient* client);
    int count() { return _clients.size(); };
    const std::list<PsychicClient*>& getClientList();

    // called to process this handler with its middleware chain and filers
    esp_err_t process(PsychicRequest* request);

    //bool filter(PsychicRequest* request);
    PsychicHandler* addFilter(PsychicRequestFilterFunction fn);
    bool filter(PsychicRequest* request);

    PsychicHandler* addMiddleware(PsychicMiddleware* middleware);
    PsychicHandler* addMiddleware(PsychicMiddlewareCallback fn);
    void removeMiddleware(PsychicMiddleware *middleware);

    // derived classes must implement these functions
    virtual bool canHandle(PsychicRequest* request) { return true; };
    virtual esp_err_t handleRequest(PsychicRequest* request, PsychicResponse* response) { return HTTPD_404_NOT_FOUND; };
};

#endif