#ifndef PsychicFileResponse_h
#define PsychicFileResponse_h

#include "PsychicCore.h"
#include "PsychicFS.h"
#include "PsychicResponse.h"

class PsychicRequest;

class PsychicFileResponse : public PsychicResponseDelegate
{
  private:
    void _setContentTypeFromPath(const char* path);
    void _initFromFS(psychic::FS fs, const char* path, const char* contentType, bool download);

  protected:
    psychic::File _content;

  public:
    PsychicFileResponse(PsychicResponse* response, const char* path,
      const char* contentType = nullptr, bool download = false);
    PsychicFileResponse(PsychicResponse* response, psychic::FS fs, const char* path,
      const char* contentType = nullptr, bool download = false);

#ifdef ARDUINO
    PsychicFileResponse(PsychicResponse* response, fs::FS& fs, const String& path,
      const String& contentType = String(), bool download = false);
    PsychicFileResponse(PsychicResponse* response, fs::File content, const String& path,
      const String& contentType = String(), bool download = false);
#endif

    ~PsychicFileResponse();
    esp_err_t send();
};

#endif // PsychicFileResponse_h