#include "PsychicHttpsServer.h"

#ifdef CONFIG_ESP_HTTPS_SERVER_ENABLE

PsychicHttpsServer::PsychicHttpsServer(uint16_t port) : PsychicHttpServer(port)
{
  // for a SSL server
  ssl_config = HTTPD_SSL_CONFIG_DEFAULT();
  ssl_config.httpd.open_fn = PsychicHttpServer::openCallback;
  ssl_config.httpd.close_fn = PsychicHttpServer::closeCallback;
  ssl_config.httpd.uri_match_fn = httpd_uri_match_wildcard;
  ssl_config.httpd.global_user_ctx = this;
  ssl_config.httpd.global_user_ctx_free_fn = destroy;
  ssl_config.httpd.max_uri_handlers = 20;

  // each SSL connection takes about 45kb of heap
  // a barebones sketch with PsychicHttp has ~150kb of heap available
  // if we set it higher than 2 and use all the connections, we get lots of memory errors.
  // not to mention there is no heap left over for the program itself.
  ssl_config.httpd.max_open_sockets = 2;

  setPort(port);
}

PsychicHttpsServer::~PsychicHttpsServer() {}

void PsychicHttpsServer::setPort(uint16_t port)
{
  this->ssl_config.port_secure = port;
}

uint16_t PsychicHttpsServer::getPort()
{
  return this->ssl_config.port_secure;
}

void PsychicHttpsServer::setCertificate(const uint8_t* cert, size_t cert_size, const uint8_t* private_key, size_t private_key_size)
{
  if (cert) {
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 2)
    this->ssl_config.servercert = cert;
    this->ssl_config.servercert_len = cert_size;
  #else
    this->ssl_config.cacert_pem = cert;
    this->ssl_config.cacert_len = cert_size;
  #endif
  }

  if (private_key) {
    this->ssl_config.prvtkey_pem = private_key;
    this->ssl_config.prvtkey_len = private_key_size;
  }
}

esp_err_t PsychicHttpsServer::_startServer()
{
  return httpd_ssl_start(&this->server, &this->ssl_config);
}

esp_err_t PsychicHttpsServer::_stopServer()
{
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 2)
  return httpd_ssl_stop(this->server);
  #else
  httpd_ssl_stop(this->server);
  return ESP_OK;
  #endif
}

#endif // CONFIG_ESP_HTTPS_SERVER_ENABLE