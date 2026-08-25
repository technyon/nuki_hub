#ifndef PsychicResponse_h
#define PsychicResponse_h

#include "PsychicCore.h"
#include "time.h"

class PsychicRequest;

class PsychicResponse
{
  protected:
    PsychicRequest* _request;

    int _code;
    char _status[60];
    std::list<HTTPHeader> _headers;
    String _contentType;
    int64_t _contentLength;
    const char* _body;

  public:
    PsychicResponse(PsychicRequest* request);
    virtual ~PsychicResponse();

    const char* version() { return "HTTP/1.1"; }

    void setCode(int code);
    int getCode() { return _code; }
    
    void setContentType(const char* contentType);
    String& getContentType() { return _contentType; }

    void setContentLength(int64_t contentLength) { _contentLength = contentLength; }
    int64_t getContentLength(int64_t contentLength) { return _contentLength; }

    void addHeader(const char* field, const char* value);
    std::list<HTTPHeader>& headers() { return _headers; }

    void setCookie(const char* key, const char* value, unsigned long max_age = 60 * 60 * 24 * 30, const char* extras = "");

    void setContent(const char* content);
    void setContent(const uint8_t* content, size_t len);

    const char* getContent();
    size_t getContentLength();

    virtual esp_err_t send();
    void sendHeaders();
    esp_err_t sendChunk(uint8_t* chunk, size_t chunksize);
    esp_err_t finishChunking();

    esp_err_t redirect(const char* url);
    esp_err_t send(int code);
    esp_err_t send(const char* content);
    esp_err_t send(const char* contentType, const char* content);
    esp_err_t send(int code, const char* contentType, const char* content);
    esp_err_t send(int code, const char* contentType, const uint8_t* content, size_t len);
    esp_err_t error(httpd_err_code_t code, const char* message);

    httpd_req_t* request();
};

class PsychicResponseDelegate
{
  protected:
    PsychicResponse* _response;

  public:
    PsychicResponseDelegate(PsychicResponse* response) : _response(response) {}
    virtual ~PsychicResponseDelegate() {}

    const char* version() { return _response->version(); }

    void setCode(int code) { _response->setCode(code); }

    void setContentType(const char* contentType) { _response->setContentType(contentType); }
    String& getContentType() { return _response->getContentType(); }

    void setContentLength(int64_t contentLength) { _response->setContentLength(contentLength); }
    int64_t getContentLength(int64_t contentLength) { return _response->getContentLength(); }

    void addHeader(const char* field, const char* value) { _response->addHeader(field, value); }

    void setCookie(const char* key, const char* value, unsigned long max_age = 60 * 60 * 24 * 30, const char* extras = "") { _response->setCookie(key, value, max_age, extras); }

    void setContent(const char* content) { _response->setContent(content); }
    void setContent(const uint8_t* content, size_t len) { _response->setContent(content, len); }

    const char* getContent() { return _response->getContent(); }
    size_t getContentLength() { return _response->getContentLength(); }

    esp_err_t send() { return _response->send(); }
    void sendHeaders() { _response->sendHeaders(); }

    esp_err_t sendChunk(uint8_t* chunk, size_t chunksize) { return _response->sendChunk(chunk, chunksize); }
    esp_err_t finishChunking() { return _response->finishChunking(); }

    esp_err_t redirect(const char* url) { return _response->redirect(url); }
    esp_err_t send(int code) { return _response->send(code); }
    esp_err_t send(const char* content) { return _response->send(content); }
    esp_err_t send(const char* contentType, const char* content) { return _response->send(contentType, content); }
    esp_err_t send(int code, const char* contentType, const char* content) { return _response->send(code, contentType, content); }
    esp_err_t send(int code, const char* contentType, const uint8_t* content, size_t len) { return _response->send(code, contentType, content, len); }
    esp_err_t error(httpd_err_code_t code, const char* message) { return _response->error(code, message); }

    httpd_req_t* request() { return _response->request(); }
};

#endif // PsychicResponse_h