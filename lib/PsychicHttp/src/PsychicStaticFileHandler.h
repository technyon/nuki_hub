#ifndef PsychicStaticFileHandler_h
#define PsychicStaticFileHandler_h

#include "PsychicCore.h"
#include "PsychicFS.h"
#include "PsychicFileResponse.h"
#include "PsychicRequest.h"
#include "PsychicResponse.h"
#include "PsychicWebHandler.h"

class PsychicStaticFileHandler : public PsychicWebHandler
{
  private:
    void _initPath();
    bool _getFile(PsychicRequest* request);
    bool _fileExists(const std::string& path);
    uint8_t _countBits(const uint8_t value) const;

  protected:
    psychic::FS _fs;
    psychic::File _file;
    std::string _filename;
    std::string _uri;
    std::string _path;
    std::string _default_file;
    std::string _cache_control;
    std::string _last_modified;
    bool _isDir;
    bool _gzipFirst;
    uint8_t _gzipStats;

  public:
#ifdef ARDUINO
    PsychicStaticFileHandler(const char* uri, fs::FS& fs, const char* path, const char* cache_control);
#endif
    // IDF / POSIX-VFS constructor — path must be an absolute VFS mount path
    // (e.g. "/littlefs/www"). Users must register the VFS partition before calling.
    PsychicStaticFileHandler(const char* uri, const char* path, const char* cache_control);
    bool canHandle(PsychicRequest* request) override;
    esp_err_t handleRequest(PsychicRequest* request, PsychicResponse* response) override;
    PsychicStaticFileHandler* setIsDir(bool isDir);
    PsychicStaticFileHandler* setDefaultFile(const char* filename);
    PsychicStaticFileHandler* setCacheControl(const char* cache_control);
    PsychicStaticFileHandler* setLastModified(const char* last_modified);
    PsychicStaticFileHandler* setLastModified(struct tm* last_modified);
};

#endif /* PsychicHttp_h */