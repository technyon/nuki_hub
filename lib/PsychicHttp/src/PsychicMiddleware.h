#ifndef PsychicMiddleware_h
#define PsychicMiddleware_h

#include "PsychicCore.h"
#include "PsychicRequest.h"
#include "PsychicResponse.h"

class PsychicMiddlewareChain;
/*
 * PsychicMiddleware :: fancy callback wrapper for handling requests and responses.
 * */

class PsychicMiddleware
{
  public:
    virtual ~PsychicMiddleware() {}
    virtual esp_err_t run(PsychicRequest* request, PsychicResponse* response, PsychicMiddlewareNext next)
    {
      return next();
    }

  private:
    friend PsychicMiddlewareChain;
    bool _freeOnRemoval = false;
};

class PsychicMiddlewareFunction : public PsychicMiddleware
{
  public:
    PsychicMiddlewareFunction(PsychicMiddlewareCallback fn) : _fn(fn) { assert(_fn); }
    esp_err_t run(PsychicRequest* request, PsychicResponse* response, PsychicMiddlewareNext next) override;

  protected:
    PsychicMiddlewareCallback _fn;
};

#endif
