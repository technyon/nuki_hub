#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include "util/CertUtil.h"
#include <esp_task_wdt.h>
#include "FS.h"
#include "SPIFFS.h"
#include "esp_random.h"
#if defined(CONFIG_SOC_SPIRAM_SUPPORTED) && defined(CONFIG_SPIRAM)
#include "esp_psram.h"
#endif
#ifdef NUKI_HUB_HTTPS_SERVER
#include "util/SSLCert.hpp"
#endif
#ifndef CONFIG_IDF_TARGET_ESP32H2
#include <esp_wifi.h>
#include <WiFi.h>
#endif
#include <Update.h>
#include "driver/gpio.h"

extern const uint8_t x509_crt_imported_bundle_bin_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_imported_bundle_bin_end[]   asm("_binary_x509_crt_bundle_end");
extern bool timeSynced;

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE) || defined(CONFIG_ESP_WIFI_REMOTE_ENABLED)
#include "esp_hosted.h"
static esp_hosted_coprocessor_fwver_t slave_version_struct = {
  .major1 = 0,
  .minor1 = 0,
  .patch1 = 0
};
static esp_hosted_coprocessor_fwver_t host_version_struct = {
  .major1 = ESP_HOSTED_VERSION_MAJOR_1,
  .minor1 = ESP_HOSTED_VERSION_MINOR_1,
  .patch1 = ESP_HOSTED_VERSION_PATCH_1
};
#endif

#ifndef NUKI_HUB_UPDATER
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include "ArduinoJson.h"
#include <freertos/queue.h>

typedef struct
{
    int socket;
    char *buffer;
    size_t len;
} WebsocketMessage;

QueueHandle_t wsMessages;

WebCfgServer::WebCfgServer(NukiWrapper* nuki, NukiOpenerWrapper* nukiOpener, NukiNetwork* network, Gpio* gpio, Preferences* preferences, bool allowRestartToPortal, uint8_t partitionType, PsychicHttpServer* psychicServer, ImportExport* importExport)
    : _nuki(nuki),
      _nukiOpener(nukiOpener),
      _network(network),
      _gpio(gpio),
      _preferences(preferences),
      _allowRestartToPortal(allowRestartToPortal),
      _partitionType(partitionType),
      _psychicServer(psychicServer),
      _importExport(importExport)
#else
WebCfgServer::WebCfgServer(NukiNetwork* network, Preferences* preferences, bool allowRestartToPortal, uint8_t partitionType, PsychicHttpServer* psychicServer, ImportExport* importExport)
    : _network(network),
      _preferences(preferences),
      _allowRestartToPortal(allowRestartToPortal),
      _partitionType(partitionType),
      _psychicServer(psychicServer),
      _importExport(importExport)
#endif
{
    _hostname = _preferences->getString(preference_hostname, "");
    String str = _preferences->getString(preference_cred_user, "");
    str = _preferences->getString(preference_cred_user, "");
    _isSSL = (psychicServer->getPort() == 443);

    _duoEnabled = _importExport->getDuoEnabled();
    _bypassGPIO = _importExport->getBypassGPIOEnabled();
    _bypassGPIOHigh = _importExport->getBypassGPIOHigh();
    _bypassGPIOLow = _importExport->getBypassGPIOLow();

    _sessionProvider = new WebSessionProvider(_preferences, _importExport);

    if(str.length() > 0)
    {
        memset(&_credUser, 0, sizeof(_credUser));
        memset(&_credPassword, 0, sizeof(_credPassword));

        memcpy(&_credUser, str.c_str(), str.length());

        str = _preferences->getString(preference_cred_password, "");
        memcpy(&_credPassword, str.c_str(), str.length());

        if (_preferences->getInt(preference_http_auth_type, 0) == 2)
        {
            _sessionProvider->loadSessions();
        }

        if (_duoEnabled)
        {
            _sessionProvider->loadSessions(1);
        }

        if (_importExport->getTOTPEnabled())
        {
            _sessionProvider->loadSessions(2);
        }
    }
    _confirmCode = generateConfirmCode();

#ifndef NUKI_HUB_UPDATER
    _brokerConfigured = _preferences->getString(preference_mqtt_broker).length() > 0 && _preferences->getInt(preference_mqtt_broker_port) > 0;
#endif
}

void WebCfgServer::initialize()
{
    //_psychicServer->onOpen([&](PsychicClient* client) { Log->printf("[http] connection #%u connected from %s\n", client->socket(), client->localIP().toString().c_str()); });
    //_psychicServer->onClose([&](PsychicClient* client) { Log->printf("[http] connection #%u closed from %s\n", client->socket(), client->localIP().toString().c_str()); });

    _psychicServer->on("/style.css", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
    {
        return sendCss(request, resp);
    });

    if(_preferences->getBool(preference_webserial_enabled, false))
    {
#ifndef NUKI_HUB_UPDATER
        if (websocketHandler == nullptr)
        {
            websocketHandler = new PsychicWebSocketHandler;
        }

        _psychicServer->on("/webserial", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
        {
            int authReq = doAuthentication(request);

            switch (authReq)
            {
            case 0:
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
                break;
            case 1:
                return request->requestAuthentication(DIGEST_AUTH, "Nuki Hub", "You must log in.");
                break;
            case 2:
                resp->setCode(302);
                resp->addHeader("Cache-Control", "no-cache");
                return resp->redirect("/get?page=login");
                break;
            case 3:
            case 5:
            case 4:
            default:
                break;
            }

            return sendWebSerial(request, resp);
        });

        //prepare our message queue of 10 messages
        wsMessages = xQueueCreate(10, sizeof(WebsocketMessage));
        if (wsMessages == 0)
        {
            Log->printf("Failed to create queue= %p\n", wsMessages);
        }

        websocketHandler->onOpen([](PsychicWebSocketClient *client)
        {
            Log->printf("[socket] connection #%u connected from %s\n", client->socket(), client->remoteIP().toString());
            client->sendMessage("NukiHub WebSerial started");
        });
        websocketHandler->onFrame([](PsychicWebSocketRequest *request, httpd_ws_frame *frame)
        {
            if(strcmp((char *)frame->payload, "ping") == 0)
            {
                WebsocketMessage wm;
                wm.socket = request->client()->socket();
                wm.len = frame->len;
                wm.buffer = (char *)malloc(frame->len);

                if (wm.buffer == NULL)
                {
                    Log->printf("Queue message: unable to allocate %d bytes\n", frame->len);
                    return ESP_FAIL;
                }

                memcpy(wm.buffer, "pong", frame->len);

                if (xQueueSend(wsMessages, &wm, 1) != pdTRUE)
                {
                    Log->printf("[socket] queue full #%d\n", wm.socket);
                    free(wm.buffer);
                }

                if (!uxQueueSpacesAvailable(wsMessages))
                {
                    return request->reply("Queue Full");
                }
            }
            else
            {
                Log->printf("[socket] #%d sent: %s\n", request->client()->socket(), (char *)frame->payload);
            }
            return ESP_OK;
        });
        websocketHandler->onClose([](PsychicWebSocketClient *client)
        {
            Log->printf("[socket] connection #%u closed from %s\n", client->socket(), client->remoteIP().toString());
        });

        _psychicServer->on("/ws", websocketHandler);
#endif

    }

    _psychicServer->on("/favicon.ico", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
    {
        return sendFavicon(request, resp);
    });

    if(_network->isApOpen())
    {
#ifndef CONFIG_IDF_TARGET_ESP32H2
        _psychicServer->on("/get", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
        {
            String value = "";
            if(request->hasParam("page"))
            {
                const PsychicWebParameter* p = request->getParam("page");
                if(p->value() != "")
                {
                    value = p->value();
                }
            }
            if (value != "login")
            {
                int authReq = doAuthentication(request);

                switch (authReq)
                {
                case 0:
                    return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
                    break;
                case 1:
                    return request->requestAuthentication(DIGEST_AUTH, "Nuki Hub", "You must log in.");
                    break;
                case 2:
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    return resp->redirect("/get?page=login");
                    break;
                case 4:
                default:
                    break;
                }
            }
            if (value == "login")
            {
                return buildLoginHtml(request, resp);
            }
            else
            {
                Log->println("Page not found, loading index");
                resp->setCode(302);
                resp->addHeader("Cache-Control", "no-cache");
                return resp->redirect("/");
            }
        });

        _psychicServer->on("/post", HTTP_POST, [&](PsychicRequest *request, PsychicResponse* resp)
        {
            String value = "";
            if(request->hasParam("page"))
            {
                const PsychicWebParameter* p = request->getParam("page");
                if(p->value() != "")
                {
                    value = p->value();
                }
            }

            if(value != "login")
            {
                int authReq = doAuthentication(request);

                switch (authReq)
                {
                case 0:
                    return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
                    break;
                case 1:
                    return request->requestAuthentication(DIGEST_AUTH, "Nuki Hub", "You must log in.");
                    break;
                case 2:
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    return resp->redirect("/get?page=login");
                    break;
                    break;
                case 4:
                default:
                    break;
                }
            }

            if (value == "login")
            {
                bool loggedIn = processLogin(request, resp);
                if (loggedIn)
                {
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    return resp->redirect("/");
                }
                else
                {
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    return resp->redirect("/get?page=login");
                }
            }
#ifndef NUKI_HUB_UPDATER
            else if (value == "import")
            {
                String message = "";
                bool restart = processImport(request, resp, message);
                esp_err_t res = buildConfirmHtml(request, resp, message, 10, true);

                if(restart)
                {
                    waitAndProcess(true, 3000);
                    restartEsp(RestartReason::RequestedViaWebServer);
                }
                return res;
            }
#endif
            else
            {
                return buildWifiConnectHtml(request, resp);
            }
        });

        _psychicServer->on("/ssidlist", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
        {
            return buildSSIDListHtml(request, resp);
        });
        _psychicServer->on("/savewifi", HTTP_POST, [&](PsychicRequest *request, PsychicResponse* resp)
        {
            int authReq = doAuthentication(request);

            switch (authReq)
            {
            case 0:
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
                break;
            case 1:
                return request->requestAuthentication(DIGEST_AUTH, "Nuki Hub", "You must log in.");
                break;
            case 2:
                resp->setCode(302);
                resp->addHeader("Cache-Control", "no-cache");
                return resp->redirect("/get?page=login");
                break;
            case 3:
            case 5:
            case 4:
            default:
                break;
            }

            String message = "";
            bool connected = processWiFi(request, resp, message);
            esp_err_t res = buildConfirmHtml(request, resp, message, 10, true);

            if(connected)
            {
                waitAndProcess(true, 3000);
                restartEsp(RestartReason::ReconfigureWifi);
                //abort();
            }
            return res;
        });
        _psychicServer->on("/reboot", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
        {
            int authReq = doAuthentication(request);

            switch (authReq)
            {
            case 0:
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
                break;
            case 1:
                return request->requestAuthentication(DIGEST_AUTH, "Nuki Hub", "You must log in.");
                break;
            case 2:
                resp->setCode(302);
                resp->addHeader("Cache-Control", "no-cache");
                return resp->redirect("/get?page=login");
                break;
            case 3:
            case 5:
            case 4:
            default:
                break;
            }

            String value = "";
            if(request->hasParam("CONFIRMTOKEN"))
            {
                const PsychicWebParameter* p = request->getParam("CONFIRMTOKEN");
                if(p->value() != "")
                {
                    value = p->value();
                }
            }
            else
            {
                return buildConfirmHtml(request, resp, "No confirm code set.", 3, true);
            }

            if(value != _confirmCode)
            {
                resp->setCode(302);
                resp->addHeader("Cache-Control", "no-cache");
                return resp->redirect("/");
            }
            esp_err_t res = buildConfirmHtml(request, resp, "Rebooting...", 2, true);
            waitAndProcess(true, 1000);
            restartEsp(RestartReason::RequestedViaWebServer);
            return res;
        });
#endif
    }
    else
    {
        _psychicServer->on("/get", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
        {
            String value = "";
            if(request->hasParam("page"))
            {
                const PsychicWebParameter* p = request->getParam("page");
                if(p->value() != "")
                {
                    value = p->value();
                }
            }

            bool adminKeyValid = false;
            if(value == "export" && timeSynced && request->hasParam("adminkey") && request->hasParam("totpkey") && _importExport->getTOTPEnabled())
            {
                String value2 = "";
                if(request->hasParam("adminkey"))
                {
                    const PsychicWebParameter* p = request->getParam("adminkey");
                    if(p->value() != "")
                    {
                        value2 = p->value();
                    }
                }
                String value3 = "";
                if(request->hasParam("totpkey"))
                {
                    const PsychicWebParameter* p = request->getParam("totpkey");
                    if(p->value() != "")
                    {
                        value3 = p->value();
                    }
                }
                if (value2.length() > 0 && value2 == _preferences->getString(preference_admin_secret, "") && _importExport->checkTOTP(&value3))
                {
                    adminKeyValid = true;
                }
            }

            if (!adminKeyValid && value != "status" && value != "login" && value != "duocheck" && value != "bypass")
            {
                int authReq = doAuthentication(request);

                switch (authReq)
                {
                case 0:
                    return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
                    break;
                case 1:
                    return request->requestAuthentication(DIGEST_AUTH, "Nuki Hub", "You must log in.");
                    break;
                case 2:
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    return resp->redirect("/get?page=login");
                    break;
                case 3:
                    if (value != "duoauth")
                    {
                        resp->setCode(302);
                        resp->addHeader("Cache-Control", "no-cache");
                        return resp->redirect("/get?page=duoauth");
                    }
                    break;
                case 5:
                    if (value != "totp")
                    {
                        resp->setCode(302);
                        resp->addHeader("Cache-Control", "no-cache");
                        return resp->redirect("/get?page=totp");
                    }
                    break;
                case 4:
                default:
                    break;
                }
            }
            else if (value == "status")
            {
                if (doAuthentication(request) != 4)
                {
                    resp->setCode(200);
                    resp->setContentType("application/json");
                    resp->setContent("{}");
                    return resp->send();
                }
            }
            if (value == "login")
            {
                return buildLoginHtml(request, resp);
            }
            else if (value == "totp")
            {
                return buildTOTPHtml(request, resp, 0);
            }
            else if (value == "bypass")
            {
                return buildBypassHtml(request, resp);
            }
            else if (value == "newbypass" && _newBypass)
            {
                _newBypass = false;
                return buildConfirmHtml(request, resp, "Logged in using Bypass. New bypass: " + _preferences->getString(preference_bypass_secret, "") + " <br/><br/><a href=\"/\">Home page</a>", 3, false);
            }
            else if (value == "logout")
            {
                return logoutSession(request, resp);
            }
            else if (value == "duoauth")
            {
                return buildDuoHtml(request, resp, 0);
            }
            else if (value == "duocheck")
            {
                return buildDuoCheckHtml(request, resp);
            }
            else if (value == "coredump")
            {
                return buildCoredumpHtml(request, resp);
            }
            else if (value == "reboot")
            {
                String value2 = "";
                if(request->hasParam("CONFIRMTOKEN"))
                {
                    const PsychicWebParameter* p = request->getParam("CONFIRMTOKEN");
                    if(p->value() != "")
                    {
                        value2 = p->value();
                    }
                }
                else
                {
                    return buildConfirmHtml(request, resp, "No confirm code set.", 3, true);
                }

                if(value2 != _confirmCode)
                {
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    return resp->redirect("/");
                }
                esp_err_t res = buildConfirmHtml(request, resp, "Rebooting...", 2, true);
                waitAndProcess(true, 1000);
                restartEsp(RestartReason::RequestedViaWebServer);
                return res;
            }
#ifndef NUKI_HUB_UPDATER
            else if (value == "restartservices")
            {
                String value2 = "";
                if(request->hasParam("CONFIRMTOKEN"))
                {
                    const PsychicWebParameter* p = request->getParam("CONFIRMTOKEN");
                    if(p->value() != "")
                    {
                        value2 = p->value();
                    }
                }
                else
                {
                    return buildConfirmHtml(request, resp, "No confirm code set.", 3, true);
                }

                if(value2 != _confirmCode)
                {
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    return resp->redirect("/");
                }
                esp_err_t res = buildConfirmHtml(request, resp, "Restarting services...", 2, true);
                _network->setRestartServices(_restartServicesRequired == 1 ? false : true);
                _restartServicesRequired = 0;
                waitAndProcess(true, 1000);
                return res;
            }
            else if (value == "info")
            {
                return buildInfoHtml(request, resp);
            }
            else if (value == "debugon")
            {
                _preferences->putBool(preference_enable_debug_mode, true);
                return buildConfirmHtml(request, resp, "Debug On", 3, true);
            }
            else if (value == "debugoff")
            {
                _preferences->putBool(preference_enable_debug_mode, false);
                return buildConfirmHtml(request, resp, "Debug Off", 3, true);
            }
            else if (value == "export")
            {
                if(!_preferences->getBool(preference_cred_duo_approval, false) || (!_importExport->getTOTPEnabled() && !_duoEnabled))
                {
                    return sendSettings(request, resp);
                }

                if(adminKeyValid)
                {
                    return sendSettings(request, resp, true);
                }

                if(_importExport->_sessionsOpts[request->client()->localIP().toString() + "approve"])
                {
                    _importExport->_sessionsOpts[request->client()->localIP().toString() + "approve"] = false;
                    return sendSettings(request, resp);
                }
                else if(timeSynced && request->hasParam("totpkey") && _importExport->getTOTPEnabled())
                {
                    const PsychicWebParameter* pass = request->getParam("totpkey");
                    if(pass->value() != "")
                    {
                        String totpkey = pass->value();
                        if (_importExport->checkTOTP(&totpkey))
                        {
                            _importExport->_sessionsOpts[request->client()->localIP().toString() + "approve"] = false;
                            return sendSettings(request, resp);
                        }
                    }
                }

                if(useTOTP(request))
                {
                    return buildTOTPHtml(request, resp, 1);
                }
                else
                {
                    return buildDuoHtml(request, resp, 1);
                }
            }
            else if (value == "impexpcfg")
            {
                return buildImportExportHtml(request, resp);
            }
            else if (value == "status")
            {
                return buildStatusHtml(request, resp);
            }
            else if (value == "acclvl")
            {
                return buildAccLvlHtml(request, resp);
            }
            else if (value == "custntw")
            {
                return buildCustomNetworkConfigHtml(request, resp);
            }
            else if (value == "advanced")
            {
                return buildAdvancedConfigHtml(request, resp);
            }
            else if (value == "cred")
            {
                return buildCredHtml(request, resp);
            }
            else if (value == "ntwconfig")
            {
                return buildNetworkConfigHtml(request, resp);
            }
            else if (value == "mqttconfig")
            {
                return buildMqttConfigHtml(request, resp);
            }
            else if (value == "mqttcaconfig")
            {
                return buildMqttSSLConfigHtml(request, resp, 0);
            }
            else if (value == "mqttcrtconfig")
            {
                return buildMqttSSLConfigHtml(request, resp, 1);
            }
            else if (value == "mqttkeyconfig")
            {
                return buildMqttSSLConfigHtml(request, resp, 2);
            }
            else if (value == "httpcrtconfig")
            {
                return buildHttpSSLConfigHtml(request, resp, 1);
            }
            else if (value == "httpkeyconfig")
            {
                return buildHttpSSLConfigHtml(request, resp, 2);
            }
            else if (value == "selfsignhttps")
            {
                return buildHttpSSLConfigHtml(request, resp, 3);
            }
            else if (value == "nukicfg")
            {
                return buildNukiConfigHtml(request, resp);
            }
            else if (value == "gpiocfg")
            {
                return buildGpioConfigHtml(request, resp);
            }
#ifndef CONFIG_IDF_TARGET_ESP32H2
            else if (value == "wifi")
            {
                return buildConfigureWifiHtml(request, resp);
            }
            else if (value == "wifimanager")
            {
                String value2 = "";
                if(request->hasParam("CONFIRMTOKEN"))
                {
                    const PsychicWebParameter* p = request->getParam("CONFIRMTOKEN");
                    if(p->value() != "")
                    {
                        value2 = p->value();
                    }
                }
                else
                {
                    return buildConfirmHtml(request, resp, "No confirm code set.", 3, true);
                }
                if(value2 != _confirmCode)
                {
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    return resp->redirect("/");
                }
                if(!_allowRestartToPortal)
                {
                    return buildConfirmHtml(request, resp, "Can't reset WiFi when network device is Ethernet", 3, true);
                }
                esp_err_t res = buildConfirmHtml(request, resp, "Restarting. Connect to ESP access point (\"NukiHub\" with password \"NukiHubESP32\") to reconfigure Wi-Fi.", 0);
                waitAndProcess(false, 1000);
                _network->reconfigureDevice();
                return res;
            }
#endif
#endif
            else if (value == "ota")
            {
                return buildOtaHtml(request, resp);
            }
            else if (value == "otadebug")
            {
                return buildOtaHtml(request, resp);
                //return buildOtaHtml(request, resp, true);
            }
            else if (value == "reboottoota")
            {
                String value2 = "";
                if(request->hasParam("CONFIRMTOKEN"))
                {
                    const PsychicWebParameter* p = request->getParam("CONFIRMTOKEN");
                    if(p->value() != "")
                    {
                        value2 = p->value();
                    }
                }
                else
                {
                    return buildConfirmHtml(request, resp, "No confirm code set.", 3, true);
                }

                if(value2 != _confirmCode)
                {
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    return resp->redirect("/");
                }
                esp_err_t res = buildConfirmHtml(request, resp, "Rebooting to other partition...", 2, true);
                waitAndProcess(true, 1000);
                esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL));
                restartEsp(RestartReason::OTAReboot);
                return res;
            }
            else if (value == "autoupdate")
            {
#ifndef NUKI_HUB_UPDATER
                return processUpdate(request, resp);
#else
                resp->setCode(302);
                resp->addHeader("Cache-Control", "no-cache");
                return resp->redirect("/");
#endif
            }
            else
            {
                Log->println("Page not found, loading index");
                resp->setCode(302);
                resp->addHeader("Cache-Control", "no-cache");
                return resp->redirect("/");
            }
        });
        _psychicServer->on("/post", HTTP_POST, [&](PsychicRequest *request, PsychicResponse* resp)
        {
            String value = "";
            if(request->hasParam("page"))
            {
                const PsychicWebParameter* p = request->getParam("page");
                if(p->value() != "")
                {
                    value = p->value();
                }
            }

            bool adminKeyValid = false;
            if(value == "import" && timeSynced && request->hasParam("adminkey") && request->hasParam("totpkey") && _importExport->getTOTPEnabled())
            {
                String value2 = "";
                if(request->hasParam("adminkey"))
                {
                    const PsychicWebParameter* p = request->getParam("adminkey");
                    if(p->value() != "")
                    {
                        value2 = p->value();
                    }
                }
                String value3 = "";
                if(request->hasParam("totpkey"))
                {
                    const PsychicWebParameter* p = request->getParam("totpkey");
                    if(p->value() != "")
                    {
                        value3 = p->value();
                    }
                }
                if (value2.length() > 0 && value2 == _preferences->getString(preference_admin_secret, "") && _importExport->checkTOTP(&value3))
                {
                    adminKeyValid = true;
                }
            }
            if(!adminKeyValid && value != "login" && value != "totp" && value != "bypass")
            {
                int authReq = doAuthentication(request);

                switch (authReq)
                {
                case 0:
                    return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
                    break;
                case 1:
                    return request->requestAuthentication(DIGEST_AUTH, "Nuki Hub", "You must log in.");
                    break;
                case 2:
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    return resp->redirect("/get?page=login");
                    break;
                case 3:
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    return resp->redirect("/get?page=duoauth");
                    break;
                case 5:
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    return resp->redirect("/get?page=totp");
                    break;
                case 4:
                default:
                    break;
                }

                if(_preferences->getBool(preference_cred_duo_approval, false) && (_importExport->getTOTPEnabled() || _duoEnabled))
                {
                    if(!_importExport->_sessionsOpts[request->client()->localIP().toString() + "approve"])
                    {
                        bool approved = false;
                        if(timeSynced && request->hasParam("totpkey") && _importExport->getTOTPEnabled())
                        {
                            const PsychicWebParameter* pass = request->getParam("totpkey");
                            if(pass->value() != "")
                            {
                                String totpkey = pass->value();
                                if (_importExport->checkTOTP(&totpkey))
                                {
                                    _importExport->_sessionsOpts[request->client()->localIP().toString() + "approve"] = false;
                                    approved = true;
                                }
                            }
                        }
                        else if(!timeSynced && _importExport->getBypassEnabled() && isAuthenticated(request, 3))
                        {
                            _importExport->_sessionsOpts[request->client()->localIP().toString() + "approve"] = false;
                            approved = true;
                        }

                        if (!approved)
                        {
                            int posttype = 3;

                            if (value == "import")
                            {
                                posttype = 2;
                            }

                            if(useTOTP(request))
                            {

                                return buildTOTPHtml(request, resp, posttype);
                            }
                            else
                            {
                                return buildDuoHtml(request, resp, posttype);
                            }
                        }
                    }
                    else
                    {
                        _importExport->_sessionsOpts[request->client()->localIP().toString() + "approve"] = false;
                    }
                }
            }

            if (value == "login")
            {
                bool loggedIn = processLogin(request, resp);
                if (loggedIn)
                {
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    return resp->redirect("/");
                }

                resp->setCode(302);
                resp->addHeader("Cache-Control", "no-cache");
                return resp->redirect("/get?page=login");
            }
            if (value == "totp")
            {
                bool loggedIn = processTOTP(request, resp);
                if (loggedIn)
                {
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    return resp->redirect("/");
                }

                resp->setCode(302);
                resp->addHeader("Cache-Control", "no-cache");
                return resp->redirect("/get?page=totp");
            }
            if (value == "bypass")
            {
                bool loggedIn = processBypass(request, resp);
                if (loggedIn)
                {
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    _newBypass = true;
                    return resp->redirect("/get?page=newbypass");
                }

                resp->setCode(302);
                resp->addHeader("Cache-Control", "no-cache");
                return resp->redirect("/");
            }
#ifndef NUKI_HUB_UPDATER
            if (value == "savecfg")
            {
                String message = "";
                bool restart = processArgs(request, resp, message);
                if(request->hasParam("mqttssl"))
                {
                    return buildConfirmHtml(request, resp, message, 3, true, "/get?page=mqttconfig");
                }
                if(request->hasParam("httpssl"))
                {
                    return buildConfirmHtml(request, resp, message, 3, true, "/get?page=ntwconfig");
                }

                return buildConfirmHtml(request, resp, message, 3, true);
            }

            if (value == "savegpiocfg")
            {
                processGpioArgs(request, resp);
                esp_err_t res = buildConfirmHtml(request, resp, "Saving GPIO configuration. Restarting.", 3, true);
                Log->println("Restarting");
                waitAndProcess(true, 1000);
                restartEsp(RestartReason::GpioConfigurationUpdated);
                return res;
            }
            if (value == "unpairlock")
            {
                return processUnpair(request, resp, false);
            }
            if (value == "unpairopener")
            {
                return processUnpair(request, resp, true);
            }
            if (value == "factoryreset")
            {
                return processFactoryReset(request, resp);
            }
            if (value == "import")
            {
                String message = "";
                bool restart = processImport(request, resp, message);

                if(adminKeyValid)
                {
                    resp->setCode(200);
                    resp->setContentType("application/json");
                    resp->setContent("{ \"result\": \"success\"}");
                    esp_err_t res = resp->send();
                    if(restart)
                    {
                        restartEsp(RestartReason::RequestedViaWebServer);
                    }
                    return res;
                }

                return buildConfirmHtml(request, resp, message, 3, true);
            }
#endif
            else
            {
#ifndef CONFIG_IDF_TARGET_ESP32H2
                if(!_network->isApOpen())
                {
#endif
#ifndef NUKI_HUB_UPDATER
                    return buildHtml(request, resp);
#else
                    return buildOtaHtml(request, resp);
#endif
#ifndef CONFIG_IDF_TARGET_ESP32H2
                }

                return buildWifiConnectHtml(request, resp);
#endif
            }
        });

        PsychicUploadHandler *updateHandler = new PsychicUploadHandler();
        updateHandler->onUpload([&](PsychicRequest *request, const String& filename, uint64_t index, uint8_t *data, size_t len, bool last)
        {
            int authReq = doAuthentication(request);

            switch (authReq)
            {
            case 0:
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
                break;
            case 1:
                return request->requestAuthentication(DIGEST_AUTH, "Nuki Hub", "You must log in.");
                break;
            case 2:
                return ESP_FAIL;
            case 3:
                return ESP_FAIL;
            case 5:
                return ESP_FAIL;
            case 4:
            default:
                break;
            }
            return handleOtaUpload(request, filename, index, data, len, last);
        });

        updateHandler->onRequest([&](PsychicRequest* request, PsychicResponse* resp)
        {
            int authReq = doAuthentication(request);

            switch (authReq)
            {
            case 0:
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
                break;
            case 1:
                return request->requestAuthentication(DIGEST_AUTH, "Nuki Hub", "You must log in.");
                break;
            case 2:
                resp->setCode(302);
                resp->addHeader("Cache-Control", "no-cache");
                return resp->redirect("/get?page=login");
                break;
            case 3:
                resp->setCode(302);
                resp->addHeader("Cache-Control", "no-cache");
                return resp->redirect("/get?page=duoauth");
                break;
            case 5:
                resp->setCode(302);
                resp->addHeader("Cache-Control", "no-cache");
                return resp->redirect("/get?page=totp");
                break;
            case 4:
            default:
                break;
            }

            String result;
            if (!Update.hasError())
            {
                Log->print("Update code or data OK Update.errorString() ");
                Log->println(Update.errorString());
                result = "<b style='color:green'>Update OK.</b>";
                resp->setCode(200);
                resp->setContentType("text/html");
                resp->setContent(result.c_str());
                esp_err_t res = resp->send();
                restartEsp(RestartReason::OTACompleted);
                return res;
            }
            else
            {
                result = " Update.errorString() " + String(Update.errorString());
                Log->print("ERROR : error ");
                Log->println(result.c_str());
                resp->setCode(500);
                resp->setContentType("text/html");
                resp->setContent(result.c_str());
                esp_err_t res = resp->send();
                restartEsp(RestartReason::OTAAborted);
                return res;
            }
        });

        _psychicServer->on("/uploadota", HTTP_POST, updateHandler);
        //Update.onProgress(printProgress);
    }

    _psychicServer->on("/", HTTP_GET, [&](PsychicRequest *request, PsychicResponse* resp)
    {
        int authReq = doAuthentication(request);

        switch (authReq)
        {
        case 0:
            return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            break;
        case 1:
            return request->requestAuthentication(DIGEST_AUTH, "Nuki Hub", "You must log in.");
            break;
        case 2:
            resp->setCode(302);
            resp->addHeader("Cache-Control", "no-cache");
            return resp->redirect("/get?page=login");
            break;
        case 3:
            resp->setCode(302);
            resp->addHeader("Cache-Control", "no-cache");
            return resp->redirect("/get?page=duoauth");
            break;
        case 5:
            resp->setCode(302);
            resp->addHeader("Cache-Control", "no-cache");
            return resp->redirect("/get?page=totp");
            break;
        case 4:
        default:
            break;
        }

#ifndef CONFIG_IDF_TARGET_ESP32H2
        if(!_network->isApOpen())
        {
#endif
#ifndef NUKI_HUB_UPDATER
            return buildHtml(request, resp);
#else
            return buildOtaHtml(request, resp);
#endif
#ifndef CONFIG_IDF_TARGET_ESP32H2
        }
        else
        {
            return buildWifiConnectHtml(request, resp);
        }
#endif
    });

}

void WebCfgServer::printCheckBox(PsychicStreamResponse *response, const char *token, const char *description, const bool value, const char *htmlClass)
{
    response->print("<tr><td>");
    response->print(description);
    response->print("</td><td>");

    response->print("<input type=hidden name=\"");
    response->print(token);
    response->print("\" value=\"0\"");
    response->print("/>");

    response->print("<input type=checkbox name=\"");
    response->print(token);

    response->print("\" class=\"");
    response->print(htmlClass);

    response->print("\" value=\"1\"");
    response->print(value ? " checked=\"checked\"" : "");
    response->print("/></td></tr>");
}

void WebCfgServer::buildHtmlHeader(PsychicStreamResponse *response, String additionalHeader)
{
    response->print("<html><head>");
    response->print("<meta name='viewport' content='width=device-width, initial-scale=1'>");
    if(strcmp(additionalHeader.c_str(), "") != 0)
    {
        response->print(additionalHeader);
    }
    response->print("<link rel='stylesheet' href='/style.css'>");
    response->print("<title>Nuki Hub</title></head><body>");
}

void WebCfgServer::waitAndProcess(const bool blocking, const uint32_t duration)
{
    int64_t timeout = esp_timer_get_time() + (duration * 1000);
    while(esp_timer_get_time() < timeout)
    {
        if(blocking)
        {
            delay(10);
        }
        else
        {
            espDelayAck(50);
        }
    }
}

esp_err_t WebCfgServer::buildConfirmHtml(PsychicRequest *request, PsychicResponse* resp, const String &message, uint32_t redirectDelay, bool redirect, String redirectTo)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    String header;

    if(!redirect)
    {
        String delay(redirectDelay);
        header = "<meta http-equiv=\"Refresh\" content=\"" + delay + "; url=/\" />";
    }
    else
    {
        String delay(redirectDelay * 1000);
        header = "<script type=\"text/JavaScript\">function Redirect() { window.location.href = \"" + redirectTo + "\"; } setTimeout(function() { Redirect(); }, " + delay + "); </script>";
    }
    buildHtmlHeader(&response, header);
    response.print(message);
    response.print("</body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::sendCss(PsychicRequest* request, PsychicResponse* resp)
{
    // escaped by https://www.cescaper.com/
    resp->addHeader("Cache-Control", "public, max-age=3600");
    resp->setCode(200);
    resp->setContentType("text/css");
    resp->setContent(stylecss);
    return resp->send();
}

esp_err_t WebCfgServer::sendFavicon(PsychicRequest* request, PsychicResponse* resp)
{
    resp->addHeader("Cache-Control", "public, max-age=604800");
    resp->setCode(200);
    resp->setContentType("image/png");
    resp->setContent((const uint8_t *)favicon_32x32, sizeof(favicon_32x32));
    return resp->send();
}

String WebCfgServer::generateConfirmCode()
{
    int code = random(1000,9999);
    return String(code);
}

void WebCfgServer::printInputField(PsychicStreamResponse* response,
                                   const char *token,
                                   const char *description,
                                   const char *value,
                                   const size_t& maxLength,
                                   const char *args,
                                   const bool& isPassword,
                                   const bool& showLengthRestriction)
{
    char maxLengthStr[20];

    itoa(maxLength, maxLengthStr, 10);

    response->print("<tr><td>");
    response->print(description);

    if(showLengthRestriction)
    {
        response->print(" (Max. ");
        response->print(maxLength);
        response->print(" characters)");
    }

    response->print("</td><td>");
    response->print("<input type=");
    response->print(isPassword ? "\"password\"" : "\"text\"");
    if(strcmp(args, "") != 0)
    {
        response->print(" ");
        response->print(args);
    }
    if(strcmp(value, "") != 0)
    {
        response->print(" value=\"");
        response->print(value);
    }
    response->print("\" name=\"");
    response->print(token);
    response->print("\" size=\"25\" maxlength=\"");
    response->print(maxLengthStr);
    response->print("\"/>");
    response->print("</td></tr>");
}

void WebCfgServer::printInputField(PsychicStreamResponse *response,
                                   const char *token,
                                   const char *description,
                                   const int value,
                                   size_t maxLength,
                                   const char *args)
{
    char valueStr[20];
    itoa(value, valueStr, 10);
    printInputField(response, token, description, valueStr, maxLength, args);
}

#ifndef NUKI_HUB_UPDATER
esp_err_t WebCfgServer::sendWebSerial(PsychicRequest* request, PsychicResponse* resp)
{
    // escaped by https://www.cescaper.com/
    resp->addHeader("Cache-Control", "public, max-age=3600");
    resp->setCode(200);
    resp->setContentType("text/html");
    resp->setContent((const uint8_t *)WEBSERIAL_HTML, sizeof(WEBSERIAL_HTML));
    return resp->send();
}

void WebCfgServer::updateWebSerial()
{
    if (websocketHandler != nullptr)
    {
        WebsocketMessage message;
        while (xQueueReceive(wsMessages, &message, 0) == pdTRUE)
        {
            PsychicWebSocketClient *client = websocketHandler->getClient(message.socket);
            if (client == NULL)
            {
                Log->printf("[socket] client #%d bad, bailing\n", message.socket);
                return;
            }

            client->sendMessage(HTTPD_WS_TYPE_TEXT, message.buffer, message.len);
            free(message.buffer);
        }
    }
}

esp_err_t WebCfgServer::buildHtml(PsychicRequest *request, PsychicResponse* resp)
{
    String header = (String)"<script>let intervalId; window.onload = function() { updateInfo(); intervalId = setInterval(updateInfo, 3000); }; function updateInfo() { var request = new XMLHttpRequest(); request.open('GET', '/get?page=status', true); request.onload = () => { const obj = JSON.parse(request.responseText); if (obj.stop == 1) { clearInterval(intervalId); } for (var key of Object.keys(obj)) { if(key=='ota' && document.getElementById(key) !== null) { document.getElementById(key).innerText = \"<a href='/ota'>\" + obj[key] + \"</a>\"; } else if(document.getElementById(key) !== null) { document.getElementById(key).innerText = obj[key]; } } }; request.send(); }</script>";
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response, header);
    if(_rebootRequired)
    {
        response.print("<table><tbody><tr><td colspan=\"2\" style=\"border: 0; color: red; font-size: 32px; font-weight: bold; text-align: center;\">REBOOT REQUIRED TO APPLY SETTINGS</td></tr></tbody></table>");
    }
    if(_restartServicesRequired > 0)
    {
        response.print("<table><tbody><tr><td colspan=\"2\" style=\"border: 0; color: red; font-size: 32px; font-weight: bold; text-align: center;\">REBOOT REQUIRED TO APPLY SETTINGS</td></tr></tbody></table>");
    }
#ifdef DEBUG_NUKIHUB
    response.print("<table><tbody><tr><td colspan=\"2\" style=\"border: 0; color: red; font-size: 32px; font-weight: bold; text-align: center;\">RUNNING DEBUG BUILD, SWITCH TO RELEASE BUILD ASAP</td></tr></tbody></table>");
#endif
    response.print("<h3>Info</h3><br>");
    response.print("<table>");
    printParameter(&response, "Hostname", _hostname.c_str(), "", "hostname");
    printParameter(&response, "MQTT Connected", _network->mqttConnectionState() > 0 ? "Yes" : "No", "", "mqttState");
    if(_nuki != nullptr)
    {
        char lockStateArr[20];
        NukiLock::lockstateToString(_nuki->keyTurnerState().lockState, lockStateArr);
        printParameter(&response, "Nuki Lock paired", _nuki->isPaired() ? ("Yes (BLE Address " + _nuki->getBleAddress().toString() + ")").c_str() : "No", "", "lockPaired");
        printParameter(&response, "Nuki Lock state", lockStateArr, "", "lockState");

        if(_nuki->isPaired())
        {
            const String lockState = pinStateToString((NukiPinState)_preferences->getInt(preference_lock_pin_status, (int)NukiPinState::NotConfigured));
            printParameter(&response, "Nuki Lock PIN status", lockState.c_str(), "", "lockPin");

            if(_preferences->getBool(preference_official_hybrid_enabled, false))
            {
                const String offConnected = _nuki->offConnected() ? "Yes": "No";
                printParameter(&response, "Nuki Lock hybrid mode connected", offConnected.c_str(), "", "lockHybrid");
            }
        }
    }
    if(_nukiOpener != nullptr)
    {
        char openerStateArr[20];
        NukiOpener::lockstateToString(_nukiOpener->keyTurnerState().lockState, openerStateArr);
        printParameter(&response, "Nuki Opener paired", _nukiOpener->isPaired() ? ("Yes (BLE Address " + _nukiOpener->getBleAddress().toString() + ")").c_str() : "No", "", "openerPaired");

        if(_nukiOpener->keyTurnerState().nukiState == NukiOpener::State::ContinuousMode)
        {
            printParameter(&response, "Nuki Opener state", "Open (Continuous Mode)", "", "openerState");
        }
        else
        {
            printParameter(&response, "Nuki Opener state", openerStateArr, "", "openerState");
        }
        if(_nukiOpener->isPaired())
        {
            String openerState = pinStateToString((NukiPinState)_preferences->getInt(preference_opener_pin_status, (int)NukiPinState::NotConfigured));
            printParameter(&response, "Nuki Opener PIN status", openerState.c_str(), "", "openerPin");
        }
    }
    printParameter(&response, "Firmware", NUKI_HUB_VERSION, "/get?page=info", "firmware");
    if(_preferences->getBool(preference_check_updates))
    {
        printParameter(&response, "Latest Firmware", _preferences->getString(preference_latest_version).c_str(), "/get?page=ota", "ota");
    }
    response.print("</table><br>");

    response.print("<ul id=\"tblnav\">");
    buildNavigationMenuEntry(&response, "Network Configuration", "/get?page=ntwconfig");
    buildNavigationMenuEntry(&response, "MQTT Configuration", "/get?page=mqttconfig",  _brokerConfigured ? "" : "Please configure MQTT broker");
    buildNavigationMenuEntry(&response, "Nuki Configuration", "/get?page=nukicfg");
    buildNavigationMenuEntry(&response, "Access Level Configuration", "/get?page=acclvl");
    buildNavigationMenuEntry(&response, "Credentials", "/get?page=cred");
    buildNavigationMenuEntry(&response, "GPIO Configuration", "/get?page=gpiocfg");
    buildNavigationMenuEntry(&response, "Firmware update", "/get?page=ota");
    buildNavigationMenuEntry(&response, "Import/Export Configuration", "/get?page=impexpcfg");
    if(_preferences->getInt(preference_network_hardware, 0) == 11)
    {
        buildNavigationMenuEntry(&response, "Custom Ethernet Configuration", "/get?page=custntw");
    }
    if (_preferences->getBool(preference_enable_debug_mode, false))
    {
        buildNavigationMenuEntry(&response, "Advanced Configuration", "/get?page=advanced");
    }
    if(_preferences->getBool(preference_webserial_enabled, false))
    {
        buildNavigationMenuEntry(&response, "Open Webserial", "/webserial");
    }
#ifndef CONFIG_IDF_TARGET_ESP32H2
    if(_allowRestartToPortal)
    {
        buildNavigationMenuEntry(&response, "Configure Wi-Fi", "/get?page=wifi");
    }
#endif
    buildNavigationMenuEntry(&response, "Info page", "/get?page=info");
    String rebootUrl = "/get?page=reboot&CONFIRMTOKEN=" + _confirmCode;
    buildNavigationMenuEntry(&response, "Reboot Nuki Hub", rebootUrl.c_str());
/*
    String restartServicesUrl = "/get?page=restartservices&CONFIRMTOKEN=" + _confirmCode;
    buildNavigationMenuEntry(&response, "Restart Services", restartServicesUrl.c_str());
*/
    if (_preferences->getInt(preference_http_auth_type, 0) == 2)
    {
        buildNavigationMenuEntry(&response, "Logout", "/get?page=logout");
    }
    response.print("</ul></body></html>");
    return response.endSend();
}

void WebCfgServer::printTextarea(PsychicStreamResponse *response,
                                 const char *token,
                                 const char *description,
                                 const char *value,
                                 const size_t& maxLength,
                                 const bool& enabled,
                                 const bool& showLengthRestriction)
{
    char maxLengthStr[20];

    itoa(maxLength, maxLengthStr, 10);

    response->print("<tr><td>");
    response->print(description);
    if(showLengthRestriction)
    {
        response->print(" (Max. ");
        response->print(maxLength);
        response->print(" characters)");
    }
    response->print("</td><td>");
    response->print(" <textarea ");
    if(!enabled)
    {
        response->print("disabled");
    }
    response->print(" name=\"");
    response->print(token);
    response->print("\" maxlength=\"");
    response->print(maxLengthStr);
    response->print("\">");
    response->print(value);
    response->print("</textarea>");
    response->print("</td></tr>");
}

void WebCfgServer::printDropDown(PsychicStreamResponse *response, const char *token, const char *description, const String preselectedValue, const std::vector<std::pair<String, String>> options, const String className)
{
    response->print("<tr><td>");
    response->print(description);
    response->print("</td><td>");
    if(className.length() > 0)
    {
        response->print("<select class=\"" + className + "\" name=\"");
    }
    else
    {
        response->print("<select name=\"");
    }
    response->print(token);
    response->print("\">");

    for(const auto& option : options)
    {
        if(option.first == preselectedValue)
        {
            response->print("<option selected=\"selected\" value=\"");
        }
        else
        {
            response->print("<option value=\"");
        }
        response->print(option.first);
        response->print("\">");
        response->print(option.second);
        response->print("</option>");
    }

    response->print("</select>");
    response->print("</td></tr>");
}

void WebCfgServer::buildNavigationMenuEntry(PsychicStreamResponse *response, const char *title, const char *targetPath, const char* warningMessage)
{
    response->print("<a class=\"naventry\" href=\"");
    response->print(targetPath);
    response->print("\">");
    response->print("<li>");
    response->print(title);
    if(strcmp(warningMessage, "") != 0)
    {
        response->print("<span>");
        response->print(warningMessage);
        response->print("</span>");
    }
    response->print("</li></a>");
}

void WebCfgServer::printParameter(PsychicStreamResponse *response, const char *description, const char *value, const char *link, const char *id)
{
    response->print("<tr>");
    response->print("<td>");
    response->print(description);
    response->print("</td>");
    if(strcmp(id, "") == 0)
    {
        response->print("<td>");
    }
    else
    {
        response->print("<td id=\"");
        response->print(id);
        response->print("\">");
    }
    if(strcmp(link, "") == 0)
    {
        response->print(value);
    }
    else
    {
        response->print("<a href=\"");
        response->print(link);
        response->print("\"> ");
        response->print(value);
        response->print("</a>");
    }
    response->print("</td>");
    response->print("</tr>");

}

bool WebCfgServer::isParameterTrue(PsychicRequest* request, const char* key)
{
    if(request->hasParam(key))
    {
        const std::vector<PsychicWebParameter*> parameters = request->getAllParam(key);
        for (PsychicWebParameter* parameter : parameters)
        {
            if(parameter->value() == "1")
            {
                return true;
            }
        }
    }
    return false;
}

#endif

#ifndef NUKI_HUB_UPDATER
bool WebCfgServer::processArgs(PsychicRequest *request, PsychicResponse* resp, String& message)
{
    ProcessArgsState state;

    processMqttArgs(request, state, message);
    processNetworkArgs(request, state, message);
    processCredentialsArgs(request, state, message);
    processNukiArgs(request, state, message);
    processAdvancedArgs(request, state, message);

    if(state.clearSession)
    {
        _sessionProvider->clearSessions();
    }

    if(state.newMFA)
    {
        _preferences->putBool(preference_mfa_reconfigure, true);
    }

    if(state.configChanged)
    {
        message = "Configuration saved, reboot required to apply some settings";
        _rebootRequired = true;
    }
    else if (state.restartServicesReconnect)
    {
        message = "Configuration saved, reboot required to apply some settings";
        _restartServicesRequired = 2;
    }
    else if (state.restartServicesNoReconnect)
    {
        message = "Configuration saved, reboot required to apply some settings";
        _restartServicesRequired = 1;
    }
    else
    {
        message = "Configuration saved.";
    }

    _importExport->readSettings();

    _network->readSettings();

    if(_nuki != nullptr)
    {
        _nuki->readSettings();
    }

    if(_nukiOpener != nullptr)
    {
        _nukiOpener->readSettings();
    }

    return state.configChanged;
}
#endif

#ifndef NUKI_HUB_UPDATER
void WebCfgServer::applySettings(PsychicRequest *request, const Setting* settings, size_t count, ProcessArgsState& state)
{
    for(size_t i = 0; i < count; i++)
    {
        const Setting& setting = settings[i];

        if(!request->hasParam(setting.token))
        {
            continue;
        }

        String value = request->getParam(setting.token)->value();
        bool changed = false;

        switch(setting.type)
        {
        case SettingType::Bool:
        {
            bool v = isParameterTrue(request, setting.token);
            if(_preferences->getBool(setting.preference, setting.defaultValue != 0) != v)
            {
                _preferences->putBool(setting.preference, v);
                changed = true;
            }
            break;
        }
        case SettingType::Int:
        {
            int v = value.toInt();
            if((setting.minValue != 0 || setting.maxValue != 0) && (v < setting.minValue || v > setting.maxValue))
            {
                break;
            }
            if(_preferences->getInt(setting.preference, setting.defaultValue) != v)
            {
                _preferences->putInt(setting.preference, v);
                changed = true;
            }
            break;
        }
        case SettingType::String:
            if(_preferences->getString(setting.preference, "") != value)
            {
                _preferences->putString(setting.preference, value);
                changed = true;
            }
            break;
        }

        if(!changed)
        {
            continue;
        }

        Log->print("Setting changed: ");
        Log->println(setting.token);

        switch(setting.effect)
        {
        case SettingEffect::ConfigChanged:
            state.configChanged = true;
            break;
        case SettingEffect::RestartReconnect:
            state.restartServicesReconnect = true;
            break;
        case SettingEffect::RestartNoReconnect:
            state.restartServicesNoReconnect = true;
            break;
        case SettingEffect::ClearSession:
            state.restartServicesNoReconnect = true;
            state.clearSession = true;
            break;
        default:
            break;
        }
    }
}

void WebCfgServer::printSetting(PsychicStreamResponse* response, const Setting* settings, size_t count, const char* token)
{
    for(size_t i = 0; i < count; i++)
    {
        const Setting& setting = settings[i];

        if(strcmp(setting.token, token) != 0)
        {
            continue;
        }

        switch(setting.type)
        {
        case SettingType::Bool:
            printCheckBox(response, setting.token, setting.label, _preferences->getBool(setting.preference, setting.defaultValue != 0), setting.args);
            break;
        case SettingType::Int:
            printInputField(response, setting.token, setting.label, _preferences->getInt(setting.preference, setting.defaultValue), setting.maxLength, setting.args);
            break;
        case SettingType::String:
            printInputField(response, setting.token, setting.label, _preferences->getString(setting.preference).c_str(), setting.maxLength, setting.args);
            break;
        }
        return;
    }
}
#endif
