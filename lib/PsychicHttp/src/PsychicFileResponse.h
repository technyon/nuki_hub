#ifndef PsychicFileResponse_h
#define PsychicFileResponse_h

#include "PsychicCore.h"
#include "PsychicResponse.h"

class PsychicRequest;

class PsychicFileResponse: public PsychicResponse
{
  using File = fs::File;
  using FS = fs::FS;
  private:
    File _content;
    void _setContentType(const String& path);
  public:
    PsychicFileResponse(PsychicRequest *request, FS &fs, const String& path, const String& contentType=String(), bool download=false);
    PsychicFileResponse(PsychicRequest *request, File content, const String& path, const String& contentType=String(), bool download=false);
    ~PsychicFileResponse();
    esp_err_t send();
};

#endif // PsychicFileResponse_h