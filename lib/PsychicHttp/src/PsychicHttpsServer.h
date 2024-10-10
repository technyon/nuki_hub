#ifndef PsychicHttpsServer_h
#define PsychicHttpsServer_h

#include <sdkconfig.h>

#ifdef CONFIG_ESP_HTTPS_SERVER_ENABLE

#include "PsychicCore.h"
#include "PsychicHttpServer.h"
#include <esp_https_server.h>
#if !CONFIG_HTTPD_WS_SUPPORT
  #error PsychicHttpsServer cannot be used unless HTTPD_WS_SUPPORT is enabled in esp-http-server component configuration
#endif

#define PSY_ENABLE_SSL //you can use this define in your code to enable/disable these features

class PsychicHttpsServer : public PsychicHttpServer
{
  protected:
    bool _use_ssl = false;

  public:
    PsychicHttpsServer();
    ~PsychicHttpsServer();

    httpd_ssl_config_t ssl_config;

    using PsychicHttpServer::listen; //keep the regular version
    esp_err_t listen(uint16_t port, const char *cert, const char *private_key);

    virtual esp_err_t _startServer() override final;
    virtual void stop() override final;
};
#else
  #warning ESP-IDF https server support not enabled.
#endif // CONFIG_ESP_HTTPS_SERVER_ENABLE

#endif // PsychicHttpsServer_h