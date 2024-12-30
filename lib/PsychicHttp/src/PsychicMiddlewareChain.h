#ifndef PsychicMiddlewareChain_h
#define PsychicMiddlewareChain_h

#include "PsychicCore.h"
#include "PsychicMiddleware.h"
#include "PsychicRequest.h"
#include "PsychicResponse.h"

/*
 * PsychicMiddlewareChain - handle tracking and executing our chain of middleware objects
 * */

class PsychicMiddlewareChain
{
  public:
    virtual ~PsychicMiddlewareChain();

    void addMiddleware(PsychicMiddleware* middleware);
    void addMiddleware(PsychicMiddlewareCallback fn);
    void removeMiddleware(PsychicMiddleware* middleware);

    esp_err_t runChain(PsychicRequest* request, PsychicMiddlewareNext finalizer);

  protected:
    std::list<PsychicMiddleware*> _middleware;
};

#endif