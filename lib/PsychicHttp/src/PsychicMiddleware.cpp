#include "PsychicMiddleware.h"

esp_err_t PsychicMiddlewareFunction::run(PsychicRequest* request, PsychicResponse* response, PsychicMiddlewareNext next)
{
  return _fn(request, request->response(), next);
}
