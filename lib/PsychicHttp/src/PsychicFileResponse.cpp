#include "PsychicFileResponse.h"
#include "PsychicRequest.h"
#include "PsychicResponse.h"
#include <http_status.h>

static inline bool endsWith(const char* str, const char* suffix)
{
  size_t slen = strlen(str), plen = strlen(suffix);
  return slen >= plen && strcmp(str + slen - plen, suffix) == 0;
}

// Shared implementation called by all path-based constructors.
void PsychicFileResponse::_initFromFS(psychic::FS fs, const char* path, const char* contentType, bool download)
{
  std::string spath(path);

  if (!download && !fs.exists(spath.c_str()) && fs.exists((spath + ".gz").c_str())) {
    spath += ".gz";
    addHeader("Content-Encoding", "gzip");
  }

  _content = fs.open(spath.c_str(), "r");
  setContentLength(_content.size());

  if (!contentType || !*contentType)
    _setContentTypeFromPath(path);
  else
    setContentType(contentType);

  const char* lastSlash = strrchr(path, '/');
  const char* filename = lastSlash ? lastSlash + 1 : path;
  std::string disposition = download ? "attachment" : "inline";
  disposition += "; filename=\"";
  disposition += filename;
  disposition += "\"";
  addHeader("Content-Disposition", disposition.c_str());
}

// PUBLIC API for IDF
// Open a file by its full VFS path (e.g. "/littlefs/index.html").
PsychicFileResponse::PsychicFileResponse(PsychicResponse* response, const char* path, const char* contentType, bool download)
    : PsychicResponseDelegate(response)
{
  _initFromFS(psychic::FS{}, path, contentType, download);
}

// INTERNAL used by PsychicStaticFileHandler (Arduino and IDF)
PsychicFileResponse::PsychicFileResponse(PsychicResponse* response, psychic::FS fs, const char* path, const char* contentType, bool download)
    : PsychicResponseDelegate(response)
{
  _initFromFS(fs, path, contentType, download);
}

#ifdef ARDUINO
// PUBLIC API for Arduino - replaces old ctor(FS&, String).
PsychicFileResponse::PsychicFileResponse(PsychicResponse* response, fs::FS& fs, const String& path, const String& contentType, bool download)
    : PsychicResponseDelegate(response)
{
  _initFromFS(psychic::FS(fs), path.c_str(), contentType.length() ? contentType.c_str() : nullptr, download);
}

// PUBLIC API for Arduino - replaces old ctor(File, String).
PsychicFileResponse::PsychicFileResponse(PsychicResponse* response, fs::File content, const String& path, const String& contentType, bool download)
    : PsychicResponseDelegate(response)
{
  if (!download && endsWith(content.name(), ".gz") && !endsWith(path.c_str(), ".gz"))
    addHeader("Content-Encoding", "gzip");

  _content = psychic::File(content);
  setContentLength(_content.size());

  if (contentType.length() == 0)
    _setContentTypeFromPath(path.c_str());
  else
    setContentType(contentType.c_str());

  const char* lastSlash = strrchr(path.c_str(), '/');
  const char* filename = lastSlash ? lastSlash + 1 : path.c_str();
  std::string disposition = download ? "attachment" : "inline";
  disposition += "; filename=\"";
  disposition += filename;
  disposition += "\"";
  addHeader("Content-Disposition", disposition.c_str());
}
#endif

PsychicFileResponse::~PsychicFileResponse()
{
  if (_content)
    _content.close();
}

void PsychicFileResponse::_setContentTypeFromPath(const char* p)
{
  const char* _contentType;

  if (endsWith(p, ".html"))
    _contentType = "text/html";
  else if (endsWith(p, ".htm"))
    _contentType = "text/html";
  else if (endsWith(p, ".css"))
    _contentType = "text/css";
  else if (endsWith(p, ".json"))
    _contentType = "application/json";
  else if (endsWith(p, ".js"))
    _contentType = "application/javascript";
  else if (endsWith(p, ".png"))
    _contentType = "image/png";
  else if (endsWith(p, ".gif"))
    _contentType = "image/gif";
  else if (endsWith(p, ".jpg"))
    _contentType = "image/jpeg";
  else if (endsWith(p, ".ico"))
    _contentType = "image/x-icon";
  else if (endsWith(p, ".svg"))
    _contentType = "image/svg+xml";
  else if (endsWith(p, ".eot"))
    _contentType = "font/eot";
  else if (endsWith(p, ".woff"))
    _contentType = "font/woff";
  else if (endsWith(p, ".woff2"))
    _contentType = "font/woff2";
  else if (endsWith(p, ".ttf"))
    _contentType = "font/ttf";
  else if (endsWith(p, ".xml"))
    _contentType = "text/xml";
  else if (endsWith(p, ".pdf"))
    _contentType = "application/pdf";
  else if (endsWith(p, ".zip"))
    _contentType = "application/zip";
  else if (endsWith(p, ".gz"))
    _contentType = "application/x-gzip";
  else
    _contentType = "text/plain";

  setContentType(_contentType);
}

esp_err_t PsychicFileResponse::send()
{
  esp_err_t err = ESP_OK;

  // just send small files directly
  size_t size = getContentLength();
  if (size < FILE_CHUNK_SIZE) {
    uint8_t* buffer = (uint8_t*)malloc(size);
    if (buffer == NULL && size > 0) {
      ESP_LOGE(PH_TAG, "Unable to allocate %zu bytes to send chunk", size);
      httpd_resp_send_err(request(), HTTPD_500_INTERNAL_SERVER_ERROR, "Unable to allocate memory.");
      return ESP_FAIL;
    }

    size_t readSize = _content.readBytes((char*)buffer, size);

    setContent(buffer, readSize);
    err = _response->send();

    free(buffer);
  } else {
    /* Retrieve the pointer to scratch buffer for temporary storage */
    char* chunk = (char*)malloc(FILE_CHUNK_SIZE);
    if (chunk == NULL) {
      ESP_LOGE(PH_TAG, "Unable to allocate %zu bytes to send chunk", (size_t)FILE_CHUNK_SIZE);
      httpd_resp_send_err(request(), HTTPD_500_INTERNAL_SERVER_ERROR, "Unable to allocate memory.");
      return ESP_FAIL;
    }

    // now the headers
    sendHeaders();

    size_t chunksize;
    do {
      /* Read file in chunks into the scratch buffer */
      chunksize = _content.readBytes(chunk, FILE_CHUNK_SIZE);
      if (chunksize > 0) {
        err = sendChunk((uint8_t*)chunk, chunksize);
        if (err != ESP_OK)
          break;
      }

      /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    // keep track of our memory
    free(chunk);

    if (err == ESP_OK) {
      ESP_LOGD(PH_TAG, "File sending complete");
      finishChunking();
    }
  }

  return err;
}
