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

typedef std::map<String, String> SessionData;

enum Disposition {
  NONE,
  INLINE,
  ATTACHMENT,
  FORM_DATA
};

struct ContentDisposition {
    Disposition disposition;
    String filename;
    String name;
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
    String _uri;
    String _query;
    String _body;
    esp_err_t _bodyParsed = ESP_ERR_NOT_FINISHED;
    esp_err_t _paramsParsed = ESP_ERR_NOT_FINISHED;

    std::list<PsychicWebParameter*> _params;

    PsychicResponse* _response;

    void _setUri(const char* uri);
    void _addParams(const String& params, bool post);
    void _parseGETParams();
    void _parsePOSTParams();

    const String _extractParam(const String& authReq, const String& param, const char delimit);
    const String _getRandomHexString();

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

    const String header(const char* name);
    bool hasHeader(const char* name);

    static void freeSession(void* ctx);
    bool hasSessionKey(const String& key);
    const String getSessionKey(const String& key);
    void setSessionKey(const String& key, const String& value);

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
    String getCookie(const char* key);

    http_method method();       // returns the HTTP method used as enum value (eg. HTTP_GET)
    const String methodStr();   // returns the HTTP method used as a string (eg. "GET")
    const String path();        // returns the request path (eg /page?foo=bar returns "/page")
    const String& uri();        // returns the full request uri (eg /page?foo=bar)
    const String& query();      // returns the request query data (eg /page?foo=bar returns "foo=bar")
    const String host();        // returns the requested host (request to http://psychic.local/foo will return "psychic.local")
    const String contentType(); // returns the Content-Type header value
    size_t contentLength();     // returns the Content-Length header value
    const String& body();       // returns the body of the request
    const ContentDisposition getContentDisposition();
    const char* version() { return "HTTP/1.1"; }

    const String& queryString() { return query(); } // compatability function.  same as query()
    const String& url() { return uri(); }           // compatability function.  same as uri()

    void loadParams();
    PsychicWebParameter* addParam(PsychicWebParameter* param);
    PsychicWebParameter* addParam(const String& name, const String& value, bool decode = true, bool post = false);
    bool hasParam(const char* key);
    bool hasParam(const char* key, bool isPost, bool isFile = false);
    PsychicWebParameter* getParam(const char* name);
    PsychicWebParameter* getParam(const char* name, bool isPost, bool isFile = false);

    const String getFilename();

    bool authenticate(const char* username, const char* password);
    esp_err_t requestAuthentication(HTTPAuthMethod mode, const char* realm, const char* authFailMsg);
};

#endif // PsychicRequest_h