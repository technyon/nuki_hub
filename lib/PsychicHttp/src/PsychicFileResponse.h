#ifndef PsychicFileResponse_h
#define PsychicFileResponse_h

#include "PsychicCore.h"
#include "PsychicResponse.h"

class PsychicRequest;

class PsychicFileResponse : public PsychicResponseDelegate
{
    using File = fs::File;
    using FS = fs::FS;

  protected:
    File _content;
    void _setContentTypeFromPath(const String& path);

  public:
    PsychicFileResponse(PsychicResponse* response, FS& fs, const String& path, const String& contentType = String(), bool download = false);
    PsychicFileResponse(PsychicResponse* response, File content, const String& path, const String& contentType = String(), bool download = false);
    ~PsychicFileResponse();
    esp_err_t send();
};

#endif // PsychicFileResponse_h