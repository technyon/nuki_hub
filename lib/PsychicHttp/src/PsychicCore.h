#ifndef PsychicCore_h
#define PsychicCore_h

#define PH_TAG "psychic"

#ifndef FILE_CHUNK_SIZE
  #define FILE_CHUNK_SIZE 8 * 1024
#endif

#ifndef STREAM_CHUNK_SIZE
  #define STREAM_CHUNK_SIZE 1024
#endif

#ifndef MAX_UPLOAD_SIZE
  #define MAX_UPLOAD_SIZE (2048 * 1024) // 2MB
#endif

#ifndef MAX_REQUEST_BODY_SIZE
  #define MAX_REQUEST_BODY_SIZE (16 * 1024) // 16K
#endif

// Max outgoing WebSocket frames a single client may have queued-but-not-yet-sent
// before further sends to that client are dropped. Bounds the heap a stalled
// client (WiFi roam, out of range, half-open socket) can consume regardless of
// broadcast rate. Define as 0 to disable the cap entirely.
#ifndef PSYCHIC_WS_MAX_PENDING_FRAMES
  #define PSYCHIC_WS_MAX_PENDING_FRAMES 8
#endif

#ifdef ARDUINO
  #include "FS.h"
  #include <Arduino.h>
#endif
#include "esp_random.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <esp_http_server.h>
#include <esp_idf_version.h>
#include <mbedtls/base64.h>
#include <algorithm>
#include <esp_log.h>
#include <functional>
#include <list>
#include <map>
#include <mbedtls/base64.h>
#if ESP_IDF_VERSION_MAJOR >= 6
  #include <esp_rom_md5.h>
#else
  #include <mbedtls/md5.h>
#endif
#include <string>

#ifdef PSY_DEVMODE
  #include "ArduinoTrace.h"
#endif

enum HTTPAuthMethod {
  BASIC_AUTH,
  DIGEST_AUTH
};

#ifdef ARDUINO
String urlEncode(const char* str);
String urlDecode(const char* encoded);
#else
std::string urlEncode(const char* str);
std::string urlDecode(const char* encoded);
#endif

// Bounds-safe substring: clamps pos to the string length so it never throws.
// Arduino String::substring() silently clamped out-of-range positions; std::string::substr()
// throws std::out_of_range instead, and C++ exceptions are disabled on ESP-IDF builds, so an
// out-of-range substr on attacker-controlled input (e.g. a malformed multipart header) would
// abort and reboot the device. Use this for any substr whose offset is derived from parsed input.
// (len is left to std::string::substr, which already clamps it to the remaining length.)
inline std::string psychicSubstr(const std::string& str, size_t pos, size_t len = std::string::npos)
{
  if (pos > str.length())
    return std::string();
  return str.substr(pos, len);
}

class PsychicHttpServer;
class PsychicRequest;
class PsychicResponse;
class PsychicWebSocketRequest;
class PsychicClient;

// filter function definition
typedef std::function<bool(PsychicRequest* request)> PsychicRequestFilterFunction;

// middleware function definition
typedef std::function<esp_err_t()> PsychicMiddlewareNext;
typedef std::function<esp_err_t(PsychicRequest* request, PsychicResponse* response, PsychicMiddlewareNext next)> PsychicMiddlewareCallback;

// client connect callback
typedef std::function<void(PsychicClient* client)> PsychicClientCallback;

// callback definitions
typedef std::function<esp_err_t(PsychicRequest* request, PsychicResponse* response)> PsychicHttpRequestCallback;
typedef std::function<esp_err_t(PsychicRequest* request, PsychicResponse* response, JsonVariant& json)> PsychicJsonRequestCallback;
#ifdef ARDUINO
typedef std::function<esp_err_t(PsychicRequest* request, const String& filename, uint64_t index, uint8_t* data, size_t len, bool final)> PsychicUploadCallback;
#else
typedef std::function<esp_err_t(PsychicRequest* request, const char* filename, uint64_t index, uint8_t* data, size_t len, bool final)> PsychicUploadCallback;
#endif

struct HTTPHeader {
    std::string field;
    std::string value;
};

class DefaultHeaders
{
    std::list<HTTPHeader> _headers;

  public:
    DefaultHeaders() {}

    void addHeader(const char* field, const char* value)
    {
      _headers.push_back({field, value});
    }

#ifdef ARDUINO
    void addHeader(const String& field, const String& value)
    {
      _headers.push_back({field.c_str(), value.c_str()});
    }
#endif

    const std::list<HTTPHeader>& getHeaders() { return _headers; }

    // delete the copy constructor, singleton class
    DefaultHeaders(DefaultHeaders const&) = delete;
    DefaultHeaders& operator=(DefaultHeaders const&) = delete;

    // single static class interface
    static DefaultHeaders& Instance()
    {
      static DefaultHeaders instance;
      return instance;
    }
};

#endif // PsychicCore_h