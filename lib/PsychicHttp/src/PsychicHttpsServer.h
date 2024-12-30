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

    #ifndef PSY_ENABLE_SSL
      #define PSY_ENABLE_SSL // you can use this define in your code to enable/disable these features
    #endif

class PsychicHttpsServer : public PsychicHttpServer
{
  protected:
    virtual esp_err_t _startServer() override final;
    virtual esp_err_t _stopServer() override final;

  public:
    PsychicHttpsServer(uint16_t port = 443);
    ~PsychicHttpsServer();

    httpd_ssl_config_t ssl_config;

    // using PsychicHttpServer::listen; // keep the regular version
    virtual void setPort(uint16_t port) override final;
    virtual uint16_t getPort() override final;
    // Pointer to certificate data in PEM format
    void setCertificate(const char* cert, const char* private_key) { setCertificate((const uint8_t*)cert, strlen(cert) + 1, (const uint8_t*)private_key, private_key ? strlen(private_key) + 1 : 0); }
    // Pointer to certificate data in PEM or DER format. PEM-format must have a terminating NULL-character. DER-format requires the length to be passed in certSize and keySize.
    void setCertificate(const uint8_t* cert, size_t cert_size, const uint8_t* private_key, size_t private_key_size);
};

  #endif // PsychicHttpsServer_h

#else
  #warning ESP-IDF https server support not enabled.
#endif // CONFIG_ESP_HTTPS_SERVER_ENABLE