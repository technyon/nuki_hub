#ifndef PsychicRequest_h
#define PsychicRequest_h

#include "PsychicClient.h"
#include "PsychicCore.h"
#include "PsychicEndpoint.h"
#include "PsychicHttpServer.h"
#include "PsychicWebParameter.h"

#ifdef PSY_ENABLE_REGEX
  #include <regex>
#endif

#ifdef ARDUINO
typedef std::map<String, String> SessionData;
#else
typedef std::map<std::string, std::string> SessionData;
#endif

enum Disposition {
  NONE,
  INLINE,
  ATTACHMENT,
  FORM_DATA
};

struct ContentDisposition {
    Disposition disposition;
#ifdef ARDUINO
    String filename;
    String name;
#else
    std::string filename;
    std::string name;
#endif
};

class PsychicRequest
{
    friend PsychicHttpServer;
    friend PsychicResponse;

  protected:
    PsychicHttpServer* _server;
    httpd_req_t* _req;
    SessionData* _session;
    PsychicClient* _client;
    PsychicEndpoint* _endpoint;

    http_method _method;
    std::string _uri;
    std::string _query;
    std::string _body;
    // _tmp and _filename back const char* return values — they must be class-level
    // (not method-local) because the returned pointer must remain valid after the
    // method returns. _tmp is a shared single-use buffer: consume the returned
    // pointer immediately and do not hold it across another getter call.
    std::string _tmp;
    std::string _filename;
    esp_err_t _bodyParsed = ESP_ERR_NOT_FINISHED;
    esp_err_t _paramsParsed = ESP_ERR_NOT_FINISHED;

    std::list<PsychicWebParameter*> _params;

    PsychicResponse* _response;

    void _setUri(const char* uri);
    void _addParams(const char* params, bool post);
    void _parseGETParams();
    void _parsePOSTParams();

    std::string _extractParam(const char* authReq, const char* param, const char delimit);
    std::string _getRandomHexString();

    // Internal helper: always returns const char* backed by _tmp. Used by the
    // public header() overloads and by internal library code that needs a raw
    // pointer regardless of platform.
    const char* _getHeader(const char* name);

  public:
    PsychicRequest(PsychicHttpServer* server, httpd_req_t* req);
    virtual ~PsychicRequest();

    void* _tempObject;

    PsychicHttpServer* server();
    httpd_req_t* request();
    virtual PsychicClient* client();

    PsychicEndpoint* endpoint();
    void setEndpoint(PsychicEndpoint* endpoint);

#ifdef PSY_ENABLE_REGEX
    bool getRegexMatches(std::smatch& matches, bool use_full_uri = false);
#endif

    bool isMultipart();
    esp_err_t loadBody();

#ifdef ARDUINO
    String header(const char* name);
#else
    const char* header(const char* name);
#endif
    // Always returns const char* regardless of platform — use this in library
    // internals (MultipartProcessor, EventSource, etc.) that need a raw pointer.
    const char* headerCStr(const char* name);
    bool hasHeader(const char* name);

    static void freeSession(void* ctx);
    bool hasSessionKey(const char* key);
#ifdef ARDUINO
    String getSessionKey(const char* key);
#else
    const char* getSessionKey(const char* key);
#endif
    void setSessionKey(const char* key, const char* value);
#ifdef ARDUINO
    bool hasSessionKey(const String& key) { return hasSessionKey(key.c_str()); }
    String getSessionKey(const String& key) { return getSessionKey(key.c_str()); }
    void setSessionKey(const String& key, const String& value) { setSessionKey(key.c_str(), value.c_str()); }
#endif

    bool hasCookie(const char* key, size_t* size = nullptr);

    PsychicResponse* response() { return _response; }
    void replaceResponse(PsychicResponse* response);
    void addResponseHeader(const char* key, const char* value);
    std::list<HTTPHeader>& getResponseHeaders();

    /**
     * @brief   Get the value string of a cookie value from the "Cookie" request headers by cookie name.
     *
     * @param[in]       key             The cookie name to be searched in the request
     * @param[out]      buffer          Pointer to the buffer into which the value of cookie will be copied if the cookie is found
     * @param[inout]    size            Pointer to size of the user buffer "val". This variable will contain cookie length if
     *                                  ESP_OK is returned and required buffer length in case ESP_ERR_HTTPD_RESULT_TRUNC is returned.
     *
     * @return
     *  - ESP_OK : Key is found in the cookie string and copied to buffer. The value is null-terminated.
     *  - ESP_ERR_NOT_FOUND          : Key not found
     *  - ESP_ERR_INVALID_ARG        : Null arguments
     *  - ESP_ERR_HTTPD_RESULT_TRUNC : Value string truncated
     *  - ESP_ERR_NO_MEM             : Memory allocation failure
     */
    esp_err_t getCookie(const char* key, char* buffer, size_t* size);

    // convenience / lazy function for getting cookies.
#ifdef ARDUINO
    String getCookie(const char* key);
#else
    const char* getCookie(const char* key);
#endif

    http_method method(); // returns the HTTP method used as enum value (eg. HTTP_GET)
#ifdef ARDUINO
    String methodStr(); // returns the HTTP method used as a string (eg. "GET")
    String path();      // returns the request path (eg /page?foo=bar returns "/page")
    String uri();       // returns the full request uri (eg /page?foo=bar)
    String query();     // returns the request query data (eg /page?foo=bar returns "foo=bar")
#else
    const char* methodStr(); // returns the HTTP method used as a string (eg. "GET")
    const char* path();      // returns the request path (eg /page?foo=bar returns "/page")
    const char* uri();       // returns the full request uri (eg /page?foo=bar)
    const char* query();     // returns the request query data (eg /page?foo=bar returns "foo=bar")
#endif
    // Always returns const char* regardless of platform — use these in library internals.
    const char* methodStrCStr();
    const char* pathCStr();
    const char* uriCStr();
    const char* queryCStr();
#ifdef ARDUINO
    String host();        // returns the requested host (request to http://psychic.local/foo will return "psychic.local")
    String contentType(); // returns the Content-Type header value
#else
    const char* host();        // returns the requested host (request to http://psychic.local/foo will return "psychic.local")
    const char* contentType(); // returns the Content-Type header value
#endif
    size_t contentLength(); // returns the Content-Length header value
#ifdef ARDUINO
    String body(); // returns the body of the request
#else
    const char* body(); // returns the body of the request
#endif
    const char* bodyCStr(); // Always returns const char* regardless of platform — use in library internals.
    const ContentDisposition getContentDisposition();
    const char* version() { return "HTTP/1.1"; }

#ifdef ARDUINO
    String queryString() { return query(); } // compatability function.  same as query()
    String url() { return uri(); }           // compatability function.  same as uri()
#else
    const char* queryString() { return query(); } // compatability function.  same as query()
    const char* url() { return uri(); }           // compatability function.  same as uri()
#endif

    void loadParams();
    PsychicWebParameter* addParam(PsychicWebParameter* param);
    PsychicWebParameter* addParam(const char* name, const char* value, bool decode = true, bool post = false);
#ifdef ARDUINO
    PsychicWebParameter* addParam(const String& name, const String& value, bool decode = true, bool post = false) { return addParam(name.c_str(), value.c_str(), decode, post); }
#endif
    bool hasParam(const char* key);
    bool hasParam(const char* key, bool isPost, bool isFile = false);
    PsychicWebParameter* getParam(const char* name);
    PsychicWebParameter* getParam(const char* name, bool isPost, bool isFile = false);
#ifdef ARDUINO
    String getParam(const char* name, const char* defaultValue);
#else
    const char* getParam(const char* name, const char* defaultValue);
#endif

#ifdef ARDUINO
    String getFilename();
#else
    const char* getFilename();
#endif
    // Always returns const char* regardless of platform — use in library internals.
    const char* getFilenameCStr();
    const char* getSessionKeyCStr(const char* key);

    bool authenticate(const char* username, const char* password, bool passwordIsHashed = false);
    esp_err_t requestAuthentication(HTTPAuthMethod mode, const char* realm, const char* authFailMsg);
};

#endif // PsychicRequest_h
