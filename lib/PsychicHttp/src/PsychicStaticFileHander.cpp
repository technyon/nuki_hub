#include "PsychicStaticFileHandler.h"

/*************************************/
/*  PsychicStaticFileHandler         */
/*************************************/

void PsychicStaticFileHandler::_initPath()
{
  // Ensure leading '/'
  if (_uri.empty() || _uri[0] != '/')
    _uri = "/" + _uri;
  if (_path.empty() || _path[0] != '/')
    _path = "/" + _path;

  // If path ends with '/' we assume a hint that this is a directory to improve performance.
  // However - if it does not end with '/' we can't assume a file, path can still be a directory.
  _isDir = _path.back() == '/';

  // Remove the trailing '/' so we can handle default file.
  // Notice that root will be "" not "/"
  if (_uri.back() == '/')
    _uri.pop_back();
  if (_path.back() == '/')
    _path.pop_back();

  // Reset stats
  _gzipFirst = false;
  _gzipStats = 0xF8;
}

#ifdef ARDUINO
PsychicStaticFileHandler::PsychicStaticFileHandler(const char* uri, fs::FS& fs, const char* path, const char* cache_control)
    : _fs(fs), _uri(uri), _path(path), _default_file("index.html"), _cache_control(cache_control ? cache_control : ""), _last_modified("")
{
  _initPath();
}
#endif

PsychicStaticFileHandler::PsychicStaticFileHandler(const char* uri, const char* path, const char* cache_control)
    : _uri(uri), _path(path), _default_file("index.html"), _cache_control(cache_control ? cache_control : ""), _last_modified("")
{
  _initPath();
}

PsychicStaticFileHandler* PsychicStaticFileHandler::setIsDir(bool isDir)
{
  _isDir = isDir;
  return this;
}

PsychicStaticFileHandler* PsychicStaticFileHandler::setDefaultFile(const char* filename)
{
  _default_file = filename;
  return this;
}

PsychicStaticFileHandler* PsychicStaticFileHandler::setCacheControl(const char* cache_control)
{
  _cache_control = cache_control;
  return this;
}

PsychicStaticFileHandler* PsychicStaticFileHandler::setLastModified(const char* last_modified)
{
  _last_modified = last_modified;
  return this;
}

PsychicStaticFileHandler* PsychicStaticFileHandler::setLastModified(struct tm* last_modified)
{
  char result[30];
  strftime(result, 30, "%a, %d %b %Y %H:%M:%S %Z", last_modified);
  return setLastModified((const char*)result);
}

bool PsychicStaticFileHandler::canHandle(PsychicRequest* request)
{
  if (request->method() != HTTP_GET) {
    ESP_LOGD(PH_TAG, "Request %s refused by PsychicStaticFileHandler: %s", request->uriCStr(), request->methodStrCStr());
    return false;
  }

  if (strncmp(request->uriCStr(), _uri.c_str(), _uri.length()) != 0) {
    ESP_LOGD(PH_TAG, "Request %s refused by PsychicStaticFileHandler: does not start with %s", request->uriCStr(), _uri.c_str());
    return false;
  }

  if (_getFile(request)) {
    return true;
  }

  ESP_LOGD(PH_TAG, "Request %s refused by PsychicStaticFileHandler: file not found", request->uriCStr());
  return false;
}

bool PsychicStaticFileHandler::_getFile(PsychicRequest* request)
{
  // Skip past the mounted prefix (_uri) to get the path relative to the mount point.
  // uriCStr() + _uri.size() is pointer arithmetic: it advances the char* past the
  // prefix, so the string is built from the tail only.
  // e.g. uri "/static/css/app.css" with mount "/static/" -> "css/app.css"
  std::string path(request->uriCStr() + _uri.size());

  // Drop any query string: the file path ends at the first '?'.
  // e.g. "css/app.css?v=2" -> "css/app.css"
  size_t queryStart = path.find('?');
  if (queryStart != std::string::npos)
    path.erase(queryStart);

  // URL-decode so encoded file names (e.g. "my%20file.txt") resolve correctly.
  path = std::string(urlDecode(path.c_str()).c_str());

  // Reject any path that contains directory traversal sequences.
  // This runs after decoding so encoded sequences (e.g. "%2e%2e") can't slip past.
  if (path.find("..") != std::string::npos)
    return false;

  // We can skip the file check and look for default if request is to the root
  // of a directory or that request path ends with '/'
  bool canSkipFileCheck = (_isDir && path.empty()) || (!path.empty() && path.back() == '/');

  path = _path + path;

  // Do we have a file or .gz file
  if (!canSkipFileCheck && _fileExists(path))
    return true;

  // Can't handle if not default file
  if (_default_file.empty())
    return false;

  // Try to add default file; ensure there is a trailing '/' on the path.
  if (path.empty() || path.back() != '/')
    path += '/';
  path += _default_file;

  return _fileExists(path);
}

bool PsychicStaticFileHandler::_fileExists(const std::string& path)
{
  bool fileFound = false;
  bool gzipFound = false;

  std::string gzip = path + ".gz";

  if (_gzipFirst) {
    _file = _fs.open(gzip.c_str(), "r");
    gzipFound = (bool)_file;
    if (!gzipFound) {
      _file = _fs.open(path.c_str(), "r");
      fileFound = (bool)_file;
    }
  } else {
    _file = _fs.open(path.c_str(), "r");
    fileFound = (bool)_file;
    if (!fileFound) {
      _file = _fs.open(gzip.c_str(), "r");
      gzipFound = (bool)_file;
    }
  }

  bool found = fileFound || gzipFound;

  if (found) {
    _filename = path;

    // Calculate gzip statistic
    _gzipStats = (_gzipStats << 1) + (gzipFound ? 1 : 0);
    if (_gzipStats == 0x00)
      _gzipFirst = false; // All files are not gzip
    else if (_gzipStats == 0xFF)
      _gzipFirst = true; // All files are gzip
    else
      _gzipFirst = _countBits(_gzipStats) > 4; // IF we have more gzip files — try gzip first
  }

  ESP_LOGD(PH_TAG, "PsychicStaticFileHandler _fileExists(%s): %d", path.c_str(), found);

  return found;
}

uint8_t PsychicStaticFileHandler::_countBits(const uint8_t value) const
{
  uint8_t w = value;
  uint8_t n;
  for (n = 0; w != 0; n++)
    w &= w - 1;
  return n;
}

esp_err_t PsychicStaticFileHandler::handleRequest(PsychicRequest* request, PsychicResponse* res)
{
  if (_file) {
    // is it not modified?
    std::string etag = std::to_string(_file.size());
    if (_last_modified.length() && _last_modified == request->headerCStr("If-Modified-Since")) {
      _file.close();
      res->send(304); // Not modified
    }
    // does our Etag match?
    else if (_cache_control.length() && request->hasHeader("If-None-Match") && etag == request->headerCStr("If-None-Match")) {
      _file.close();

      res->addHeader("Cache-Control", _cache_control.c_str());
      res->addHeader("ETag", etag.c_str());
      res->setCode(304);
      res->send();
    }
    // nope, send them the full file.
    else {
      // PsychicFileResponse reopens the file from _fs/_filename, so release our
      // own handle first to avoid holding two descriptors per served file.
      _file.close();
      PsychicFileResponse response(res, _fs, _filename.c_str());

      if (_last_modified.length())
        response.addHeader("Last-Modified", _last_modified.c_str());
      if (_cache_control.length()) {
        response.addHeader("Cache-Control", _cache_control.c_str());
        response.addHeader("ETag", etag.c_str());
      }

      return response.send();
    }
  } else {
    return res->send(404);
  }

  return ESP_OK;
}
