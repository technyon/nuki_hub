#include "PsychicMiddlewareChain.h"

PsychicMiddlewareChain::~PsychicMiddlewareChain()
{
  for (auto middleware : _middleware)
    if (middleware->_freeOnRemoval)
      delete middleware;
  _middleware.clear();
}

void PsychicMiddlewareChain::addMiddleware(PsychicMiddleware* middleware)
{
  _middleware.push_back(middleware);
}

void PsychicMiddlewareChain::addMiddleware(PsychicMiddlewareCallback fn)
{
  PsychicMiddlewareFunction* closure = new PsychicMiddlewareFunction(fn);
  closure->_freeOnRemoval = true;
  _middleware.push_back(closure);
}

void PsychicMiddlewareChain::removeMiddleware(PsychicMiddleware* middleware)
{
  _middleware.remove(middleware);
  if (middleware->_freeOnRemoval)
    delete middleware;
}

esp_err_t PsychicMiddlewareChain::runChain(PsychicRequest* request, PsychicMiddlewareNext finalizer)
{
  if (_middleware.size() == 0)
    return finalizer();

  PsychicMiddlewareNext next;
  std::list<PsychicMiddleware*>::iterator it = _middleware.begin();

  next = [this, &next, &it, request, finalizer]() {
    if (it == _middleware.end())
      return finalizer();
    PsychicMiddleware* m = *it;
    it++;
    return m->run(request, request->response(), next);
  };

  return next();
}