#ifndef PsychicMiddlewares_h
#define PsychicMiddlewares_h

#include "PsychicMiddleware.h"

#include <Stream.h>
#include <http_status.h>

// curl-like logging middleware
class LoggingMiddleware : public PsychicMiddleware
{
  public:
    void setOutput(Print& output);

    esp_err_t run(PsychicRequest* request, PsychicResponse* response, PsychicMiddlewareNext next) override;

  private:
    Print* _out;
};

class AuthenticationMiddleware : public PsychicMiddleware
{
  public:
    AuthenticationMiddleware& setUsername(const char* username);
    AuthenticationMiddleware& setPassword(const char* password);

    AuthenticationMiddleware& setRealm(const char* realm);
    AuthenticationMiddleware& setAuthMethod(HTTPAuthMethod method);
    AuthenticationMiddleware& setAuthFailureMessage(const char* message);

    const String& getUsername() const { return _username; }
    const String& getPassword() const { return _password; }

    const String& getRealm() const { return _realm; }
    HTTPAuthMethod getAuthMethod() const { return _method; }
    const String& getAuthFailureMessage() const { return _authFailMsg; }

    bool isAllowed(PsychicRequest* request) const;

    esp_err_t run(PsychicRequest* request, PsychicResponse* response, PsychicMiddlewareNext next) override;

  private:
    String _username;
    String _password;

    String _realm;
    HTTPAuthMethod _method = BASIC_AUTH;
    String _authFailMsg;
};

class CorsMiddleware : public PsychicMiddleware
{
  public:
    CorsMiddleware& setOrigin(const char* origin);
    CorsMiddleware& setMethods(const char* methods);
    CorsMiddleware& setHeaders(const char* headers);
    CorsMiddleware& setAllowCredentials(bool credentials);
    CorsMiddleware& setMaxAge(uint32_t seconds);

    const String& getOrigin() const { return _origin; }
    const String& getMethods() const { return _methods; }
    const String& getHeaders() const { return _headers; }
    bool getAllowCredentials() const { return _credentials; }
    uint32_t getMaxAge() const { return _maxAge; }

    void addCORSHeaders(PsychicResponse* response);

    esp_err_t run(PsychicRequest* request, PsychicResponse* response, PsychicMiddlewareNext next) override;

  private:
    String _origin = "*";
    String _methods = "*";
    String _headers = "*";
    bool _credentials = true;
    uint32_t _maxAge = 86400;
};

#endif
