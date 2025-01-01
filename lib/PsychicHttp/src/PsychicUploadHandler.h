#ifndef PsychicUploadHandler_h
#define PsychicUploadHandler_h

#include "MultipartProcessor.h"
#include "PsychicCore.h"
#include "PsychicHttpServer.h"
#include "PsychicRequest.h"
#include "PsychicWebHandler.h"

/*
 * HANDLER :: Can be attached to any endpoint or as a generic request handler.
 */

class PsychicUploadHandler : public PsychicWebHandler
{
  protected:
    esp_err_t _basicUploadHandler(PsychicRequest* request);
    esp_err_t _multipartUploadHandler(PsychicRequest* request);

    PsychicUploadCallback _uploadCallback;

  public:
    PsychicUploadHandler();
    ~PsychicUploadHandler();

    bool canHandle(PsychicRequest* request) override;
    esp_err_t handleRequest(PsychicRequest* request, PsychicResponse* response) override;

    PsychicUploadHandler* onUpload(PsychicUploadCallback fn);
};

#endif // PsychicUploadHandler_h