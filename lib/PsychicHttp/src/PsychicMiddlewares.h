#ifndef PsychicMiddlewares_h
#define PsychicMiddlewares_h

#include "PsychicMiddleware.h"

#ifdef ARDUINO
  #include <Stream.h>
#endif
#include <http_status.h>

// curl-like logging middleware
#ifdef ARDUINO
class LoggingMiddleware : public PsychicMiddleware
{
  public:
    void setOutput(Print& output);

    esp_err_t run(PsychicRequest* request, PsychicResponse* response, PsychicMiddlewareNext next) override;

  private:
    Print* _out = nullptr;
};
#else
class LoggingMiddleware : public PsychicMiddleware
{
  public:
    esp_err_t run(PsychicRequest* request, PsychicResponse* response, PsychicMiddlewareNext next) override;
};
#endif

class AuthenticationMiddleware : public PsychicMiddleware
{
  public:
    AuthenticationMiddleware& setUsername(const char* username);
    AuthenticationMiddleware& setPassword(const char* password);

    AuthenticationMiddleware& setRealm(const char* realm);
    AuthenticationMiddleware& setAuthMethod(HTTPAuthMethod method);
    AuthenticationMiddleware& setAuthFailureMessage(const char* message);

#ifdef ARDUINO
    String getUsername() const { return String(_username.c_str()); }
    String getPassword() const { return String(_password.c_str()); }
    String getRealm() const { return String(_realm.c_str()); }
    String getAuthFailureMessage() const { return String(_authFailMsg.c_str()); }
#else
    const char* getUsername() const { return _username.c_str(); }
    const char* getPassword() const { return _password.c_str(); }
    const char* getRealm() const { return _realm.c_str(); }
    const char* getAuthFailureMessage() const { return _authFailMsg.c_str(); }
#endif
    HTTPAuthMethod getAuthMethod() const { return _method; }

    bool isAllowed(PsychicRequest* request) const;

    esp_err_t run(PsychicRequest* request, PsychicResponse* response, PsychicMiddlewareNext next) override;

  private:
    std::string _username;
    std::string _password;

    std::string _realm;
    HTTPAuthMethod _method = BASIC_AUTH;
    std::string _authFailMsg;
};

class CorsMiddleware : public PsychicMiddleware
{
  public:
    CorsMiddleware& setOrigin(const char* origin);
    CorsMiddleware& setMethods(const char* methods);
    CorsMiddleware& setHeaders(const char* headers);
    CorsMiddleware& setAllowCredentials(bool credentials);
    CorsMiddleware& setMaxAge(uint32_t seconds);

#ifdef ARDUINO
    String getOrigin() const { return String(_origin.c_str()); }
    String getMethods() const { return String(_methods.c_str()); }
    String getHeaders() const { return String(_headers.c_str()); }
#else
    const char* getOrigin() const { return _origin.c_str(); }
    const char* getMethods() const { return _methods.c_str(); }
    const char* getHeaders() const { return _headers.c_str(); }
#endif
    bool getAllowCredentials() const { return _credentials; }
    uint32_t getMaxAge() const { return _maxAge; }

    void addCORSHeaders(PsychicResponse* response);

    esp_err_t run(PsychicRequest* request, PsychicResponse* response, PsychicMiddlewareNext next) override;

  private:
    std::string _origin = "*";
    std::string _methods = "*";
    std::string _headers = "*";
    bool _credentials = true;
    uint32_t _maxAge = 86400;
};

#endif
