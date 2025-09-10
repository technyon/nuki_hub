#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
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

    if(str.length() > 0)
    {
        memset(&_credUser, 0, sizeof(_credUser));
        memset(&_credPassword, 0, sizeof(_credPassword));

        const char *user = str.c_str();
        memcpy(&_credUser, user, str.length());

        str = _preferences->getString(preference_cred_password, "");
        const char *pass = str.c_str();
        memcpy(&_credPassword, pass, str.length());

        if (_preferences->getInt(preference_http_auth_type, 0) == 2)
        {
            loadSessions();
        }

        if (_duoEnabled)
        {
            loadSessions(1);
        }

        if (_importExport->getTOTPEnabled())
        {
            loadSessions(2);
        }
    }
    _confirmCode = generateConfirmCode();


#ifndef NUKI_HUB_UPDATER
    _brokerConfigured = _preferences->getString(preference_mqtt_broker).length() > 0 && _preferences->getInt(preference_mqtt_broker_port) > 0;
#endif
}

bool WebCfgServer::isAuthenticated(PsychicRequest *request, int type)
{
    String cookieKey = "sessionId";

    if (type == 1)
    {
        cookieKey = "duoId";
    }
    else if (type == 2)
    {
        cookieKey = "totpId";
    }
    else if (type == 3)
    {
        cookieKey = "bypassId";
    }

    if (request->hasCookie(cookieKey.c_str()))
    {
        String cookie = request->getCookie(cookieKey.c_str());

        if ((type == 0 && _httpSessions[cookie].is<JsonVariant>()) || (type == 1 && _importExport->_duoSessions[cookie].is<JsonVariant>()) || (type == 2 && _importExport->_totpSessions[cookie].is<JsonVariant>()) || (type == 3 && _importExport->_bypassSessions[cookie].is<JsonVariant>()))
        {
            struct timeval time;
            gettimeofday(&time, NULL);
            int64_t time_us = 0;

            if (timeSynced)
            {
                time_us = (int64_t)time.tv_sec * 1000000L + (int64_t)time.tv_usec;
            }

            if ((type == 0 && _httpSessions[cookie].as<signed long long>() > time_us) || (type == 1 && _importExport->_duoSessions[cookie].as<signed long long>() > time_us) || (type == 2 && _importExport->_totpSessions[cookie].as<signed long long>() > time_us) || (type == 3 && _importExport->_bypassSessions[cookie].as<signed long long>() > time_us))
            {
                return true;
            }
            else
            {
                Log->println("Cookie found, but not valid anymore");
            }
        }
    }
    return false;
}

esp_err_t WebCfgServer::logoutSession(PsychicRequest *request, PsychicResponse* resp)
{
    Log->println("Logging out");

    if (!_isSSL)
    {
        resp->setCookie("sessionId", "", 0, "HttpOnly");
    }
    else
    {
        resp->setCookie("sessionId", "", 0, "Secure; HttpOnly");
    }

    if (request->hasCookie("sessionId"))
    {
        String cookie = request->getCookie("sessionId");
        _httpSessions.remove(cookie);
        saveSessions();
    }
    else
    {
        Log->println("No session cookie found");
    }

    if (_duoEnabled)
    {
        if (!_isSSL)
        {
            resp->setCookie("duoId", "", 0, "HttpOnly");
        }
        else
        {
            resp->setCookie("duoId", "", 0, "Secure; HttpOnly");
        }

        if (request->hasCookie("duoId"))
        {
            String cookie2 = request->getCookie("duoId");
            _importExport->_duoSessions.remove(cookie2);
            saveSessions(1);
        }
        else
        {
            Log->println("No duo session cookie found");
        }
    }

    if (_importExport->getTOTPEnabled())
    {
        if (!_isSSL)
        {
            resp->setCookie("totpId", "", 0, "HttpOnly");
        }
        else
        {
            resp->setCookie("totpId", "", 0, "Secure; HttpOnly");
        }

        if (request->hasCookie("totpId"))
        {
            String cookie2 = request->getCookie("totpId");
            _importExport->_totpSessions.remove(cookie2);
            saveSessions(2);
        }
        else
        {
            Log->println("No totp session cookie found");
        }
    }

    if (_importExport->getBypassEnabled())
    {
        if (!_isSSL)
        {
            resp->setCookie("bypassId", "", 0, "HttpOnly");
        }
        else
        {
            resp->setCookie("bypassId", "", 0, "Secure; HttpOnly");
        }

        if (request->hasCookie("bypassId"))
        {
            String cookie2 = request->getCookie("bypassId");
            _importExport->_bypassSessions.remove(cookie2);
        }
    }

    return buildConfirmHtml(request, resp, "Logging out", 3, true);
}

void WebCfgServer::saveSessions(int type)
{
    if(_preferences->getBool(preference_update_time, false))
    {
        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
        }
        else
        {
            File file;

            if (type == 0)
            {
                file = SPIFFS.open("/sessions.json", "w");
                serializeJson(_httpSessions, file);
            }
            else if (type == 1)
            {
                file = SPIFFS.open("/duosessions.json", "w");
                serializeJson(_importExport->_duoSessions, file);
            }
            else if (type == 2)
            {
                file = SPIFFS.open("/totpsessions.json", "w");
                serializeJson(_importExport->_totpSessions, file);
            }
            file.close();
        }
    }
}

void WebCfgServer::loadSessions(int type)
{
    if(_preferences->getBool(preference_update_time, false))
    {
        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
        }
        else
        {
            File file;

            if (type == 0)
            {
                file = SPIFFS.open("/sessions.json", "r");

                if (!file || file.isDirectory())
                {
                    Log->println("sessions.json not found");
                }
                else
                {
                    deserializeJson(_httpSessions, file);
                }
            }
            else if (type == 1)
            {
                file = SPIFFS.open("/duosessions.json", "r");

                if (!file || file.isDirectory())
                {
                    Log->println("duosessions.json not found");
                }
                else
                {
                    deserializeJson(_importExport->_duoSessions, file);
                }
            }
            else if (type == 2)
            {
                file = SPIFFS.open("/totpsessions.json", "r");

                if (!file || file.isDirectory())
                {
                    Log->println("totpsessions.json not found");
                }
                else
                {
                    deserializeJson(_importExport->_totpSessions, file);
                }
            }
            file.close();
        }
    }
}

void WebCfgServer::clearSessions()
{
    if (!SPIFFS.begin(true))
    {
        Log->println("SPIFFS Mount Failed");
    }
    else
    {
        _httpSessions.clear();
        _importExport->_duoSessions.clear();
        _importExport->_totpSessions.clear();
        File file;
        file = SPIFFS.open("/sessions.json", "w");
        serializeJson(_httpSessions, file);
        file.close();
        file = SPIFFS.open("/duosessions.json", "w");
        serializeJson(_importExport->_duoSessions, file);
        file.close();
        file = SPIFFS.open("/totpsessions.json", "w");
        serializeJson(_importExport->_totpSessions, file);
        file.close();
    }
}

int WebCfgServer::doAuthentication(PsychicRequest *request)
{
    if (!_network->isApOpen() && _preferences->getString(preference_bypass_proxy, "") != "" && request->client()->localIP().toString() == _preferences->getString(preference_bypass_proxy, ""))
    {
        return 4;
    }
    else if(strlen(_credUser) > 0 && strlen(_credPassword) > 0)
    {
        int savedAuthType = _preferences->getInt(preference_http_auth_type, 0);
        if (savedAuthType == 2)
        {
            if (!isAuthenticated(request))
            {
                Log->println("Authentication Failed");
                return savedAuthType;
            }
        }
        else
        {
            if (!request->authenticate(_credUser, _credPassword))
            {
                Log->println("Authentication Failed");
                return savedAuthType;
            }
        }

        if (_duoEnabled || _importExport->getTOTPEnabled())
        {
            if (_bypassGPIO)
            {
                if (digitalRead(BOOT_BUTTON_GPIO) == LOW)
                {
                    Log->println("Duo bypassed because boot button pressed");
                    return 4;
                }
            }
            if (_bypassGPIOHigh > -1)
            {
                if (digitalRead(_bypassGPIOHigh) == HIGH)
                {
                    Log->println("Duo bypassed because bypass GPIO pin pulled high");
                    return 4;
                }
            }
            if (_bypassGPIOLow > -1)
            {
                if (digitalRead(_bypassGPIOLow) == LOW)
                {
                    Log->println("Duo bypassed because bypass GPIO pin pulled low");
                    return 4;
                }
            }

            if(_duoEnabled && isAuthenticated(request, 1))
            {
                _importExport->_sessionsOpts[request->client()->localIP().toString() + "totp"] = false;
                return 4;
            }
            else if(_importExport->getTOTPEnabled() && isAuthenticated(request, 2))
            {
                _importExport->_sessionsOpts[request->client()->localIP().toString() + "totp"] = true;
                return 4;
            }
            else if(!timeSynced && _importExport->getBypassEnabled() && isAuthenticated(request, 3))
            {
                _importExport->_sessionsOpts[request->client()->localIP().toString() + "totp"] = false;
                return 4;
            }

            Log->println("Authentication Failed");

            if(_importExport->getTOTPEnabled() && _importExport->_sessionsOpts[request->client()->localIP().toString() + "totp"])
            {
                return 5;
            }

            return 3;
        }
    }

    return 4;
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

                if(_importExport->_sessionsOpts[request->client()->localIP().toString() + "totp"] && _importExport->getTOTPEnabled())
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

                            if(_importExport->_sessionsOpts[request->client()->localIP().toString() + "totp"] && _importExport->getTOTPEnabled())
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
                else
                {
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    return resp->redirect("/get?page=login");
                }
            }
            else if (value == "totp")
            {
                bool loggedIn = processTOTP(request, resp);
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
                    return resp->redirect("/get?page=totp");
                }
            }
            else if (value == "bypass")
            {
                bool loggedIn = processBypass(request, resp);
                if (loggedIn)
                {
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    _newBypass = true;
                    return resp->redirect("/get?page=newbypass");
                }
                else
                {
                    resp->setCode(302);
                    resp->addHeader("Cache-Control", "no-cache");
                    return resp->redirect("/");
                }
            }
#ifndef NUKI_HUB_UPDATER
            else if (value == "savecfg")
            {
                String message = "";
                bool restart = processArgs(request, resp, message);
                if(request->hasParam("mqttssl"))
                {
                    return buildConfirmHtml(request, resp, message, 3, true, "/get?page=mqttconfig");
                }
                else if(request->hasParam("httpssl"))
                {
                    return buildConfirmHtml(request, resp, message, 3, true, "/get?page=ntwconfig");
                }
                else
                {
                    return buildConfirmHtml(request, resp, message, 3, true);
                }
            }
            else if (value == "savegpiocfg")
            {
                processGpioArgs(request, resp);
                esp_err_t res = buildConfirmHtml(request, resp, "Saving GPIO configuration. Restarting.", 3, true);
                Log->println("Restarting");
                waitAndProcess(true, 1000);
                restartEsp(RestartReason::GpioConfigurationUpdated);
                return res;
            }
            else if (value == "unpairlock")
            {
                return processUnpair(request, resp, false);
            }
            else if (value == "unpairopener")
            {
                return processUnpair(request, resp, true);
            }
            else if (value == "factoryreset")
            {
                return processFactoryReset(request, resp);
            }
            else if (value == "import")
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
                else
                {
                    return buildConfirmHtml(request, resp, message, 3, true);
                }
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
                else
                {
                    return buildWifiConnectHtml(request, resp);
                }
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

#ifndef CONFIG_IDF_TARGET_ESP32H2
esp_err_t WebCfgServer::buildSSIDListHtml(PsychicRequest *request, PsychicResponse* resp)
{
    _network->scan(true, false);
    createSsidList();

    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();

    for (int i = 0; i < _ssidList.size(); i++)
    {
        response.print("<tr class=\"trssid\" onclick=\"document.getElementById('inputssid').value = '" + _ssidList[i] + "';\"><td colspan=\"2\">" + _ssidList[i] + String((" (")) + String(_rssiList[i]) + String((" %)")) + "</td></tr>");
    }
    return response.endSend();
}

void WebCfgServer::createSsidList()
{
    int _foundNetworks = WiFi.scanComplete();
    std::vector<String> _tmpSsidList;
    std::vector<int> _tmpRssiList;

    for (int i = 0; i < _foundNetworks; i++)
    {
        int rssi = constrain((100.0 + WiFi.RSSI(i)) * 2, 0, 100);
        auto it1 = std::find(_ssidList.begin(), _ssidList.end(), WiFi.SSID(i));
        auto it2 = std::find(_tmpSsidList.begin(), _tmpSsidList.end(), WiFi.SSID(i));

        if(it1 == _ssidList.end())
        {
            _ssidList.push_back(WiFi.SSID(i));
            _rssiList.push_back(rssi);
            _tmpSsidList.push_back(WiFi.SSID(i));
            _tmpRssiList.push_back(rssi);
        }
        else if (it2 == _tmpSsidList.end())
        {
            _tmpSsidList.push_back(WiFi.SSID(i));
            _tmpRssiList.push_back(rssi);
            int index = it1 - _ssidList.begin();
            _rssiList[index] = rssi;
        }
        else
        {
            int index = it1 - _ssidList.begin();
            int index2 = it2 - _tmpSsidList.begin();
            if (_tmpRssiList[index2] < rssi)
            {
                _tmpRssiList[index2] = rssi;
                _rssiList[index] = rssi;
            }
        }
    }
}

esp_err_t WebCfgServer::buildWifiConnectHtml(PsychicRequest *request, PsychicResponse* resp)
{
    String header = "<style>.trssid:hover { cursor: pointer; color: blue; }</style><script>let intervalId; window.onload = function() { intervalId = setInterval(updateSSID, 3000); }; function updateSSID() { var request = new XMLHttpRequest(); request.open('GET', '/ssidlist', true); request.onload = () => { if (document.getElementById(\"aplist\") !== null) { document.getElementById(\"aplist\").innerHTML = request.responseText; } }; request.send(); }</script>";
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response, header);
    response.print("<h3>Available WiFi networks</h3>");
    response.print("<table id=\"aplist\">");
    createSsidList();
    for (int i = 0; i < _ssidList.size(); i++)
    {
        response.print("<tr class=\"trssid\" onclick=\"document.getElementById('inputssid').value = '" + _ssidList[i] + "';\"><td colspan=\"2\">" + _ssidList[i] + String((" (")) + String(_rssiList[i]) + String((" %)")) + "</td></tr>");
    }
    response.print("</table>");
    response.print("<form class=\"adapt\" method=\"post\" action=\"savewifi\">");
    response.print("<h3>WiFi credentials</h3>");
    response.print("<table>");
    printInputField(&response, "WIFISSID", "SSID", "", 32, "id=\"inputssid\"", false, true);
    printInputField(&response, "WIFIPASS", "Secret key", "", 63, "id=\"inputpass\"", false, true);
    printCheckBox(&response, "FINDBESTRSSI", "Find AP with best signal (disable for hidden SSID)", _preferences->getBool(preference_find_best_rssi, true), "");
    response.print("</table>");
    response.print("<h3>IP Address assignment</h3>");
    response.print("<table>");
    printCheckBox(&response, "DHCPENA", "Enable DHCP", _preferences->getBool(preference_ip_dhcp_enabled), "");
    printInputField(&response, "IPADDR", "Static IP address", _preferences->getString(preference_ip_address).c_str(), 15, "");
    printInputField(&response, "IPSUB", "Subnet", _preferences->getString(preference_ip_subnet).c_str(), 15, "");
    printInputField(&response, "IPGTW", "Default gateway", _preferences->getString(preference_ip_gateway).c_str(), 15, "");
    printInputField(&response, "DNSSRV", "DNS Server", _preferences->getString(preference_ip_dns_server).c_str(), 15, "");
    response.print("</table>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form>");
    response.print("<form action=\"/reboot\" method=\"get\"><br>");
    response.print("<input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\" /><input type=\"submit\" value=\"Reboot\" /></form>");
    response.print("</body></html>");
    return response.endSend();
}

bool WebCfgServer::processWiFi(PsychicRequest *request, PsychicResponse* resp, String& message)
{
    bool res = false;
    int params = request->params();
    String ssid;
    String pass;

    for(int index = 0; index < params; index++)
    {
        const PsychicWebParameter* p = request->getParam(index);
        String key = p->name();
        String value = p->value();


        if(index < params -1)
        {
            const PsychicWebParameter* next = request->getParam(index+1);
            if(key == next->name())
            {
                continue;
            }
        }

        if(key == "WIFISSID")
        {
            ssid = value;
        }
        else if(key == "WIFIPASS")
        {
            pass = value;
        }
        else if(key == "DHCPENA")
        {
            if(_preferences->getBool(preference_ip_dhcp_enabled, true) != (value == "1"))
            {
                _preferences->putBool(preference_ip_dhcp_enabled, (value == "1"));
            }
        }
        else if(key == "IPADDR")
        {
            if(_preferences->getString(preference_ip_address, "") != value)
            {
                _preferences->putString(preference_ip_address, value);
            }
        }
        else if(key == "IPSUB")
        {
            if(_preferences->getString(preference_ip_subnet, "") != value)
            {
                _preferences->putString(preference_ip_subnet, value);
            }
        }
        else if(key == "IPGTW")
        {
            if(_preferences->getString(preference_ip_gateway, "") != value)
            {
                _preferences->putString(preference_ip_gateway, value);
            }
        }
        else if(key == "DNSSRV")
        {
            if(_preferences->getString(preference_ip_dns_server, "") != value)
            {
                _preferences->putString(preference_ip_dns_server, value);
            }
        }
        else if(key == "FINDBESTRSSI")
        {
            if(_preferences->getBool(preference_find_best_rssi, false) != (value == "1"))
            {
                _preferences->putBool(preference_find_best_rssi, (value == "1"));
            }
        }
    }

    ssid.trim();
    pass.trim();

    if (ssid.length() > 0 && pass.length() > 0)
    {
        if (_preferences->getBool(preference_ip_dhcp_enabled, true) && _preferences->getString(preference_ip_address, "").length() <= 0)
        {
            const IPConfiguration* _ipConfiguration = new IPConfiguration(_preferences);

            if(!_ipConfiguration->dhcpEnabled())
            {
                WiFi.config(_ipConfiguration->ipAddress(), _ipConfiguration->dnsServer(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet());
            }
        }

        WiFi.begin(ssid, pass);

        int loop = 0;
        while(!_network->isConnected() && loop < 150)
        {
            if (esp_task_wdt_status(NULL) == ESP_OK)
            {
                esp_task_wdt_reset();
            }
            vTaskDelay(100 / portTICK_PERIOD_MS);
            loop++;
        }

        if (!_network->isConnected())
        {
            message = "Failed to connect to the given SSID, settings not saved<br/>";
            return res;
        }
        else
        {
            message = "Connection successful. Rebooting Nuki Hub.<br/>Please connect this device to the wireless network with the SSID "
                      + ssid + " or a wired/wireless connection that has access to the network of the selected SSID now<br/>and navigate to Nuki Hub at ";

            if (_isSSL)
            {
                if (_preferences->getString(preference_https_fqdn, "") != "")
                {
                    message += "<a href=\"https://" + _preferences->getString(preference_https_fqdn, "") + "\">https://" + _preferences->getString(preference_https_fqdn, "") + "</a>";
                }
                else
                {
                    message += "<a href=\"https://" + _network->localIP() + "\">https://" + _network->localIP() + "</a>";
                }
            }
            else
            {
                message += "<a href=\"http://" + _network->localIP() + "\">http://" + _network->localIP() + "</a>";
            }
            _preferences->putString(preference_wifi_ssid, ssid);
            _preferences->putString(preference_wifi_pass, pass);
            res = true;
        }
    }
    else
    {
        message = "No SSID or secret key entered, credentials not saved<br/>";
        return res;
    }

    return res;
}
#endif

esp_err_t WebCfgServer::buildOtaHtml(PsychicRequest *request, PsychicResponse* resp, bool debug)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();

    buildHtmlHeader(&response);

    bool errored = false;
    if(request->hasParam("errored"))
    {
        const PsychicWebParameter* p = request->getParam("errored");
        if(p->value() != "")
        {
            errored = true;
        }
    }

    if(errored)
    {
        response.print("<div>Over-the-air update errored. Please check the logs for more info</div><br/>");
    }

    if(_partitionType == 0)
    {
        response.print("<h4 class=\"warning\">You are currently running Nuki Hub with an outdated partition scheme. Because of this you cannot use OTA to update to 9.00 or higher. Please check GitHub for instructions on how to update to 9.00 and the new partition scheme</h4>");
        response.print("<button title=\"Open latest release on GitHub\" onclick=\" window.open('");
        response.print(GITHUB_LATEST_RELEASE_URL);
        response.print("', '_blank'); return false;\">Open latest release on GitHub</button>");
        return response.endSend();
    }

#ifndef NUKI_HUB_UPDATER
    bool manifestSuccess = false;
    JsonDocument doc;
    
    if(_network->isInternetConnected())
    {
        NetworkClientSecure *clientOTAUpdate = new NetworkClientSecure;
        if (clientOTAUpdate)
        {
            clientOTAUpdate->setCACertBundle(x509_crt_imported_bundle_bin_start, x509_crt_imported_bundle_bin_end - x509_crt_imported_bundle_bin_start);
            {
                HTTPClient httpsOTAClient;
                httpsOTAClient.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
                httpsOTAClient.setTimeout(2500);
                httpsOTAClient.useHTTP10(true);

                if (httpsOTAClient.begin(*clientOTAUpdate, GITHUB_OTA_MANIFEST_URL))
                {
                    int httpResponseCodeOTA = httpsOTAClient.GET();

                    if (httpResponseCodeOTA == HTTP_CODE_OK || httpResponseCodeOTA == HTTP_CODE_MOVED_PERMANENTLY)
                    {
                        DeserializationError jsonError = deserializeJson(doc, httpsOTAClient.getStream());
                        if (!jsonError)
                        {
                            manifestSuccess = true;
                        }
                    }
                    httpsOTAClient.end();
                }
            }
            delete clientOTAUpdate;
        }
    }

    response.print("<div id=\"msgdiv\" style=\"visibility:hidden\">Initiating Over-the-air update. This will take about two minutes, please be patient.<br>You will be forwarded automatically when the update is complete.</div>");
    response.print("<div id=\"autoupdform\"><h4>Update Nuki Hub</h4>");
    response.print("Click on the button to reboot and automatically update Nuki Hub and the Nuki Hub updater to the latest versions from GitHub");
    response.print("<div style=\"clear: both\"></div>");

    String release_type;

    if(debug)
    {
        release_type = "debug";
    }
    else
    {
        release_type = "release";
    }

#ifndef DEBUG_NUKIHUB
    String build_type = "release";
#else
    String build_type = "debug";
#endif
    response.print("<form onsubmit=\"if(document.getElementById('currentver').innerHTML == document.getElementById('latestver').innerHTML && '" + release_type + "' == '" + build_type + "') { alert('You are already on this version, build and build type'); return false; } else { return confirm('Do you really want to update to the latest release?'); } \" action=\"/get\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"page\" value=\"autoupdate\"><input type=\"hidden\" name=\"release\" value=\"1\" /><input type=\"hidden\" name=\"" + release_type + "\" value=\"1\" /><input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"background: green\" value=\"Update to latest release\"></form>");
    response.print("<form onsubmit=\"if(document.getElementById('currentver').innerHTML == document.getElementById('betaver').innerHTML && '" + release_type + "' == '" + build_type + "') { alert('You are already on this version, build and build type'); return false; } else { return confirm('Do you really want to update to the latest beta? This version could contain breaking bugs and necessitate downgrading to the latest release version using USB/Serial'); }\" action=\"/get\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"page\" value=\"autoupdate\"><input type=\"hidden\" name=\"beta\" value=\"1\" /><input type=\"hidden\" name=\"" + release_type + "\" value=\"1\" /><input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"color: black; background: yellow\"  value=\"Update to latest beta\"></form>");
    response.print("<form onsubmit=\"if(document.getElementById('currentver').innerHTML == document.getElementById('devver').innerHTML && '" + release_type + "' == '" + build_type + "') { alert('You are already on this version, build and build type'); return false; } else { return confirm('Do you really want to update to the latest development version? This version could contain breaking bugs and necessitate downgrading to the latest release version using USB/Serial'); }\" action=\"/get\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"page\" value=\"autoupdate\"><input type=\"hidden\" name=\"master\" value=\"1\" /><input type=\"hidden\" name=\"" + release_type + "\" value=\"1\" /><input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"background: red\"  value=\"Update to latest development version\"></form>");
#if defined(CONFIG_IDF_TARGET_ESP32S3) && defined(CONFIG_SPIRAM)
    if(esp_psram_get_size() <= 0)
    {
        response.print("<form onsubmit=\"return confirm('Do you really want to update to the latest release?');\" action=\"/get\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"page\" value=\"autoupdate\"><input type=\"hidden\" name=\"other\" value=\"1\" /><input type=\"hidden\" name=\"release\" value=\"1\" /><input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"background: blue\"  value=\"Update to other PSRAM release version\"></form>");
        response.print("<form onsubmit=\"return confirm('Do you really want to update to the latest release?');\" action=\"/get\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"page\" value=\"autoupdate\"><input type=\"hidden\" name=\"other\" value=\"2\" /><input type=\"hidden\" name=\"release\" value=\"1\" /><input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"background: blue\"  value=\"Update to No PSRAM release version\"></form>");
    }
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    response.print("<form onsubmit=\"return confirm('Do you really want to update to the latest release?');\" action=\"/get\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"page\" value=\"autoupdate\"><input type=\"hidden\" name=\"other\" value=\"1\" /><input type=\"hidden\" name=\"release\" value=\"1\" /><input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"background: blue\"  value=\"Update to Quad PSRAM release version\"></form>");
    response.print("<form onsubmit=\"return confirm('Do you really want to update to the latest release?');\" action=\"/get\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"page\" value=\"autoupdate\"><input type=\"hidden\" name=\"other\" value=\"2\" /><input type=\"hidden\" name=\"release\" value=\"1\" /><input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"background: blue\"  value=\"Update to Octal PSRAM release version\"></form>");
#elif defined(CONFIG_IDF_TARGET_ESP32) && defined(CONFIG_SPIRAM) && !defined(NUKI_TARGET_GL_S10)
    if(_preferences->getInt(preference_network_hardware) == 8)
    {
        response.print("<form onsubmit=\"return confirm('Do you really want to update to the latest release?');\" action=\"/get\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"page\" value=\"autoupdate\"><input type=\"hidden\" name=\"other\" value=\"1\" /><input type=\"hidden\" name=\"release\" value=\"1\" /><input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"background: blue\"  value=\"Update to specific GL-S10 release version\"></form>");
    }
    if(esp_psram_get_size() <= 0)
    {
        response.print("<form onsubmit=\"return confirm('Do you really want to update to the latest release?');\" action=\"/get\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"page\" value=\"autoupdate\"><input type=\"hidden\" name=\"other\" value=\"2\" /><input type=\"hidden\" name=\"release\" value=\"1\" /><input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"background: blue\"  value=\"Update to No PSRAM release version\"></form>");
    }
#elif defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_SPIRAM)
    if(_preferences->getInt(preference_network_hardware) == 8)
    {
        response.print("<form onsubmit=\"return confirm('Do you really want to update to the latest release?');\" action=\"/get\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"page\" value=\"autoupdate\"><input type=\"hidden\" name=\"other\" value=\"1\" /><input type=\"hidden\" name=\"release\" value=\"1\" /><input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"background: blue\"  value=\"Update to specific GL-S10 release version\"></form>");
    }
    response.print("<form onsubmit=\"return confirm('Do you really want to update to the latest release?');\" action=\"/get\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"page\" value=\"autoupdate\"><input type=\"hidden\" name=\"other\" value=\"2\" /><input type=\"hidden\" name=\"release\" value=\"1\" /><input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"background: blue\"  value=\"Update to Quad PSRAM release version\"></form>");
#endif
    response.print("<div style=\"clear: both\"></div><br>");

    response.print("<b>Current version: </b><span id=\"currentver\">");
    response.print(NUKI_HUB_VERSION);
    response.print(" (");
    response.print(NUKI_HUB_BUILD);
    response.print(")</span>, ");
    response.print(NUKI_HUB_DATE);
    response.print("<br>");

    if(!manifestSuccess)
    {
        response.print("<span id=\"currentver\" style=\"display: none;\">currentver</span><span id=\"latestver\" style=\"display: none;\">latestver</span><span id=\"devver\" style=\"display: none;\">devver</span><span id=\"betaver\" style=\"display: none;\">betaver</span>");
    }
    else
    {
        response.print("<b>Latest release version: </b><span id=\"latestver\">");
        response.print(doc["release"]["fullversion"].as<const char*>());
        response.print(" (");
        response.print(doc["release"]["build"].as<const char*>());
        response.print(")</span>, ");
        response.print(doc["release"]["time"].as<const char*>());
        response.print("<br>");
        response.print("<b>Latest beta version: </b><span id=\"betaver\">");
        if(doc["beta"]["fullversion"] != "No beta available")
        {
            response.print(doc["beta"]["fullversion"].as<const char*>());
            response.print(" (");
            response.print(doc["beta"]["build"].as<const char*>());
            response.print(")</span>, ");
            response.print(doc["beta"]["time"].as<const char*>());
        }
        else
        {
            response.print(doc["beta"]["fullversion"].as<const char*>());
            response.print("</span>");
        }
        response.print("<br>");
        response.print("<b>Latest development version: </b><span id=\"devver\">");
        response.print(doc["master"]["fullversion"].as<const char*>());
        response.print(" (");
        response.print(doc["master"]["build"].as<const char*>());
        response.print(")</span>, ");
        response.print(doc["master"]["time"].as<const char*>());
        response.print("<br>");

        String currentVersion = NUKI_HUB_VERSION;
        const char* latestVersion;

        if(atof(doc["release"]["version_int"]) >= NUKI_HUB_VERSION_INT)
        {
            latestVersion = doc["release"]["fullversion"];
        }
        else if(currentVersion.indexOf("beta") > 0)
        {
            latestVersion = doc["beta"]["fullversion"];
        }
        else if(currentVersion.indexOf("master") > 0)
        {
            latestVersion = doc["master"]["fullversion"];
        }
        else
        {
            latestVersion = doc["release"]["fullversion"];
        }

        if(strcmp(latestVersion, _preferences->getString(preference_latest_version).c_str()) != 0)
        {
            _preferences->putString(preference_latest_version, latestVersion);
        }
    }
#endif
    response.print("<br></div>");

    if(_partitionType == 1)
    {
        response.print("<h4><a onclick=\"hideshowmanual();\">Manually update Nuki Hub</a></h4><div id=\"manualupdate\" style=\"display: none\">");
        response.print("<div id=\"rebootform\"><h4>Reboot to Nuki Hub Updater</h4>");
        response.print("Click on the button to reboot to the Nuki Hub updater, where you can select the latest Nuki Hub binary to update");
        response.print("<form action=\"/get\" method=\"get\"><br>");
        response.print("<input type=\"hidden\" name=\"page\" value=\"reboottoota\">");
        response.print("<input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\" /><input type=\"submit\" value=\"Reboot to Nuki Hub Updater\" /></form><br><br></div>");
        response.print("<div id=\"upform\"><h4>Update Nuki Hub Updater</h4>");
        response.print("Select the latest Nuki Hub updater binary to update the Nuki Hub updater");
        response.print("<form enctype=\"multipart/form-data\" action=\"/uploadota\" method=\"post\">Choose the nuki_hub_updater.bin file to upload: <input name=\"uploadedfile\" type=\"file\" accept=\".bin\" /><br/>");
    }
    else
    {
        response.print("<div id=\"manualupdate\">");
        response.print("<div id=\"rebootform\"><h4>Reboot to Nuki Hub</h4>");
        response.print("Click on the button to reboot to Nuki Hub");
        response.print("<form action=\"/get\" method=\"get\"><br>");
        response.print("<input type=\"hidden\" name=\"page\" value=\"reboottoota\">");
        response.print("<input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\" /><input type=\"submit\" value=\"Reboot to Nuki Hub\" /></form><br><br></div>");
        response.print("<div id=\"upform\"><h4>Update Nuki Hub</h4>");
        response.print("Select the latest Nuki Hub binary to update Nuki Hub");
        response.print("<form enctype=\"multipart/form-data\" action=\"/uploadota\" method=\"post\">Choose the nuki_hub.bin file to upload: <input name=\"uploadedfile\" type=\"file\" accept=\".bin\" /><br/>");
    }
    response.print("<br><input id=\"submitbtn\" type=\"submit\" value=\"Upload File\" /></form><br><br></div>");
    response.print("<div id=\"gitdiv\">");
    response.print("<h4>GitHub</h4><br>");
    response.print("<button title=\"Open latest release on GitHub\" onclick=\" window.open('");
    response.print(GITHUB_LATEST_RELEASE_URL);
    response.print("', '_blank'); return false;\">Open latest release on GitHub</button>");
    response.print("<br><br><button title=\"Download latest binary from GitHub\" onclick=\" window.open('");
    response.print(GITHUB_LATEST_RELEASE_BINARY_URL);
    response.print("'); return false;\">Download latest binary from GitHub</button>");
    response.print("<br><br><button title=\"Download latest updater binary from GitHub\" onclick=\" window.open('");
    response.print(GITHUB_LATEST_UPDATER_BINARY_URL);
    response.print("'); return false;\">Download latest updater binary from GitHub</button></div></div>");
    response.print("<script type=\"text/javascript\">");
    response.print("window.addEventListener('load', function () {");
    response.print("	var button = document.getElementById(\"submitbtn\");");
    response.print("	button.addEventListener('click',hideshow,false);");
    response.print("	function hideshow() {");
    response.print("		document.getElementById('autoupdform').style.visibility = 'hidden';");
    response.print("		document.getElementById('rebootform').style.visibility = 'hidden';");
    response.print("		document.getElementById('upform').style.visibility = 'hidden';");
    response.print("		document.getElementById('gitdiv').style.visibility = 'hidden';");
    response.print("		document.getElementById('msgdiv').style.visibility = 'visible';");
    response.print("	}");
    response.print("});");
    response.print("function hideshowmanual() {");
    response.print("	var x = document.getElementById(\"manualupdate\");");
    response.print("	if (x.style.display === \"none\") {");
    response.print("	    x.style.display = \"block\";");
    response.print("	} else {");
    response.print("	    x.style.display = \"none\";");
    response.print("    }");
    response.print("}");
    response.print("</script>");
    response.print("</body></html>");
    return response.endSend();
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
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    }
}

esp_err_t WebCfgServer::handleOtaUpload(PsychicRequest *request, const String& filename, uint64_t index, uint8_t *data, size_t len, bool final)
{
    if(!request->url().endsWith("/uploadota"))
    {
        return(ESP_FAIL);
    }

    if(filename == "")
    {
        Log->println("Invalid file for OTA upload");
        return(ESP_FAIL);
    }

    if (!Update.hasError())
    {
        if (!index)
        {
            Update.clearError();

            Log->println("Starting manual OTA update");
            _otaContentLen = request->contentLength();

            if(_partitionType == 1 && _otaContentLen > 1600000)
            {
                Log->println("Uploaded OTA file too large, are you trying to upload a Nuki Hub binary instead of a Nuki Hub updater binary?");
                return(ESP_FAIL);
            }
            else if(_partitionType == 2 && _otaContentLen < 1600000)
            {
                Log->println("Uploaded OTA file is too small, are you trying to upload a Nuki Hub updater binary instead of a Nuki Hub binary?");
                return(ESP_FAIL);
            }

            esp_task_wdt_config_t twdt_config =
            {
                .timeout_ms = 30000,
                .idle_core_mask = 0,
                .trigger_panic = false,
            };
            esp_task_wdt_reconfigure(&twdt_config);

#ifndef NUKI_HUB_UPDATER
            _network->disableAutoRestarts();
            _network->disableMqtt();
            if(_nuki != nullptr)
            {
                _nuki->disableWatchdog();
            }
            if(_nukiOpener != nullptr)
            {
                _nukiOpener->disableWatchdog();
            }
#endif
            Log->print("handleFileUpload Name: ");
            Log->println(filename);

            if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
            {
                if (!Update.hasError())
                {
                    Update.abort();
                }
                Log->print("ERROR : update.begin error Update.errorString() ");
                Log->println(Update.errorString());
                return(ESP_FAIL);
            }
        }

        if ((len) && (!Update.hasError()))
        {
            if (Update.write(data, len) != len)
            {
                if (!Update.hasError())
                {
                    Update.abort();
                }
                Log->print("ERROR : update.write error Update.errorString() ");
                Log->println(Update.errorString());
                return(ESP_FAIL);
            }
        }

        if ((final) && (!Update.hasError()))
        {
            if (Update.end(true))
            {
                Log->print("Update Success: ");
                Log->print(index+len);
                Log->println(" written");
            }
            else
            {
                if (!Update.hasError())
                {
                    Update.abort();
                }
                Log->print("ERROR : update end error Update.errorString() ");
                Log->println(Update.errorString());
                return(ESP_FAIL);
            }
        }
        Log->print("Progress: 100%");
        Log->println();
        Log->print("handleFileUpload Total Size: ");
        Log->println(index+len);
        Log->println("Update complete");
        Log->flush();
        return(ESP_OK);
    }
    else
    {
        return(ESP_FAIL);
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

esp_err_t WebCfgServer::buildLoginHtml(PsychicRequest *request, PsychicResponse* resp)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    response.print("<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    response.print("<style>form{border:3px solid #f1f1f1; max-width: 400px;}input[type=password],input[type=text]{width:100%;padding:12px 20px;margin:8px 0;display:inline-block;border:1px solid #ccc;box-sizing:border-box}button{background-color:#04aa6d;color:#fff;padding:14px 20px;margin:8px 0;border:none;cursor:pointer;width:100%}button:hover{opacity:.8}.container{padding:16px}span.password{float:right;padding-top:16px}@media screen and (max-width:300px){span.psw{display:block;float:none}}</style>");
    response.print("</head><body><center><h2>Nuki Hub login</h2><form action=\"/post?page=login\" method=\"post\">");
    response.print("<div class=\"container\"><label for=\"username\"><b>Username</b></label><input type=\"text\" placeholder=\"Enter Username\" name=\"username\" required>");
    response.print("<label for=\"password\"><b>Password</b></label><input type=\"password\" placeholder=\"Enter Password\" name=\"password\" required>");
    if (_importExport->getTOTPEnabled() || _duoEnabled)
    {
        if (_importExport->getTOTPEnabled())
        {
            response.print("<button name=\"totp\" type=\"submit\" formaction=\"/post?page=login&totp=1\">Login with TOTP</button>");
        }
        if (_duoEnabled)
        {
            response.print("<button name=\"duo\" type=\"submit\">Login with Duo</button>");
        }
    }
    else
    {
        response.print("<button type=\"submit\">Login</button>");
    }
    response.print("<label><input type=\"checkbox\" name=\"remember\"> Remember me</label></div>");
    response.print("</form></center></body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildTOTPHtml(PsychicRequest *request, PsychicResponse* resp, int type)
{
    if (!timeSynced)
    {
        return buildConfirmHtml(request, resp, "NTP time not synced yet, TOTP not available, please wait for NTP to sync or use <a href=\"/get?page=bypass\">one-time bypass</a>", 3, true);
    }

    if((pow(_importExport->_invalidCount, 5) + _importExport->_lastCodeCheck) > espMillis())
    {
        return buildConfirmHtml(request, resp, "Too many invalid TOTP tries, please wait before retrying or use <a href=\"/get?page=bypass\">one-time bypass</a>", 3, true);
    }

    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    response.print("<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    response.print("<style>form{border:3px solid #f1f1f1; max-width: 400px;}input[type=password],input[type=text]{width:100%;padding:12px 20px;margin:8px 0;display:inline-block;border:1px solid #ccc;box-sizing:border-box}button{background-color:#04aa6d;color:#fff;padding:14px 20px;margin:8px 0;border:none;cursor:pointer;width:100%}button:hover{opacity:.8}.container{padding:16px}span.password{float:right;padding-top:16px}@media screen and (max-width:300px){span.psw{display:block;float:none}}</style>");
    response.print("</head><body><center><h2>Nuki Hub TOTP</h2>");

    String typeText = "Login";

    if(type == 0)
    {
        response.print("<form action=\"/post?page=totp\" method=\"post\">");
    }
    else
    {
        if(type == 1)
        {
            typeText = "Export";
            response.print((String)"<form action=\"" + request->uri() + "\" method=\"get\" target=\"_blank\">");
        }
        else if(type == 2)
        {
            typeText = "Import";
            response.print((String)"<form action=\"" + request->uri() + "\" method=\"post\">");
        }
        else
        {
            typeText = "Save";
            response.print((String)"<form action=\"" + request->uri() + "\" method=\"post\">");
        }
        int params = request->params();

        for(int index = 0; index < params; index++)
        {
            const PsychicWebParameter* p = request->getParam(index);
            if (p->name() != "totpkey")
            {
                response.print((String)"<input type=\"hidden\" name=\"" + p->name() + "\" value='" + p->value() + "' />");
            }
        }
    }

    response.print("<div class=\"container\">");
    response.print("<label for=\"totpkey\"><b>TOTP</b></label><input type=\"text\" placeholder=\"Enter TOTP code\" name=\"totpkey\">");
    response.print("<button type=\"submit\" ");
    if(type == 1)
    {
        response.print("onclick=\"setTimeout(function() { window.location.href = '/' }, 1000);\"");
    }
    response.print((String)">" + typeText + "</button></div>");
    response.print("</form></center></body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildBypassHtml(PsychicRequest *request, PsychicResponse* resp)
{
    if (timeSynced)
    {
        return buildConfirmHtml(request, resp, "One-time bypass is only available if NTP time is not synced</a>", 3, true);
    }

    if((pow(_importExport->_invalidCount2, 5) + _importExport->_lastCodeCheck2) > espMillis())
    {
        return buildConfirmHtml(request, resp, "Too many invalid bypass tries, please wait before retrying", 3, true);
    }

    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    response.print("<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    response.print("<style>form{border:3px solid #f1f1f1; max-width: 400px;}input[type=password],input[type=text]{width:100%;padding:12px 20px;margin:8px 0;display:inline-block;border:1px solid #ccc;box-sizing:border-box}button{background-color:#04aa6d;color:#fff;padding:14px 20px;margin:8px 0;border:none;cursor:pointer;width:100%}button:hover{opacity:.8}.container{padding:16px}span.password{float:right;padding-top:16px}@media screen and (max-width:300px){span.psw{display:block;float:none}}</style>");
    response.print("</head><body><center><h2>Nuki Hub One-time Bypass</h2>");
    response.print("<form action=\"/post?page=bypass\" method=\"post\">");
    response.print("<div class=\"container\">");
    response.print("<label for=\"bypass\"><b>Bypass code</b></label><input type=\"text\" placeholder=\"Enter bypass code\" name=\"bypass\">");
    response.print("<button type=\"submit\" ");
    response.print(">Login</button></div>");
    response.print("</form></center></body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildDuoCheckHtml(PsychicRequest *request, PsychicResponse* resp)
{
    char valueStr[2];
    itoa(_importExport->checkDuoAuth(request), valueStr, 10);
    resp->setCode(200);
    resp->setContentType("text/plain");
    resp->setContent(valueStr);
    return resp->send();
}

esp_err_t WebCfgServer::buildCoredumpHtml(PsychicRequest *request, PsychicResponse* resp)
{
    if (!SPIFFS.begin(true))
    {
        Log->println("SPIFFS Mount Failed");
    }
    else
    {
        File file = SPIFFS.open("/coredump.hex", "r");

        if (!file || file.isDirectory())
        {
            Log->println("coredump.hex not found");
        }
        else
        {
            PsychicFileResponse response(resp, file, "coredump.hex");
            String name = "coredump.txt";
            char buf[26 + name.length()];
            snprintf(buf, sizeof(buf), "attachment; filename=\"%s\"", name.c_str());
            response.addHeader("Content-Disposition", buf);
            return response.send();
        }
    }

    resp->setCode(302);
    resp->addHeader("Cache-Control", "no-cache");
    return resp->redirect("/");
}

esp_err_t WebCfgServer::buildDuoHtml(PsychicRequest *request, PsychicResponse* resp, int type)
{
    if (!timeSynced)
    {
        return buildConfirmHtml(request, resp, "NTP time not synced yet, Duo not available, please wait for NTP to sync or use <a href=\"/get?page=bypass\">one-time bypass</a>", 3, true);
    }

    String duoText;

    if (type == 0)
    {
        duoText = "login";
    }
    else if (type == 1)
    {
        duoText = "export";
    }
    else if (type == 2)
    {
        duoText = "import";
    }
    else if (type == 3)
    {
        duoText = "save";
    }

    bool duo = _importExport->startDuoAuth((char*)((String("Approve Nuki Hub ") + duoText).c_str()));

    if (!duo)
    {
        return buildConfirmHtml(request, resp, "Duo check failed", 3, true);
    }
    else
    {
        PsychicStreamResponse response(resp, "text/html");
        char buffer[33];
        int i;
        for (i = 0; i < 4; i++)
        {
            sprintf(buffer + (i * 8), "%08lx", (unsigned long int)esp_random());
        }

        if(type == 0)
        {
            int64_t durationLength = 60*60*_preferences->getInt(preference_cred_session_lifetime_duo_remember, 720);

            if (!_importExport->_sessionsOpts[request->client()->localIP().toString()])
            {
                durationLength = _preferences->getInt(preference_cred_session_lifetime_duo, 3600);
            }

            if (!_isSSL)
            {
                response.setCookie("duoId", buffer, durationLength, "HttpOnly");
            }
            else
            {
                response.setCookie("duoId", buffer, durationLength, "Secure; HttpOnly");
            }
        }

        _importExport->setDuoCheckIP(request->client()->localIP().toString());
        _importExport->setDuoCheckId(buffer);

        response.beginSend();
        response.print("<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
        response.print("<style>.container{border:3px solid #f1f1f1; max-width: 400px; padding:16px}</style>");
        response.print((String)"<script>let intervalId; let stop = 0; window.onload = function() { intervalId = setInterval(updateInfo, 2000); }; function updateInfo() { var request = new XMLHttpRequest(); request.open('GET', '/get?page=duocheck&id=" + String(buffer) + "&type=" + type + "', true); request.onload = () => { const obj = request.responseText; if ((obj == \"1\" || obj == \"0\") && stop == 0) { stop = 1; clearInterval(intervalId); if (obj == \"1\") { document.getElementById('duoresult').innerHTML = '" + duoText + " approved<br>Redirecting...'; ");
        if(type == 0)
        {
            response.print("setTimeout(function() { window.location.href = \"/\"");
        }
        else if (type == 1)
        {
            response.print((String)"window.open('" + request->uri() + "'); setTimeout(function() { window.location.href = \"/\"");
        }
        else
        {
            response.print("setTimeout(function() { document.getElementById('frmrepost').submit()");
        }
        response.print("; }, 2000); } else { document.getElementById('duoresult').innerHTML = '" + duoText + " failed<br>Refresh to retry'; } } }; request.send(); }</script>");
        response.print("</head><body><center><h2>Nuki Hub " + duoText + "</h2>");
        response.print("<div class=\"container\">Duo Push sent<br><br>");
        response.print("Please confirm " + duoText + " in the Duo app<br><br><div id=\"duoresult\"></div></div>");
        response.print("</div>");
        if (type > 1)
        {
            response.print((String)"<form id=\"frmrepost\" action=\"" + request->uri() + "\" method=\"post\">");
            int params = request->params();

            for(int index = 0; index < params; index++)
            {
                const PsychicWebParameter* p = request->getParam(index);
                if (p->name() != "submit")
                {
                    response.print((String)"<input type=\"hidden\" name=\"" + p->name() + "\" value='" + p->value() + "' />");
                }
            }
            response.print("</form>");
        }
        response.print("</center></body></html>");

        return response.endSend();
    }
}

bool WebCfgServer::processLogin(PsychicRequest *request, PsychicResponse* resp)
{
    if(request->hasParam("username") && request->hasParam("password"))
    {
        const PsychicWebParameter* user = request->getParam("username");
        const PsychicWebParameter* pass = request->getParam("password");
        if(user->value() != "" && pass->value() != "")
        {
            if (user->value() == _preferences->getString(preference_cred_user, "") && pass->value() == _preferences->getString(preference_cred_password, ""))
            {
                char buffer[33];
                int i;
                int64_t durationLength = 60*60*_preferences->getInt(preference_cred_session_lifetime_remember, 720);
                for (i = 0; i < 4; i++)
                {
                    sprintf(buffer + (i * 8), "%08lx", (unsigned long int)esp_random());
                }
                if(!request->hasParam("remember"))
                {
                    durationLength = _preferences->getInt(preference_cred_session_lifetime, 3600);
                }

                _importExport->_sessionsOpts[request->client()->localIP().toString()] = request->hasParam("remember");

                if (!_isSSL)
                {
                    resp->setCookie("sessionId", buffer, durationLength, "HttpOnly");
                }
                else
                {
                    resp->setCookie("sessionId", buffer, durationLength, "Secure; HttpOnly");
                }

                struct timeval time;
                gettimeofday(&time, NULL);
                int64_t time_us = (int64_t)time.tv_sec * 1000000L + (int64_t)time.tv_usec;
                _httpSessions[buffer] = time_us + (durationLength*1000000L);
                saveSessions();

                _importExport->_sessionsOpts[request->client()->localIP().toString() + "totp"] = request->hasParam("totp");

                return true;
            }
        }
    }
    return false;
}

bool WebCfgServer::processBypass(PsychicRequest *request, PsychicResponse* resp)
{
    if(!timeSynced && request->hasParam("bypass"))
    {
        const PsychicWebParameter* pass = request->getParam("bypass");
        if(pass->value() != "")
        {
            String bypass = pass->value();
            if (_importExport->checkBypass(bypass))
            {
                char buffer[33];
                int i;
                for (i = 0; i < 4; i++)
                {
                    sprintf(buffer + (i * 8), "%08lx", (unsigned long int)esp_random());
                }

                if (!_isSSL)
                {
                    resp->setCookie("bypassId", buffer, 3600, "HttpOnly");
                }
                else
                {
                    resp->setCookie("bypassId", buffer, 3600, "Secure; HttpOnly");
                }

                struct timeval time;
                gettimeofday(&time, NULL);
                int64_t time_us = (int64_t)time.tv_sec * 1000000L + (int64_t)time.tv_usec;
                _importExport->_bypassSessions[buffer] = time_us + ((int64_t)3600*1000000L);

                char randomstr2[33];
                randomSeed(esp_random());
                char chars[] = {'1', '2', '3','4', '5', '6','7', '8', '9', '0', 'A', 'B', 'C', 'D','E', 'F', 'G','H', 'I', 'J','K', 'L', 'M', 'N', 'O','P', 'Q','R', 'S', 'T','U', 'V', 'W','X', 'Y', 'Z'};
                for(int i = 0; i < 32; i++)
                {
                    randomstr2[i] = chars[random(36)];
                }
                randomstr2[32] = '\0';
                _preferences->putString(preference_bypass_secret, randomstr2);

                return true;
            }
        }
    }
    return false;
}

bool WebCfgServer::processTOTP(PsychicRequest *request, PsychicResponse* resp)
{
    if(timeSynced && request->hasParam("totpkey"))
    {
        const PsychicWebParameter* pass = request->getParam("totpkey");
        if(pass->value() != "")
        {
            String totpkey = pass->value();
            if (_importExport->checkTOTP(&totpkey))
            {
                char buffer[33];
                int i;
                int64_t durationLength = 60*60*_preferences->getInt(preference_cred_session_lifetime_totp_remember, 720);
                for (i = 0; i < 4; i++)
                {
                    sprintf(buffer + (i * 8), "%08lx", (unsigned long int)esp_random());
                }

                if (!_importExport->_sessionsOpts[request->client()->localIP().toString()])
                {
                    durationLength = _preferences->getInt(preference_cred_session_lifetime_totp, 3600);
                }

                if (!_isSSL)
                {
                    resp->setCookie("totpId", buffer, durationLength, "HttpOnly");
                }
                else
                {
                    resp->setCookie("totpId", buffer, durationLength, "Secure; HttpOnly");
                }

                struct timeval time;
                gettimeofday(&time, NULL);
                int64_t time_us = (int64_t)time.tv_sec * 1000000L + (int64_t)time.tv_usec;
                _importExport->_totpSessions[buffer] = time_us + (durationLength*1000000L);
                saveSessions(2);
                return true;
            }
        }
    }
    return false;
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

esp_err_t WebCfgServer::sendSettings(PsychicRequest *request, PsychicResponse* resp, bool adminKey)
{
    JsonDocument json;
    String jsonPretty;
    String name;

    if(request->hasParam("type"))
    {
        name = "nuki_hub_http_ssl.json";
        const PsychicWebParameter* p = request->getParam("type");
        if(p->value() == "https")
        {
            name = "nuki_hub_http_ssl.json";
            _importExport->exportHttpsJson(json);
        }
        else
        {
            name = "nuki_hub_mqtt_ssl.json";
            _importExport->exportMqttsJson(json);
        }
    }
    else
    {
        name = "nuki_hub_settings.json";
        bool redacted = false;
        bool pairing = false;

        if(request->hasParam("redacted"))
        {
            const PsychicWebParameter* p = request->getParam("redacted");
            if(p->value() == "1")
            {
                redacted = true;
            }
        }
        if(request->hasParam("pairing"))
        {
            const PsychicWebParameter* p = request->getParam("pairing");
            if(p->value() == "1")
            {
                pairing = true;
            }
        }
        _importExport->exportNukiHubJson(json, redacted, pairing, (_nuki != nullptr), (_nukiOpener != nullptr));
    }

    serializeJsonPretty(json, jsonPretty);
    char buf[26 + name.length()];
    snprintf(buf, sizeof(buf), "attachment; filename=\"%s\"", name.c_str());
    if(!adminKey)
    {
        resp->addHeader("Content-Disposition", buf);
    }
    resp->setCode(200);
    resp->setContentType("application/json");
    resp->setContent(jsonPretty.c_str());
    return resp->send();
}

bool WebCfgServer::processArgs(PsychicRequest *request, PsychicResponse* resp, String& message)
{
    bool configChanged = false;
    bool aclLvlChanged = false;
    bool clearMqttCredentials = false;
    bool clearCredentials = false;
    bool manPairLck = false;
    bool manPairOpn = false;
    bool networkReconfigure = false;
    bool clearSession = false;
    bool newMFA = false;
    bool restartServicesNoReconnect = false;
    bool restartServicesReconnect = false;
    unsigned char currentBleAddress[6];
    unsigned char authorizationId[4] = {0x00};
    unsigned char secretKeyK[32] = {0x00};
    unsigned char pincode[2] = {0x00};
    unsigned char ultraPincode[4] = {0x00};
    bool isUltra = false;
    unsigned char currentBleAddressOpn[6];
    unsigned char authorizationIdOpn[4] = {0x00};
    unsigned char secretKeyKOpn[32] = {0x00};

    uint32_t aclPrefs[17] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t basicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t basicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t advancedLockConfigAclPrefs[25] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t advancedOpenerConfigAclPrefs[21] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    int params = request->params();

    String pass1 = "";
    String pass2 = "";

    for(int index = 0; index < params; index++)
    {
        const PsychicWebParameter* p = request->getParam(index);
        String key = p->name();
        String value = p->value();

        if(index < params -1)
        {
            const PsychicWebParameter* next = request->getParam(index+1);
            if(key == next->name())
            {
                continue;
            }
        }

        if(key == "MQTTSERVER")
        {
            if(_preferences->getString(preference_mqtt_broker, "") != value)
            {
                _preferences->putString(preference_mqtt_broker, value);
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "MQTTPORT")
        {
            if(_preferences->getInt(preference_mqtt_broker_port, 0) !=  value.toInt())
            {
                _preferences->putInt(preference_mqtt_broker_port,  value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "MQTTUSER")
        {
            if(value == "#")
            {
                clearMqttCredentials = true;
            }
            else
            {
                if(_preferences->getString(preference_mqtt_user, "") != value)
                {
                    _preferences->putString(preference_mqtt_user, value);
                    Log->print("Setting changed: ");
                    Log->println(key);
                    restartServicesReconnect = true;
                }
            }
        }
        else if(key == "MQTTPASS")
        {
            if(value != "*")
            {
                if(_preferences->getString(preference_mqtt_password, "") != value)
                {
                    _preferences->putString(preference_mqtt_password, value);
                    Log->print("Setting changed: ");
                    Log->println(key);
                    restartServicesReconnect = true;
                }
            }
        }
        else if(key == "MQTTPATH")
        {
            if(_preferences->getString(preference_mqtt_lock_path, "") != value)
            {
                _preferences->putString(preference_mqtt_lock_path, value);
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "MQTTCA")
        {
            if (!SPIFFS.begin(true))
            {
                Log->println("SPIFFS Mount Failed");
            }
            else
            {
                if(value != "*")
                {
                    if(value != "")
                    {
                        File file = SPIFFS.open("/mqtt_ssl.ca", FILE_WRITE);
                        if (!file)
                        {
                            Log->println("Failed to open /mqtt_ssl.ca for writing");
                        }
                        else
                        {
                            if (!file.print(value))
                            {
                                Log->println("Failed to write /mqtt_ssl.ca");
                            }
                            file.close();
                        }
                    }
                    else
                    {
                        if (!SPIFFS.remove("/mqtt_ssl.ca"))
                        {
                            Log->println("Failed to delete /mqtt_ssl.ca");
                        }
                    }
                    Log->print("Setting changed: ");
                    Log->println(key);
                    restartServicesReconnect = true;
                }
            }
        }
        else if(key == "MQTTCRT")
        {
            if (!SPIFFS.begin(true))
            {
                Log->println("SPIFFS Mount Failed");
            }
            else
            {
                if(value != "*")
                {
                    if(value != "")
                    {
                        File file = SPIFFS.open("/mqtt_ssl.crt", FILE_WRITE);
                        if (!file)
                        {
                            Log->println("Failed to open /mqtt_ssl.crt for writing");
                        }
                        else
                        {
                            if (!file.print(value))
                            {
                                Log->println("Failed to write /mqtt_ssl.crt");
                            }
                            file.close();
                        }
                    }
                    else
                    {
                        if (!SPIFFS.remove("/mqtt_ssl.crt"))
                        {
                            Log->println("Failed to delete /mqtt_ssl.crt");
                        }
                    }
                    Log->print("Setting changed: ");
                    Log->println(key);
                    restartServicesReconnect = true;
                }
            }
        }
        else if(key == "MQTTKEY")
        {
            if (!SPIFFS.begin(true))
            {
                Log->println("SPIFFS Mount Failed");
            }
            else
            {
                if(value != "*")
                {
                    if(value != "")
                    {
                        File file = SPIFFS.open("/mqtt_ssl.key", FILE_WRITE);
                        if (!file)
                        {
                            Log->println("Failed to open /mqtt_ssl.key for writing");
                        }
                        else
                        {
                            if (!file.print(value))
                            {
                                Log->println("Failed to write /mqtt_ssl.key");
                            }
                            file.close();
                        }
                    }
                    else
                    {
                        if (!SPIFFS.remove("/mqtt_ssl.key"))
                        {
                            Log->println("Failed to delete /mqtt_ssl.key");
                        }
                    }
                    Log->print("Setting changed: ");
                    Log->println(key);
                    restartServicesReconnect = true;
                }
            }
        }
        else if(key == "HTTPCRT" && nuki_hub_https_server_enabled)
        {
            if (!SPIFFS.begin(true))
            {
                Log->println("SPIFFS Mount Failed");
            }
            else
            {
                if(value != "*")
                {
                    if(value != "")
                    {
                        File file = SPIFFS.open("/http_ssl.crt", FILE_WRITE);
                        if (!file)
                        {
                            Log->println("Failed to open /http_ssl.crt for writing");
                        }
                        else
                        {
                            if (!file.print(value))
                            {
                                Log->println("Failed to write /http_ssl.crt");
                            }
                            file.close();
                        }
                    }
                    else
                    {
                        if (!SPIFFS.remove("/http_ssl.crt"))
                        {
                            Log->println("Failed to delete /http_ssl.crt");
                        }
                    }
                    Log->print("Setting changed: ");
                    Log->println(key);
                    restartServicesNoReconnect = true;
                }
            }
        }
        else if(key == "HTTPKEY" && nuki_hub_https_server_enabled)
        {
            if (!SPIFFS.begin(true))
            {
                Log->println("SPIFFS Mount Failed");
            }
            else
            {
                if(value != "*")
                {
                    if(value != "")
                    {
                        File file = SPIFFS.open("/http_ssl.key", FILE_WRITE);
                        if (!file)
                        {
                            Log->println("Failed to open /http_ssl.key for writing");
                        }
                        else
                        {
                            if (!file.print(value))
                            {
                                Log->println("Failed to write /http_ssl.key");
                            }
                            file.close();
                        }
                    }
                    else
                    {
                        if (!SPIFFS.remove("/http_ssl.key"))
                        {
                            Log->println("Failed to delete /http_ssl.key");
                        }
                    }
                    Log->print("Setting changed: ");
                    Log->println(key);
                    restartServicesNoReconnect = true;
                }
            }
        }
        else if(key == "HTTPGEN" && nuki_hub_https_server_enabled)
        {
#ifdef NUKI_HUB_HTTPS_SERVER
            createSSLCertificate();
#endif
            Log->print("Setting changed: ");
            Log->println(key);
            configChanged = true;
        }
        else if(key == "UPTIME")
        {
            if(_preferences->getBool(preference_update_time, false) != (value == "1"))
            {
                _preferences->putBool(preference_update_time, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
            }
        }
        else if(key == "TIMESRV")
        {
            if(_preferences->getString(preference_time_server, "pool.ntp.org") != value)
            {
                _preferences->putString(preference_time_server, value);
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "NWHW")
        {
            if(_preferences->getInt(preference_network_hardware, 0) != value.toInt())
            {
                if(value.toInt() > 1)
                {
                    networkReconfigure = true;
                    if(value.toInt() != 11)
                    {
                        _preferences->putInt(preference_network_custom_phy, 0);
                    }
                }
                _preferences->putInt(preference_network_hardware, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "NWCUSTPHY")
        {
            if(_preferences->getInt(preference_network_custom_phy, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_phy, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "NWCUSTADDR")
        {
            if(_preferences->getInt(preference_network_custom_addr, -1) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_addr, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "NWCUSTIRQ")
        {
            if(_preferences->getInt(preference_network_custom_irq, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_irq, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "NWCUSTRST")
        {
            if(_preferences->getInt(preference_network_custom_rst, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_rst, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "NWCUSTCS")
        {
            if(_preferences->getInt(preference_network_custom_cs, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_cs, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "NWCUSTSCK")
        {
            if(_preferences->getInt(preference_network_custom_sck, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_sck, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "NWCUSTMISO")
        {
            if(_preferences->getInt(preference_network_custom_miso, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_miso, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "NWCUSTMOSI")
        {
            if(_preferences->getInt(preference_network_custom_mosi, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_mosi, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "NWCUSTPWR")
        {
            if(_preferences->getInt(preference_network_custom_pwr, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_pwr, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "NWCUSTMDIO")
        {
            if(_preferences->getInt(preference_network_custom_mdio, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_mdio, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "NWCUSTMDC")
        {
            if(_preferences->getInt(preference_network_custom_mdc, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_mdc, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "NWCUSTCLK")
        {
            if(_preferences->getInt(preference_network_custom_clk, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_clk, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "RSSI")
        {
            if(_preferences->getInt(preference_rssi_publish_interval, 60) != value.toInt())
            {
                _preferences->putInt(preference_rssi_publish_interval, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "HTTPSFQDN")
        {
            if(_preferences->getString(preference_https_fqdn, "") != value)
            {
                _preferences->putString(preference_https_fqdn, value);
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
            }
        }
        else if(key == "DUOHOST")
        {
            if(value != "*")
            {
                if(_preferences->getString(preference_cred_duo_host, "") != value)
                {
                    _preferences->putString(preference_cred_duo_host, value);
                    Log->print("Setting changed: ");
                    Log->println(key);
                    restartServicesNoReconnect = true;
                    clearSession = true;
                    newMFA = true;
                }
            }
        }
        else if(key == "DUOIKEY")
        {
            if(value != "*")
            {
                if(_preferences->getString(preference_cred_duo_ikey, "") != value)
                {
                    _preferences->putString(preference_cred_duo_ikey, value);
                    Log->print("Setting changed: ");
                    Log->println(key);
                    restartServicesNoReconnect = true;
                    clearSession = true;
                    newMFA = true;
                }
            }
        }
        else if(key == "DUOSKEY")
        {
            if(value != "*")
            {
                if(_preferences->getString(preference_cred_duo_skey, "") != value)
                {
                    _preferences->putString(preference_cred_duo_skey, value);
                    Log->print("Setting changed: ");
                    Log->println(key);
                    restartServicesNoReconnect = true;
                    clearSession = true;
                    newMFA = true;
                }
            }
        }
        else if(key == "DUOUSER")
        {
            if(value != "*")
            {
                if(_preferences->getString(preference_cred_duo_user, "") != value)
                {
                    _preferences->putString(preference_cred_duo_user, value);
                    Log->print("Setting changed: ");
                    Log->println(key);
                    restartServicesNoReconnect = true;
                    clearSession = true;
                    newMFA = true;
                }
            }
        }
        else if(key == "DUOENA")
        {
            if(_preferences->getBool(preference_cred_duo_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_cred_duo_enabled, (value == "1"));
                if (value == "1")
                {
                    _preferences->putBool(preference_update_time, true);
                }
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
                clearSession = true;
                newMFA = true;
            }
        }
        else if(key == "DUOBYPASS")
        {
            if(_preferences->getBool(preference_cred_bypass_boot_btn_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_cred_bypass_boot_btn_enabled, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
            }
        }
        else if(key == "DUOBYPASSHIGH")
        {
            if(_preferences->getInt(preference_cred_bypass_gpio_high, -1) != value.toInt())
            {
                _preferences->putInt(preference_cred_bypass_gpio_high, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
            }
        }
        else if(key == "DUOBYPASSLOW")
        {
            if(_preferences->getInt(preference_cred_bypass_gpio_low, -1) != value.toInt())
            {
                _preferences->putInt(preference_cred_bypass_gpio_low, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
            }
        }
        else if(key == "DUOAPPROVAL")
        {
            if(_preferences->getBool(preference_cred_duo_approval, false) != (value == "1"))
            {
                _preferences->putBool(preference_cred_duo_approval, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
            }
        }
        else if(key == "CREDLFTM")
        {
            if(_preferences->getInt(preference_cred_session_lifetime, 3600) != value.toInt())
            {
                _preferences->putInt(preference_cred_session_lifetime, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
                clearSession = true;
            }
        }
        else if(key == "CREDLFTMRMBR")
        {
            if(_preferences->getInt(preference_cred_session_lifetime_remember, 720) != value.toInt())
            {
                _preferences->putInt(preference_cred_session_lifetime_remember, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
                clearSession = true;
            }
        }
        else if(key == "CREDDUOLFTM")
        {
            if(_preferences->getInt(preference_cred_session_lifetime_duo, 3600) != value.toInt())
            {
                _preferences->putInt(preference_cred_session_lifetime_duo, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
                clearSession = true;
            }
        }
        else if(key == "CREDDUOLFTMRMBR")
        {
            if(_preferences->getInt(preference_cred_session_lifetime_duo_remember, 720) != value.toInt())
            {
                _preferences->putInt(preference_cred_session_lifetime_duo_remember, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
                clearSession = true;
            }
        }
        else if(key == "CREDTOTPLFTM")
        {
            if(_preferences->getInt(preference_cred_session_lifetime_totp, 3600) != value.toInt())
            {
                _preferences->putInt(preference_cred_session_lifetime_totp, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
                clearSession = true;
            }
        }
        else if(key == "CREDTOTPLFTMRMBR")
        {
            if(_preferences->getInt(preference_cred_session_lifetime_totp_remember, 720) != value.toInt())
            {
                _preferences->putInt(preference_cred_session_lifetime_totp_remember, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
                clearSession = true;
            }
        }
        else if(key == "HADEVDISC")
        {
            if(_preferences->getBool(preference_hass_device_discovery, false) != (value == "1"))
            {
                _network->disableHASS();
                _preferences->putBool(preference_hass_device_discovery, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "ENHADISC")
        {
            if(_preferences->getBool(preference_mqtt_hass_enabled, false) != (value == "1"))
            {
                _network->disableHASS();
                _preferences->putBool(preference_mqtt_hass_enabled, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "HASSDISCOVERY")
        {
            if(_preferences->getString(preference_mqtt_hass_discovery, "") != value)
            {
                _network->disableHASS();
                _preferences->putString(preference_mqtt_hass_discovery, value);
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "OPENERCONT")
        {
            if(_preferences->getBool(preference_opener_continuous_mode, false) != (value == "1"))
            {
                _preferences->putBool(preference_opener_continuous_mode, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "HASSCUURL")
        {
            if(_preferences->getString(preference_mqtt_hass_cu_url, "") != value)
            {
                _preferences->putString(preference_mqtt_hass_cu_url, value);
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "HOSTNAME")
        {
            if(_preferences->getString(preference_hostname, "") != value)
            {
                _preferences->putString(preference_hostname, value);
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "NETTIMEOUT")
        {
            if(_preferences->getInt(preference_network_timeout, 60) != value.toInt())
            {
                _preferences->putInt(preference_network_timeout, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "FINDBESTRSSI")
        {
            if(_preferences->getBool(preference_find_best_rssi, false) != (value == "1"))
            {
                _preferences->putBool(preference_find_best_rssi, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "RSTDISC")
        {
            if(_preferences->getBool(preference_restart_on_disconnect, false) != (value == "1"))
            {
                _preferences->putBool(preference_restart_on_disconnect, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "MQTTLOG")
        {
            if(_preferences->getBool(preference_mqtt_log_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_mqtt_log_enabled, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "MQTTSENA")
        {
            if(_preferences->getBool(preference_mqtt_ssl_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_mqtt_ssl_enabled, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "WEBLOG")
        {
            if(_preferences->getBool(preference_webserial_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_webserial_enabled, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
            }
        }
        else if(key == "CHECKUPDATE")
        {
            if(_preferences->getBool(preference_check_updates, false) != (value == "1"))
            {
                _preferences->putBool(preference_check_updates, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "UPDATEMQTT")
        {
            if(_preferences->getBool(preference_update_from_mqtt, false) != (value == "1"))
            {
                _preferences->putBool(preference_update_from_mqtt, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "OFFHYBRID")
        {
            if(_preferences->getBool(preference_official_hybrid_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_official_hybrid_enabled, (value == "1"));
                if((value == "1"))
                {
                    _preferences->putBool(preference_register_as_app, true);
                }
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "HYBRIDACT")
        {
            if(_preferences->getBool(preference_official_hybrid_actions, false) != (value == "1"))
            {
                _preferences->putBool(preference_official_hybrid_actions, (value == "1"));
                if(value == "1")
                {
                    _preferences->putBool(preference_register_as_app, true);
                }
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "HYBRIDTIMER")
        {
            if(_preferences->getInt(preference_query_interval_hybrid_lockstate, 600) != value.toInt())
            {
                _preferences->putInt(preference_query_interval_hybrid_lockstate, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "HYBRIDRETRY")
        {
            if(_preferences->getBool(preference_official_hybrid_retry, false) != (value == "1"))
            {
                _preferences->putBool(preference_official_hybrid_retry, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "HYBRIDREBOOT")
        {
            if(_preferences->getBool(preference_hybrid_reboot_on_disconnect, false) != (value == "1"))
            {
                _preferences->putBool(preference_hybrid_reboot_on_disconnect, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "DISNONJSON")
        {
            if(_preferences->getBool(preference_disable_non_json, false) != (value == "1"))
            {
                _preferences->putBool(preference_disable_non_json, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "DHCPENA")
        {
            if(_preferences->getBool(preference_ip_dhcp_enabled, true) != (value == "1"))
            {
                _preferences->putBool(preference_ip_dhcp_enabled, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "IPADDR")
        {
            if(_preferences->getString(preference_ip_address, "") != value)
            {
                _preferences->putString(preference_ip_address, value);
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "IPSUB")
        {
            if(_preferences->getString(preference_ip_subnet, "") != value)
            {
                _preferences->putString(preference_ip_subnet, value);
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "IPGTW")
        {
            if(_preferences->getString(preference_ip_gateway, "") != value)
            {
                _preferences->putString(preference_ip_gateway, value);
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "DNSSRV")
        {
            if(_preferences->getString(preference_ip_dns_server, "") != value)
            {
                _preferences->putString(preference_ip_dns_server, value);
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "LSTINT")
        {
            if(_preferences->getInt(preference_query_interval_lockstate, 1800) != value.toInt())
            {
                _preferences->putInt(preference_query_interval_lockstate, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "CFGINT")
        {
            if(_preferences->getInt(preference_query_interval_configuration, 3600) != value.toInt())
            {
                _preferences->putInt(preference_query_interval_configuration, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "BATINT")
        {
            if(_preferences->getInt(preference_query_interval_battery, 1800) != value.toInt())
            {
                _preferences->putInt(preference_query_interval_battery, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "KPINT")
        {
            if(_preferences->getInt(preference_query_interval_keypad, 1800) != value.toInt())
            {
                _preferences->putInt(preference_query_interval_keypad, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "NRTRY")
        {
            if(_preferences->getInt(preference_command_nr_of_retries, 3) != value.toInt())
            {
                _preferences->putInt(preference_command_nr_of_retries, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "TRYDLY")
        {
            if(_preferences->getInt(preference_command_retry_delay, 100) != value.toInt())
            {
                _preferences->putInt(preference_command_retry_delay, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "TXPWR")
        {
#if defined(CONFIG_IDF_TARGET_ESP32)
            if(value.toInt() >= -12 && value.toInt() <= 9)
#else
            if(value.toInt() >= -12 && value.toInt() <= 20)
#endif
            {
                if(_preferences->getInt(preference_ble_tx_power, 9) != value.toInt())
                {
                    _preferences->putInt(preference_ble_tx_power, value.toInt());
                    Log->print("Setting changed: ");
                    Log->println(key);
                }
            }
        }
        else if(key == "RSBC")
        {
            if(_preferences->getInt(preference_restart_ble_beacon_lost, 60) != value.toInt())
            {
                _preferences->putInt(preference_restart_ble_beacon_lost, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "TSKNTWK")
        {
            if(value.toInt() > 12287 && value.toInt() < 65537)
            {
                if(_preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE) != value.toInt())
                {
                    _preferences->putInt(preference_task_size_network, value.toInt());
                    Log->print("Setting changed: ");
                    Log->println(key);
                    configChanged = true;
                }
            }
        }
        else if(key == "TSKNUKI")
        {
            if(value.toInt() > 8191 && value.toInt() < 65537)
            {
                if(_preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE) != value.toInt())
                {
                    _preferences->putInt(preference_task_size_nuki, value.toInt());
                    Log->print("Setting changed: ");
                    Log->println(key);
                    configChanged = true;
                }
            }
        }
        else if(key == "ALMAX")
        {
            if(value.toInt() > 0 && value.toInt() < 101)
            {
                if(_preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG) != value.toInt())
                {
                    _preferences->putInt(preference_authlog_max_entries, value.toInt());
                    Log->print("Setting changed: ");
                    Log->println(key);
                }
            }
        }
        else if(key == "KPMAX")
        {
            if(value.toInt() > 0 && value.toInt() < 201)
            {
                if(_preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD) != value.toInt())
                {
                    _preferences->putInt(preference_keypad_max_entries, value.toInt());
                    Log->print("Setting changed: ");
                    Log->println(key);
                }
            }
        }
        else if(key == "TCMAX")
        {
            if(value.toInt() > 0 && value.toInt() < 101)
            {
                if(_preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL) != value.toInt())
                {
                    _preferences->putInt(preference_timecontrol_max_entries, value.toInt());
                    Log->print("Setting changed: ");
                    Log->println(key);
                }
            }
        }
        else if(key == "AUTHMAX")
        {
            if(value.toInt() > 0 && value.toInt() < 101)
            {
                if(_preferences->getInt(preference_auth_max_entries, MAX_AUTH) != value.toInt())
                {
                    _preferences->putInt(preference_auth_max_entries, value.toInt());
                    Log->print("Setting changed: ");
                    Log->println(key);
                }
            }
        }
        else if(key == "BUFFSIZE")
        {
            if(value.toInt() > 4095 && value.toInt() < 65537)
            {
                if(_preferences->getInt(preference_buffer_size, CHAR_BUFFER_SIZE) != value.toInt())
                {
                    _preferences->putInt(preference_buffer_size, value.toInt());
                    Log->print("Setting changed: ");
                    Log->println(key);
                    configChanged = true;
                }
            }
        }
        else if(key == "BTLPRST")
        {
            if(_preferences->getBool(preference_enable_bootloop_reset, false) != (value == "1"))
            {
                _preferences->putBool(preference_enable_bootloop_reset, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "DISNTWNOCON")
        {
            if(_preferences->getBool(preference_disable_network_not_connected, false) != (value == "1"))
            {
                _preferences->putBool(preference_disable_network_not_connected, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "OTAUPD")
        {
            if(_preferences->getString(preference_ota_updater_url, "") != value)
            {
                _preferences->putString(preference_ota_updater_url, value);
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "OTAMAIN")
        {
            if(_preferences->getString(preference_ota_main_url, "") != value)
            {
                _preferences->putString(preference_ota_main_url, value);
                Log->print("Setting changed: ");
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "SHOWSECRETS")
        {
            if(_preferences->getBool(preference_show_secrets, false) != (value == "1"))
            {
                _preferences->putBool(preference_show_secrets, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "DBGCONN")
        {
            if(_preferences->getBool(preference_debug_connect, false) != (value == "1"))
            {
                _preferences->putBool(preference_debug_connect, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
            }
        }
        else if(key == "DBGCOMMU")
        {
            if(_preferences->getBool(preference_debug_communication, false) != (value == "1"))
            {
                _preferences->putBool(preference_debug_communication, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
            }
        }
        else if(key == "DBGHEAP")
        {
            if(_preferences->getBool(preference_publish_debug_info, false) != (value == "1"))
            {
                _preferences->putBool(preference_publish_debug_info, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
            }
        }
        else if(key == "DBGREAD")
        {
            if(_preferences->getBool(preference_debug_readable_data, false) != (value == "1"))
            {
                _preferences->putBool(preference_debug_readable_data, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
            }
        }
        else if(key == "DBGHEX")
        {
            if(_preferences->getBool(preference_debug_hex_data, false) != (value == "1"))
            {
                _preferences->putBool(preference_debug_hex_data, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
            }
        }
        else if(key == "DBGCOMM")
        {
            if(_preferences->getBool(preference_debug_command, false) != (value == "1"))
            {
                _preferences->putBool(preference_debug_command, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
            }
        }
        else if(key == "LCKFORCEID")
        {
            if(_preferences->getBool(preference_lock_force_id, false) != (value == "1"))
            {
                _preferences->putBool(preference_lock_force_id, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "LCKFORCEKP")
        {
            if(_preferences->getBool(preference_lock_force_keypad, false) != (value == "1"))
            {
                _preferences->putBool(preference_lock_force_keypad, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "LCKFORCEDS")
        {
            if(_preferences->getBool(preference_lock_force_doorsensor, false) != (value == "1"))
            {
                _preferences->putBool(preference_lock_force_doorsensor, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "OPFORCEID")
        {
            if(_preferences->getBool(preference_opener_force_id, false) != (value == "1"))
            {
                _preferences->putBool(preference_opener_force_id, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "OPFORCEKP")
        {
            if(_preferences->getBool(preference_opener_force_keypad, false) != (value == "1"))
            {
                _preferences->putBool(preference_opener_force_keypad, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "ACLLVLCHANGED")
        {
            aclLvlChanged = true;
        }
        else if(key == "CONFPUB")
        {
            if(_preferences->getBool(preference_conf_info_enabled, true) != (value == "1"))
            {
                _preferences->putBool(preference_conf_info_enabled, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "CONFNHPUB")
        {
            if(_preferences->getBool(preference_publish_config, false) != (value == "1"))
            {
                if(_preferences->getBool(preference_config_from_mqtt, false) && _preferences->getInt(preference_buffer_size, CHAR_BUFFER_SIZE) < 8192)
                {
                    _preferences->putInt(preference_buffer_size, 8192);
                }
                _preferences->putBool(preference_publish_config, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "CONFNHCTRL")
        {
            if(_preferences->getBool(preference_config_from_mqtt, false) != (value == "1"))
            {
                if(_preferences->getBool(preference_config_from_mqtt, false) && _preferences->getInt(preference_buffer_size, CHAR_BUFFER_SIZE) < 8192)
                {
                    _preferences->putInt(preference_buffer_size, 8192);
                }
                _preferences->putBool(preference_config_from_mqtt, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "KPPUB")
        {
            if(_preferences->getBool(preference_keypad_info_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_keypad_info_enabled, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "KPCODE")
        {
            if(_preferences->getBool(preference_keypad_publish_code, false) != (value == "1"))
            {
                _preferences->putBool(preference_keypad_publish_code, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "KPCHECK")
        {
            if(_preferences->getBool(preference_keypad_check_code_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_keypad_check_code_enabled, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "KPENA")
        {
            if(_preferences->getBool(preference_keypad_control_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_keypad_control_enabled, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "TCPUB")
        {
            if(_preferences->getBool(preference_timecontrol_info_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_timecontrol_info_enabled, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "AUTHPUB")
        {
            if(_preferences->getBool(preference_auth_info_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_auth_info_enabled, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "KPPER")
        {
            if(_preferences->getBool(preference_keypad_topic_per_entry, false) != (value == "1"))
            {
                _preferences->putBool(preference_keypad_topic_per_entry, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "TCPER")
        {
            if(_preferences->getBool(preference_timecontrol_topic_per_entry, false) != (value == "1"))
            {
                _preferences->putBool(preference_timecontrol_topic_per_entry, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "TCENA")
        {
            if(_preferences->getBool(preference_timecontrol_control_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_timecontrol_control_enabled, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "AUTHPER")
        {
            if(_preferences->getBool(preference_auth_topic_per_entry, false) != (value == "1"))
            {
                _preferences->putBool(preference_auth_topic_per_entry, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "AUTHENA")
        {
            if(_preferences->getBool(preference_auth_control_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_auth_control_enabled, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "PUBAUTH")
        {
            if(_preferences->getBool(preference_publish_authdata, false) != (value == "1"))
            {
                _preferences->putBool(preference_publish_authdata, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "CREDDIGEST")
        {
            if(_preferences->getInt(preference_http_auth_type, 0) != value.toInt())
            {
                _preferences->putInt(preference_http_auth_type, value.toInt());
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
                clearSession = true;
            }
        }
        else if(key == "CREDTRUSTPROXY")
        {
            if(value != "*")
            {
                if(_preferences->getString(preference_bypass_proxy, "") != value)
                {
                    _preferences->putString(preference_bypass_proxy, value);
                    Log->print("Setting changed: ");
                    Log->println(key);
                    restartServicesNoReconnect = true;
                    clearSession = true;
                }
            }
        }
        else if(key == "ACLLCKLCK")
        {
            aclPrefs[0] = ((value == "1") ? 1 : 0);
        }
        else if(key == "ACLLCKUNLCK")
        {
            aclPrefs[1] = ((value == "1") ? 1 : 0);
        }
        else if(key == "ACLLCKUNLTCH")
        {
            aclPrefs[2] = ((value == "1") ? 1 : 0);
        }
        else if(key == "ACLLCKLNG")
        {
            aclPrefs[3] = ((value == "1") ? 1 : 0);
        }
        else if(key == "ACLLCKLNGU")
        {
            aclPrefs[4] = ((value == "1") ? 1 : 0);
        }
        else if(key == "ACLLCKFLLCK")
        {
            aclPrefs[5] = ((value == "1") ? 1 : 0);
        }
        else if(key == "ACLLCKFOB1")
        {
            aclPrefs[6] = ((value == "1") ? 1 : 0);
        }
        else if(key == "ACLLCKFOB2")
        {
            aclPrefs[7] = ((value == "1") ? 1 : 0);
        }
        else if(key == "ACLLCKFOB3")
        {
            aclPrefs[8] = ((value == "1") ? 1 : 0);
        }
        else if(key == "ACLOPNUNLCK")
        {
            aclPrefs[9] = ((value == "1") ? 1 : 0);
        }
        else if(key == "ACLOPNLCK")
        {
            aclPrefs[10] = ((value == "1") ? 1 : 0);
        }
        else if(key == "ACLOPNUNLTCH")
        {
            aclPrefs[11] = ((value == "1") ? 1 : 0);
        }
        else if(key == "ACLOPNUNLCKCM")
        {
            aclPrefs[12] = ((value == "1") ? 1 : 0);
        }
        else if(key == "ACLOPNLCKCM")
        {
            aclPrefs[13] = ((value == "1") ? 1 : 0);
        }
        else if(key == "ACLOPNFOB1")
        {
            aclPrefs[14] = ((value == "1") ? 1 : 0);
        }
        else if(key == "ACLOPNFOB2")
        {
            aclPrefs[15] = ((value == "1") ? 1 : 0);
        }
        else if(key == "ACLOPNFOB3")
        {
            aclPrefs[16] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKNAME")
        {
            basicLockConfigAclPrefs[0] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKLAT")
        {
            basicLockConfigAclPrefs[1] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKLONG")
        {
            basicLockConfigAclPrefs[2] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKAUNL")
        {
            basicLockConfigAclPrefs[3] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKPRENA")
        {
            basicLockConfigAclPrefs[4] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKBTENA")
        {
            basicLockConfigAclPrefs[5] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKLEDENA")
        {
            basicLockConfigAclPrefs[6] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKLEDBR")
        {
            basicLockConfigAclPrefs[7] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKTZOFF")
        {
            basicLockConfigAclPrefs[8] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKDSTM")
        {
            basicLockConfigAclPrefs[9] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKFOB1")
        {
            basicLockConfigAclPrefs[10] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKFOB2")
        {
            basicLockConfigAclPrefs[11] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKFOB3")
        {
            basicLockConfigAclPrefs[12] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKSGLLCK")
        {
            basicLockConfigAclPrefs[13] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKADVM")
        {
            basicLockConfigAclPrefs[14] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKTZID")
        {
            basicLockConfigAclPrefs[15] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKUPOD")
        {
            advancedLockConfigAclPrefs[0] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKLPOD")
        {
            advancedLockConfigAclPrefs[1] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKSLPOD")
        {
            advancedLockConfigAclPrefs[2] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKUTLTOD")
        {
            advancedLockConfigAclPrefs[3] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKLNGT")
        {
            advancedLockConfigAclPrefs[4] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKSBPA")
        {
            advancedLockConfigAclPrefs[5] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKDBPA")
        {
            advancedLockConfigAclPrefs[6] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKDC")
        {
            advancedLockConfigAclPrefs[7] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKBATT")
        {
            advancedLockConfigAclPrefs[8] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKABTD")
        {
            advancedLockConfigAclPrefs[9] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKUNLD")
        {
            advancedLockConfigAclPrefs[10] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKALT")
        {
            advancedLockConfigAclPrefs[11] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKAUNLD")
        {
            advancedLockConfigAclPrefs[12] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKNMENA")
        {
            advancedLockConfigAclPrefs[13] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKNMST")
        {
            advancedLockConfigAclPrefs[14] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKNMET")
        {
            advancedLockConfigAclPrefs[15] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKNMALENA")
        {
            advancedLockConfigAclPrefs[16] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKNMAULD")
        {
            advancedLockConfigAclPrefs[17] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKNMLOS")
        {
            advancedLockConfigAclPrefs[18] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKALENA")
        {
            advancedLockConfigAclPrefs[19] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKIALENA")
        {
            advancedLockConfigAclPrefs[20] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKAUENA")
        {
            advancedLockConfigAclPrefs[21] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKRBTNUKI")
        {
            advancedLockConfigAclPrefs[22] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKMTRSPD")
        {
            advancedLockConfigAclPrefs[23] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFLCKESSDNM")
        {
            advancedLockConfigAclPrefs[24] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNNAME")
        {
            basicOpenerConfigAclPrefs[0] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNLAT")
        {
            basicOpenerConfigAclPrefs[1] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNLONG")
        {
            basicOpenerConfigAclPrefs[2] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNPRENA")
        {
            basicOpenerConfigAclPrefs[3] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNBTENA")
        {
            basicOpenerConfigAclPrefs[4] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNLEDENA")
        {
            basicOpenerConfigAclPrefs[5] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNTZOFF")
        {
            basicOpenerConfigAclPrefs[6] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNDSTM")
        {
            basicOpenerConfigAclPrefs[7] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNFOB1")
        {
            basicOpenerConfigAclPrefs[8] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNFOB2")
        {
            basicOpenerConfigAclPrefs[9] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNFOB3")
        {
            basicOpenerConfigAclPrefs[10] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNOPM")
        {
            basicOpenerConfigAclPrefs[11] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNADVM")
        {
            basicOpenerConfigAclPrefs[12] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNTZID")
        {
            basicOpenerConfigAclPrefs[13] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNICID")
        {
            advancedOpenerConfigAclPrefs[0] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNBUSMS")
        {
            advancedOpenerConfigAclPrefs[1] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNSCDUR")
        {
            advancedOpenerConfigAclPrefs[2] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNESD")
        {
            advancedOpenerConfigAclPrefs[3] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNRESD")
        {
            advancedOpenerConfigAclPrefs[4] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNESDUR")
        {
            advancedOpenerConfigAclPrefs[5] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNDRTOAR")
        {
            advancedOpenerConfigAclPrefs[6] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNRTOT")
        {
            advancedOpenerConfigAclPrefs[7] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNDRBSUP")
        {
            advancedOpenerConfigAclPrefs[8] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNDRBSUPDUR")
        {
            advancedOpenerConfigAclPrefs[9] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNSRING")
        {
            advancedOpenerConfigAclPrefs[10] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNSOPN")
        {
            advancedOpenerConfigAclPrefs[11] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNSRTO")
        {
            advancedOpenerConfigAclPrefs[12] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNSCM")
        {
            advancedOpenerConfigAclPrefs[13] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNSCFRM")
        {
            advancedOpenerConfigAclPrefs[14] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNSLVL")
        {
            advancedOpenerConfigAclPrefs[15] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNSBPA")
        {
            advancedOpenerConfigAclPrefs[16] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNDBPA")
        {
            advancedOpenerConfigAclPrefs[17] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNBATT")
        {
            advancedOpenerConfigAclPrefs[18] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNABTD")
        {
            advancedOpenerConfigAclPrefs[19] = ((value == "1") ? 1 : 0);
        }
        else if(key == "CONFOPNRBTNUKI")
        {
            advancedOpenerConfigAclPrefs[20] = ((value == "1") ? 1 : 0);
        }
        else if(key == "REGAPP")
        {
            if(_preferences->getBool(preference_register_as_app, false) != (value == "1"))
            {
                _preferences->putBool(preference_register_as_app, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "REGAPPOPN")
        {
            if(_preferences->getBool(preference_register_opener_as_app, false) != (value == "1"))
            {
                _preferences->putBool(preference_register_opener_as_app, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
            }
        }
        else if(key == "LOCKENA")
        {
            if(_preferences->getBool(preference_lock_enabled, true) != (value == "1"))
            {
                _preferences->putBool(preference_lock_enabled, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "GEMINIENA")
        {
            if(_preferences->getBool(preference_lock_gemini_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_lock_gemini_enabled, (value == "1"));
                if (value == "1")
                {
                    _preferences->putBool(preference_register_as_app, true);
                    _preferences->putBool(preference_lock_enabled, true);
                    _preferences->putBool(preference_official_hybrid_enabled, true);
                    _preferences->putBool(preference_official_hybrid_actions, true);
                }
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
            }
        }
        else if(key == "OPENA")
        {
            if(_preferences->getBool(preference_opener_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_opener_enabled, (value == "1"));
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesReconnect = true;
            }
        }
        else if(key == "CREDUSER")
        {
            if(value == "#")
            {
                clearCredentials = true;
            }
            else
            {
                if(_preferences->getString(preference_cred_user, "") != value)
                {
                    _preferences->putString(preference_cred_user, value);
                    Log->print("Setting changed: ");
                    Log->println(key);
                    restartServicesNoReconnect = true;
                    clearSession = true;
                }
            }
        }
        else if(key == "CREDPASS")
        {
            pass1 = value;
        }
        else if(key == "CREDPASSRE")
        {
            pass2 = value;
        }
        else if(key == "CREDTOTP")
        {
            if(value != "*")
            {
                if(_preferences->getString(preference_totp_secret, "") != value)
                {
                    _preferences->putString(preference_totp_secret, value);
                    Log->print("Setting changed: ");
                    Log->println(key);
                    restartServicesNoReconnect = true;
                    clearSession = true;
                    newMFA = true;
                    _importExport->_sessionsOpts[request->client()->localIP().toString() + "totp"] = true;
                }
            }
        }
        else if(key == "CREDBYPASS")
        {
            if(value != "*")
            {
                if(_preferences->getString(preference_bypass_secret, "") != value)
                {
                    _preferences->putString(preference_bypass_secret, value);
                    Log->print("Setting changed: ");
                    Log->println(key);
                    restartServicesNoReconnect = true;
                }
            }
        }
        else if(key == "CREDADMIN")
        {
            if(value != "*")
            {
                if(_preferences->getString(preference_admin_secret, "") != value)
                {
                    _preferences->putString(preference_admin_secret, value);
                    Log->print("Setting changed: ");
                    Log->println(key);
                }
            }
        }
        else if(key == "NUKIPIN" && _nuki != nullptr)
        {
            if(value == "#")
            {
                if (_preferences->getBool(preference_lock_gemini_enabled, false))
                {
                    message = "Nuki Lock Ultra/Go/5th gen PIN cleared";
                    _nuki->setUltraPin(0xffffffff);
                    _preferences->putInt(preference_lock_gemini_pin, 0);
                }
                else
                {
                    message = "Nuki Lock PIN cleared";
                    _nuki->setPin(0xffff);
                }
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
            }
            else
            {
                if (_preferences->getBool(preference_lock_gemini_enabled, false))
                {
                    if(_nuki->getUltraPin() != value.toInt())
                    {
                        message = "Nuki Lock Ultra/Go/5th gen PIN saved";
                        _nuki->setUltraPin(value.toInt());
                        _preferences->putInt(preference_lock_gemini_pin, value.toInt());
                        Log->print("Setting changed: ");
                        Log->println(key);
                        restartServicesNoReconnect = true;
                    }
                }
                else
                {
                    if(_nuki->getPin() != value.toInt())
                    {
                        message = "Nuki Lock PIN saved";
                        _nuki->setPin(value.toInt());
                        Log->print("Setting changed: ");
                        Log->println(key);
                        restartServicesNoReconnect = true;
                    }
                }
            }
        }
        else if(key == "NUKIOPPIN" && _nukiOpener != nullptr)
        {
            if(value == "#")
            {
                message = "Nuki Opener PIN cleared";
                _nukiOpener->setPin(0xffff);
                Log->print("Setting changed: ");
                Log->println(key);
                restartServicesNoReconnect = true;
            }
            else
            {
                if(_nukiOpener->getPin() != value.toInt())
                {
                    message = "Nuki Opener PIN saved";
                    _nukiOpener->setPin(value.toInt());
                    Log->print("Setting changed: ");
                    Log->println(key);
                    restartServicesNoReconnect = true;
                }
            }
        }
        else if(key == "LCKMANPAIR" && (value == "1"))
        {
            manPairLck = true;
        }
        else if(key == "OPNMANPAIR" && (value == "1"))
        {
            manPairOpn = true;
        }
        else if(key == "LCKBLEADDR")
        {
            if(value.length() == 12) for(int i=0; i<value.length(); i+=2)
                {
                    currentBleAddress[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                }
        }
        else if(key == "LCKSECRETK")
        {
            if(value.length() == 64) for(int i=0; i<value.length(); i+=2)
                {
                    secretKeyK[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                }
        }
        else if(key == "LCKAUTHID")
        {
            if(value.length() == 8) for(int i=0; i<value.length(); i+=2)
                {
                    authorizationId[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                }
        }
        else if(key == "LCKISULTRA" && (value == "1"))
        {
            isUltra = true;
        }
        else if(key == "OPNBLEADDR")
        {
            if(value.length() == 12) for(int i=0; i<value.length(); i+=2)
                {
                    currentBleAddressOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                }
        }
        else if(key == "OPNSECRETK")
        {
            if(value.length() == 64) for(int i=0; i<value.length(); i+=2)
                {
                    secretKeyKOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                }
        }
        else if(key == "OPNAUTHID")
        {
            if(value.length() == 8) for(int i=0; i<value.length(); i+=2)
                {
                    authorizationIdOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                }
        }
    }

    if(networkReconfigure)
    {
        _preferences->putBool(preference_ntw_reconfigure, true);
    }

    if(manPairLck)
    {
        Log->println("Changing lock pairing");
        Preferences nukiBlePref;
        nukiBlePref.begin("NukiHub", false);
        nukiBlePref.putBytes("bleAddress", currentBleAddress, 6);
        nukiBlePref.putBytes("secretKeyK", secretKeyK, 32);
        nukiBlePref.putBytes("authorizationId", authorizationId, 4);
        nukiBlePref.putBytes("securityPinCode", pincode, 2);
        nukiBlePref.putBytes("ultraPinCode", ultraPincode, 4);
        nukiBlePref.putBool("isUltra", isUltra);

        nukiBlePref.end();
        Log->print("Setting changed: ");
        Log->println("Lock pairing data");
        restartServicesNoReconnect = true;
    }

    if(manPairOpn)
    {
        Log->println("Changing opener pairing");
        Preferences nukiBlePref;
        nukiBlePref.begin("NukiHubopener", false);
        nukiBlePref.putBytes("bleAddress", currentBleAddressOpn, 6);
        nukiBlePref.putBytes("secretKeyK", secretKeyKOpn, 32);
        nukiBlePref.putBytes("authorizationId", authorizationIdOpn, 4);
        nukiBlePref.putBytes("securityPinCode", pincode, 2);
        nukiBlePref.end();
        Log->print("Setting changed: ");
        Log->println("Opener pairing data");
        restartServicesNoReconnect = true;
    }

    if(pass1 != "" && pass1 != "*" && pass1 == pass2)
    {
        if(_preferences->getString(preference_cred_password, "") != pass1)
        {
            _preferences->putString(preference_cred_password, pass1);
            Log->print("Setting changed: ");
            Log->println("CREDPASS");
            restartServicesNoReconnect = true;
            clearSession = true;
        }
    }

    if(clearMqttCredentials)
    {
        if(_preferences->getString(preference_mqtt_user, "") != "")
        {
            _preferences->putString(preference_mqtt_user, "");
            Log->print("Setting changed: ");
            Log->println("MQTTUSER");
            restartServicesReconnect = true;
        }
        if(_preferences->getString(preference_mqtt_password, "") != "")
        {
            _preferences->putString(preference_mqtt_password, "");
            Log->print("Setting changed: ");
            Log->println("MQTTPASS");
            restartServicesReconnect = true;
        }
    }

    if(clearCredentials)
    {
        if(_preferences->getString(preference_cred_user, "") != "")
        {
            _preferences->putString(preference_cred_user, "");
            Log->print("Setting changed: ");
            Log->println("CREDUSER");
            restartServicesNoReconnect = true;
            clearSession = true;
        }
        if(_preferences->getString(preference_cred_password, "") != "")
        {
            _preferences->putString(preference_cred_password, "");
            Log->print("Setting changed: ");
            Log->println("CREDPASS");
            restartServicesNoReconnect = true;
            clearSession = true;
        }
    }

    if(aclLvlChanged)
    {
        uint32_t curAclPrefs[17] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        uint32_t curBasicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        uint32_t curAdvancedLockConfigAclPrefs[25] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        uint32_t curBasicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        uint32_t curAdvancedOpenerConfigAclPrefs[21] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        _preferences->getBytes(preference_acl, &curAclPrefs, sizeof(curAclPrefs));
        _preferences->getBytes(preference_conf_lock_basic_acl, &curBasicLockConfigAclPrefs, sizeof(curBasicLockConfigAclPrefs));
        _preferences->getBytes(preference_conf_lock_advanced_acl, &curAdvancedLockConfigAclPrefs, sizeof(curAdvancedLockConfigAclPrefs));
        _preferences->getBytes(preference_conf_opener_basic_acl, &curBasicOpenerConfigAclPrefs, sizeof(curBasicOpenerConfigAclPrefs));
        _preferences->getBytes(preference_conf_opener_advanced_acl, &curAdvancedOpenerConfigAclPrefs, sizeof(curAdvancedOpenerConfigAclPrefs));

        for(int i=0; i < 17; i++)
        {
            if(curAclPrefs[i] != aclPrefs[i])
            {
                _preferences->putBytes(preference_acl, (byte*)(&aclPrefs), sizeof(aclPrefs));
                Log->print("Setting changed: ");
                Log->println("ACLPREFS");
                break;
            }
        }
        for(int i=0; i < 16; i++)
        {
            if(curBasicLockConfigAclPrefs[i] != basicLockConfigAclPrefs[i])
            {
                _preferences->putBytes(preference_conf_lock_basic_acl, (byte*)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
                Log->print("Setting changed: ");
                Log->println("ACLCONFBASICLOCK");
                break;
            }
        }
        for(int i=0; i < 25; i++)
        {
            if(curAdvancedLockConfigAclPrefs[i] != advancedLockConfigAclPrefs[i])
            {
                _preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
                Log->print("Setting changed: ");
                Log->println("ACLCONFADVANCEDLOCK");
                break;

            }
        }
        for(int i=0; i < 14; i++)
        {
            if(curBasicOpenerConfigAclPrefs[i] != basicOpenerConfigAclPrefs[i])
            {
                _preferences->putBytes(preference_conf_opener_basic_acl, (byte*)(&basicOpenerConfigAclPrefs), sizeof(basicOpenerConfigAclPrefs));
                Log->print("Setting changed: ");
                Log->println("ACLCONFBASICOPENER");
                break;
            }
        }
        for(int i=0; i < 21; i++)
        {
            if(curAdvancedOpenerConfigAclPrefs[i] != advancedOpenerConfigAclPrefs[i])
            {
                _preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));
                Log->print("Setting changed: ");
                Log->println("ACLCONFADVANCEDOPENER");
                break;
            }
        }
    }

    if(clearSession)
    {
        clearSessions();
    }

    if(newMFA)
    {
        _preferences->putBool(preference_mfa_reconfigure, true);
    }

    if(configChanged)
    {
        message = "Configuration saved, reboot required to apply some settings";
        _rebootRequired = true;
    }
    else if (restartServicesReconnect)
    {
        message = "Configuration saved, restart services required to apply some settings";
        _restartServicesRequired = 2;
    }
    else if (restartServicesNoReconnect)
    {
        message = "Configuration saved, restart services required to apply some settings";
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

    return configChanged;
}

bool WebCfgServer::processImport(PsychicRequest *request, PsychicResponse* resp, String& message)
{
    bool configChanged = false;
    int params = request->params();

    for(int index = 0; index < params; index++)
    {
        const PsychicWebParameter* p = request->getParam(index);
        if(p->name() == "importjson")
        {
            JsonDocument doc;

            DeserializationError error = deserializeJson(doc, p->value());
            if (error)
            {
                Log->println("Invalid JSON for import");
                message = "Invalid JSON, config not changed";
                return configChanged;
            }

            _importExport->importJson(doc);

            configChanged = true;
        }
    }

    if(configChanged)
    {
        message = "Configuration saved, reboot is required to apply.";
        _rebootRequired = true;
    }
    else
    {
        message = "Configuration saved and applied.";
    }

    return configChanged;
}

void WebCfgServer::processGpioArgs(PsychicRequest *request, PsychicResponse* resp)
{
    int params = request->params();
    std::vector<PinEntry> pinConfiguration;

    for(int index = 0; index < params; index++)
    {
        const PsychicWebParameter* p = request->getParam(index);

        if(p->name() == "RETGPIO")
        {
            if(_preferences->getBool(preference_retain_gpio, false) != (p->value() == "1"))
            {
                _preferences->putBool(preference_retain_gpio, (p->value() == "1"));
            }
        }
        else
        {
            PinRole role = (PinRole)p->value().toInt();
            if(role != PinRole::Disabled)
            {
                PinEntry entry;
                entry.pin = p->name().toInt();
                entry.role = role;
                pinConfiguration.push_back(entry);
            }
        }
    }

    _gpio->savePinConfiguration(pinConfiguration);
}

esp_err_t WebCfgServer::buildImportExportHtml(PsychicRequest *request, PsychicResponse* resp)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<div id=\"upform\"><h4>Import configuration</h4>");
    response.print("<form method=\"post\" action=\"post\"><textarea id=\"importjson\" name=\"importjson\" rows=\"10\" cols=\"50\"></textarea><br/>");
    response.print("<input type=\"hidden\" name=\"page\" value=\"import\">");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Import\"></form><br><br></div>");
    response.print("<div id=\"gitdiv\">");
    response.print("<h4>Export configuration</h4><br>");
    response.print("<button title=\"Basic export\" onclick=\" window.open('/get?page=export', '_self'); return false;\">Basic export</button>");
    response.print("<br><br><button title=\"Export with redacted settings\" onclick=\" window.open('/get?page=export&redacted=1'); return false;\">Export with redacted settings</button>");
    response.print("<br><br><button title=\"Export with redacted settings and pairing data\" onclick=\" window.open('/get?page=export&redacted=1&pairing=1'); return false;\">Export with redacted settings and pairing data</button>");
    if(nuki_hub_https_server_enabled)
    {
        response.print("<br><br><button title=\"Export HTTP SSL certificate and key\" onclick=\" window.open('/get?page=export&type=https'); return false;\">Export HTTP SSL certificate and key</button>");
    }
    response.print("<br><br><button title=\"Export MQTT SSL CA, client certificate and client key\" onclick=\" window.open('/get?page=export&type=mqtts'); return false;\">Export MQTT SSL CA, client certificate and client key</button>");
    response.print("<br><br><button title=\"Export Coredump\" onclick=\" window.open('/get?page=coredump'); return false;\">Export Coredump</button>");
    response.print("</div></body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildCustomNetworkConfigHtml(PsychicRequest *request, PsychicResponse* resp)
{
    String header = "<script>window.onload=function(){var physelect=document.getElementsByName('NWCUSTPHY')[0];hideshowopt(physelect.value);physelect.addEventListener('change', function(event){var select=event.target;var selectedOption=select.options[select.selectedIndex];hideshowopt(selectedOption.getAttribute('value'));});};function hideshowopt(value){if(value>=1&&value<=3){hideopt('internalopt',true);hideopt('externalopt',false);}else if(value>=4&&value<=9){hideopt('internalopt', false);hideopt('externalopt', true);}else {hideopt('internalopt', true);hideopt('externalopt', true);}}function hideopt(opts,hide){var hideopts = document.getElementsByClassName(opts);for(var i=0;i<hideopts.length;i++){if(hide==true){hideopts[i].style.display='none';}else{hideopts[i].style.display='block';}}}</script>";
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response, header);
    response.print("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response.print("<h3>Custom Ethernet Configuration</h3>");
    response.print("<table>");
    printDropDown(&response, "NWCUSTPHY", "PHY", String(_preferences->getInt(preference_network_custom_phy)), getNetworkCustomPHYOptions(), "");
    printInputField(&response, "NWCUSTADDR", "ADDR", _preferences->getInt(preference_network_custom_addr, 1), 6, "");
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
    printDropDown(&response, "NWCUSTCLK", "CLK", String(_preferences->getInt(preference_network_custom_clk, 0)), getNetworkCustomCLKOptions(), "internalopt");
    printInputField(&response, "NWCUSTPWR", "PWR", _preferences->getInt(preference_network_custom_pwr, 12), 6, "class=\"internalopt\"");
    printInputField(&response, "NWCUSTMDIO", "MDIO", _preferences->getInt(preference_network_custom_mdio), 6, "class=\"internalopt\"");
    printInputField(&response, "NWCUSTMDC", "MDC", _preferences->getInt(preference_network_custom_mdc), 6, "class=\"internalopt\"");
#endif
    printInputField(&response, "NWCUSTIRQ", "IRQ", _preferences->getInt(preference_network_custom_irq, -1), 6, "class=\"externalopt\"");
    printInputField(&response, "NWCUSTRST", "RST", _preferences->getInt(preference_network_custom_rst, -1), 6, "class=\"externalopt\"");
    printInputField(&response, "NWCUSTCS", "CS", _preferences->getInt(preference_network_custom_cs, -1), 6, "class=\"externalopt\"");
    printInputField(&response, "NWCUSTSCK", "SCK", _preferences->getInt(preference_network_custom_sck, -1), 6, "class=\"externalopt\"");
    printInputField(&response, "NWCUSTMISO", "MISO", _preferences->getInt(preference_network_custom_miso, -1), 6, "class=\"externalopt\"");
    printInputField(&response, "NWCUSTMOSI", "MOSI", _preferences->getInt(preference_network_custom_mosi, -1), 6, "class=\"externalopt\"");
    response.print("</table>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form>");
    response.print("</body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildHtml(PsychicRequest *request, PsychicResponse* resp)
{
    String header = (String)"<script>let intervalId; window.onload = function() { updateInfo(); intervalId = setInterval(updateInfo, 3000); }; function updateInfo() { var request = new XMLHttpRequest(); request.open('GET', '/get?page=status', true); request.onload = () => { const obj = JSON.parse(request.responseText); if (obj.stop == 1) { clearInterval(intervalId); } for (var key of Object.keys(obj)) { if(key=='ota' && document.getElementById(key) !== null) { document.getElementById(key).innerText = \"<a href='/ota'>\" + obj[key] + \"</a>\"; } else if(document.getElementById(key) !== null) { document.getElementById(key).innerText = obj[key]; } } }; request.send(); }</script>";
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response, header);
    if(_rebootRequired)
    {
        response.print("<table><tbody><tr><td colspan=\"2\" style=\"border: 0; color: red; font-size: 32px; font-weight: bold; text-align: center;\">REBOOT REQUIRED TO APPLY SOME SETTINGS</td></tr></tbody></table>");
    }
    if(_restartServicesRequired > 0)
    {
        response.print("<table><tbody><tr><td colspan=\"2\" style=\"border: 0; color: red; font-size: 32px; font-weight: bold; text-align: center;\">RESTART SERVICES REQUIRED TO APPLY SOME SETTINGS</td></tr></tbody></table>");
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
    String restartServicesUrl = "/get?page=restartservices&CONFIRMTOKEN=" + _confirmCode;
    buildNavigationMenuEntry(&response, "Restart Services", restartServicesUrl.c_str());
    if (_preferences->getInt(preference_http_auth_type, 0) == 2)
    {
        buildNavigationMenuEntry(&response, "Logout", "/get?page=logout");
    }
    response.print("</ul></body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildCredHtml(PsychicRequest *request, PsychicResponse* resp)
{
    char chars[] = {'2', '3','4', '5', '6','7', 'A', 'B', 'C', 'D','E', 'F', 'G','H', 'I', 'J','K', 'L', 'M', 'N', 'O','P', 'Q','R', 'S', 'T','U', 'V', 'W','X', 'Y', 'Z'};
    char chars2[] = {'1', '2', '3','4', '5', '6','7', '8', '9', '0', 'A', 'B', 'C', 'D','E', 'F', 'G','H', 'I', 'J','K', 'L', 'M', 'N', 'O','P', 'Q','R', 'S', 'T','U', 'V', 'W','X', 'Y', 'Z'};

    char randomstr[17];
    randomSeed(esp_random());
    for(int i = 0; i < 16; i++)
    {
        randomstr[i] = chars[random(32)];
    }
    randomstr[16] = '\0';
    char randomstr2[33];
    randomSeed(esp_random());
    for(int i = 0; i < 32; i++)
    {
        randomstr2[i] = chars2[random(36)];
    }
    randomstr2[32] = '\0';
    char randomstr3[33];
    randomSeed(esp_random());
    for(int i = 0; i < 32; i++)
    {
        randomstr3[i] = chars2[random(36)];
    }
    randomstr3[32] = '\0';

    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form id=\"credfrm\" class=\"adapt\" onsubmit=\"return testcreds();\" method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response.print("<h3>Credentials</h3>");
    response.print("<table>");
    printInputField(&response, "CREDUSER", "User (# to clear)", _preferences->getString(preference_cred_user).c_str(), 30, "id=\"inputuser\"", false, true);
    printInputField(&response, "CREDPASS", "Password", "*", 30, "id=\"inputpass\"", true, true);
    printInputField(&response, "CREDPASSRE", "Retype password", "*", 30, "id=\"inputpass2\"", true);
    std::vector<std::pair<String, String>> httpAuthOptions;
    httpAuthOptions.push_back(std::make_pair("0", "Basic"));
    httpAuthOptions.push_back(std::make_pair("1", "Digest"));
    httpAuthOptions.push_back(std::make_pair("2", "Form"));
    printDropDown(&response, "CREDDIGEST", "HTTP Authentication type", String(_preferences->getInt(preference_http_auth_type, 0)), httpAuthOptions, "");
    printInputField(&response, "CREDTRUSTPROXY", "Bypass authentication for reverse proxy with IP", _preferences->getString(preference_bypass_proxy, "").c_str(), 255, "");
    printCheckBox(&response, "DUOENA", "Duo Push authentication enabled", _preferences->getBool(preference_cred_duo_enabled, false), "");
    printCheckBox(&response, "DUOAPPROVAL", "Require MFA (Duo/TOTP) authentication for all sensitive Nuki Hub operations (changing/exporting settings)", _preferences->getBool(preference_cred_duo_approval, false), "");
    printCheckBox(&response, "DUOBYPASS", "Bypass MFA (Duo/TOTP) authentication by pressing the BOOT button while logging in", _preferences->getBool(preference_cred_bypass_boot_btn_enabled, false), "");
    printInputField(&response, "DUOBYPASSHIGH", "Bypass MFA (Duo/TOTP) authentication by pulling GPIO High", _preferences->getInt(preference_cred_bypass_gpio_high, -1), 2, "");
    printInputField(&response, "DUOBYPASSLOW", "Bypass MFA (Duo/TOTP) authentication by pulling GPIO Low", _preferences->getInt(preference_cred_bypass_gpio_low, -1), 2, "");
    printInputField(&response, "DUOHOST", "Duo API hostname", "*", 255, "", true, false);
    printInputField(&response, "DUOIKEY", "Duo integration key", "*", 255, "", true, false);
    printInputField(&response, "DUOSKEY", "Duo secret key", "*", 255, "", true, false);
    printInputField(&response, "DUOUSER", "Duo user", "*", 255, "", true, false);
    printInputField(&response, "CREDTOTP", "TOTP Secret Key", "*", 16, "", true, false);
    response.print("<tr id=\"totpgentr\" ><td><input type=\"button\" id=\"totpgen\" onclick=\"document.getElementsByName('CREDTOTP')[0].type='text'; document.getElementsByName('CREDTOTP')[0].value='");
    response.print(randomstr);
    response.print("'; document.getElementById('totpgentr').style.display='none';\" value=\"Generate new TOTP key\"></td></tr>");
    printInputField(&response, "CREDBYPASS", "One-time MFA Bypass", "*", 32, "", true, false);
    response.print("<tr id=\"bypassgentr\" ><td><input type=\"button\" id=\"bypassgen\" onclick=\"document.getElementsByName('CREDBYPASS')[0].type='text'; document.getElementsByName('CREDBYPASS')[0].value='");
    response.print(randomstr2);
    response.print("'; document.getElementById('bypassgentr').style.display='none';\" value=\"Generate new Bypass\"></td></tr>");
    printInputField(&response, "CREDADMIN", "Admin key", "*", 32, "", true, false);
    response.print("<tr id=\"admingentr\" ><td><input type=\"button\" id=\"admingen\" onclick=\"document.getElementsByName('CREDADMIN')[0].type='text'; document.getElementsByName('CREDADMIN')[0].value='");
    response.print(randomstr3);
    response.print("'; document.getElementById('admingentr').style.display='none';\" value=\"Generate new Admin key\"></td></tr>");
    printInputField(&response, "CREDLFTM", "Session validity (in seconds)",  _preferences->getInt(preference_cred_session_lifetime, 3600), 12, "");
    printInputField(&response, "CREDLFTMRMBR", "Session validity remember (in hours)", _preferences->getInt(preference_cred_session_lifetime_remember, 720), 12, "");
    printInputField(&response, "CREDDUOLFTM", "Duo Session validity (in seconds)",  _preferences->getInt(preference_cred_session_lifetime_duo, 3600), 12, "");
    printInputField(&response, "CREDDUOLFTMRMBR", "Duo Session validity remember (in hours)", _preferences->getInt(preference_cred_session_lifetime_duo_remember, 720), 12, "");
    printInputField(&response, "CREDTOTPLFTM", "TOTP Session validity (in seconds)",  _preferences->getInt(preference_cred_session_lifetime_totp, 3600), 12, "");
    printInputField(&response, "CREDTOTPLFTMRMBR", "TOTP Session validity remember (in hours)", _preferences->getInt(preference_cred_session_lifetime_totp_remember, 720), 12, "");
    response.print("</table>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form><script>function testcreds() { var input_user = document.getElementById(\"inputuser\").value; var input_pass = document.getElementById(\"inputpass\").value; var input_pass2 = document.getElementById(\"inputpass2\").value; var pattern = /^[ -~]*$/; if(input_user == '#' || input_user == '') { return true; } if (input_pass != input_pass2) { alert('Passwords do not match'); return false;} if(!pattern.test(input_user) || !pattern.test(input_pass)) { alert('Only non unicode characters are allowed in username and password'); return false;} else { return true; } }</script>");
    if(_nuki != nullptr)
    {
        response.print("<br><br><form class=\"adapt\" method=\"post\" action=\"post\">");
        response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
        response.print("<h3>Nuki Lock PIN</h3>");
        response.print("<table>");
        printInputField(&response, "NUKIPIN", "PIN Code (# to clear)", "*", 20, "", true);
        response.print("</table>");
        response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
        response.print("</form>");
    }
    if(_nukiOpener != nullptr)
    {
        response.print("<br><br><form class=\"adapt\" method=\"post\" action=\"post\">");
        response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
        response.print("<h3>Nuki Opener PIN</h3>");
        response.print("<table>");
        printInputField(&response, "NUKIOPPIN", "PIN Code (# to clear)", "*", 20, "", true);
        response.print("</table>");
        response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
        response.print("</form>");
    }
    if(_nuki != nullptr)
    {
        response.print("<br><br><h3>Unpair Nuki Lock</h3>");
        response.print("<form class=\"adapt\" method=\"post\" action=\"/post\">");
        response.print("<input type=\"hidden\" name=\"page\" value=\"unpairlock\">");
        response.print("<table>");
        String message = "Type ";
        message.concat(_confirmCode);
        message.concat(" to confirm unpair");
        printInputField(&response, "CONFIRMTOKEN", message.c_str(), "", 10, "");
        response.print("</table>");
        response.print("<br><button type=\"submit\">OK</button></form>");
    }
    if(_nukiOpener != nullptr)
    {
        response.print("<br><br><h3>Unpair Nuki Opener</h3>");
        response.print("<form class=\"adapt\" method=\"post\" action=\"/post\">");
        response.print("<input type=\"hidden\" name=\"page\" value=\"unpairopener\">");
        response.print("<table>");
        String message = "Type ";
        message.concat(_confirmCode);
        message.concat(" to confirm unpair");
        printInputField(&response, "CONFIRMTOKEN", message.c_str(), "", 10, "");
        response.print("</table>");
        response.print("<br><button type=\"submit\">OK</button></form>");
    }
    response.print("<br><br><h3>Factory reset Nuki Hub</h3>");
    response.print("<h4 class=\"warning\">This will reset all settings to default and unpair Nuki Lock and/or Opener.");
#ifndef CONFIG_IDF_TARGET_ESP32H2
    response.print(" Optionally will also reset WiFi settings and reopen WiFi manager portal.");
#endif
    response.print("</h4>");
    response.print("<form class=\"adapt\" method=\"post\" action=\"/post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"factoryreset\">");
    response.print("<table>");
    String message = "Type ";
    message.concat(_confirmCode);
    message.concat(" to confirm factory reset");
    printInputField(&response, "CONFIRMTOKEN", message.c_str(), "", 10, "");
#ifndef CONFIG_IDF_TARGET_ESP32H2
    printCheckBox(&response, "WIFI", "Also reset WiFi settings", false, "");
#endif
    response.print("</table>");
    response.print("<br><button type=\"submit\">OK</button></form>");
    response.print("</body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildNetworkConfigHtml(PsychicRequest *request, PsychicResponse* resp)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response.print("<h3>Network Configuration</h3>");
    response.print("<table>");
    printInputField(&response, "HOSTNAME", "Hostname (needs to be unique)", _preferences->getString(preference_hostname).c_str(), 100, "");
    printDropDown(&response, "NWHW", "Network hardware", String(_preferences->getInt(preference_network_hardware)), getNetworkDetectionOptions(), "");
    printInputField(&response, "HASSCUURL", "Home Assistant device configuration URL (empty to use http://LOCALIP; fill when using a reverse proxy for example)", _preferences->getString(preference_mqtt_hass_cu_url).c_str(), 261, "");
#ifndef CONFIG_IDF_TARGET_ESP32H2
    printInputField(&response, "RSSI", "RSSI Publish interval (seconds; -1 to disable)", _preferences->getInt(preference_rssi_publish_interval), 6, "");
#endif
    printCheckBox(&response, "RSTDISC", "Restart on disconnect", _preferences->getBool(preference_restart_on_disconnect), "");
    printCheckBox(&response, "CHECKUPDATE", "Check for Firmware Updates every 24h", _preferences->getBool(preference_check_updates), "");
    printCheckBox(&response, "FINDBESTRSSI", "Find WiFi AP with strongest signal", _preferences->getBool(preference_find_best_rssi, false), "");
    if(nuki_hub_https_server_enabled)
    {
        response.print("<tr><td>Set HTTP SSL Certificate</td><td><button title=\"Set HTTP SSL Certificate\" onclick=\" window.open('/get?page=httpcrtconfig', '_self'); return false;\">Change</button></td></tr>");
        response.print("<tr><td>Set HTTP SSL Key</td><td><button title=\"Set HTTP SSL Key\" onclick=\" window.open('/get?page=httpkeyconfig', '_self'); return false;\">Change</button></td></tr>");
        response.print("<tr><td>Generate self-signed HTTP SSL Certificate and key</td><td><button title=\"Generate HTTP SSL Certificate and key\" onclick=\" window.open('/get?page=selfsignhttps', '_self'); return false;\">Generate</button></td></tr>");
        printInputField(&response, "HTTPSFQDN", "Nuki Hub FQDN for HTTP redirect", _preferences->getString(preference_https_fqdn, "").c_str(), 255, "");
    }
    response.print("</table>");
    response.print("<h3>IP Address assignment</h3>");
    response.print("<table>");
    printCheckBox(&response, "DHCPENA", "Enable DHCP", _preferences->getBool(preference_ip_dhcp_enabled), "");
    printInputField(&response, "IPADDR", "Static IP address", _preferences->getString(preference_ip_address).c_str(), 15, "");
    printInputField(&response, "IPSUB", "Subnet", _preferences->getString(preference_ip_subnet).c_str(), 15, "");
    printInputField(&response, "IPGTW", "Default gateway", _preferences->getString(preference_ip_gateway).c_str(), 15, "");
    printInputField(&response, "DNSSRV", "DNS Server", _preferences->getString(preference_ip_dns_server).c_str(), 15, "");
    response.print("</table>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form>");
    response.print("</body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildMqttConfigHtml(PsychicRequest *request, PsychicResponse* resp)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response.print("<h3>Basic MQTT Configuration</h3>");
    response.print("<table>");
    printInputField(&response, "MQTTSERVER", "MQTT Broker", _preferences->getString(preference_mqtt_broker).c_str(), 100, "");
    printInputField(&response, "MQTTPORT", "MQTT Broker port", _preferences->getInt(preference_mqtt_broker_port), 5, "");
    printInputField(&response, "MQTTUSER", "MQTT User (# to clear)", _preferences->getString(preference_mqtt_user).c_str(), 30, "", false, true);
    printInputField(&response, "MQTTPASS", "MQTT Password", "*", 40, "", true, true);
    printInputField(&response, "MQTTPATH", "MQTT Nuki Hub Path", _preferences->getString(preference_mqtt_lock_path).c_str(), 180, "");
    printCheckBox(&response, "ENHADISC", "Enable Home Assistant auto discovery", _preferences->getBool(preference_mqtt_hass_enabled), "chkHass");
    response.print("</table><br>");

    response.print("<h3>Advanced MQTT Configuration</h3>");
    response.print("<table>");
    printInputField(&response, "HASSDISCOVERY", "Home Assistant discovery topic (usually \"homeassistant\")", _preferences->getString(preference_mqtt_hass_discovery).c_str(), 30, "class=\"chkHass\"");
    //printCheckBox(&response, "HADEVDISC", "Use Home Assistant device based discovery (2024.11+)", _preferences->getBool(preference_hass_device_discovery), "");
    if(_preferences->getBool(preference_opener_enabled, false))
    {
        printCheckBox(&response, "OPENERCONT", "Set Nuki Opener Lock/Unlock action in Home Assistant to Continuous mode", _preferences->getBool(preference_opener_continuous_mode), "");
    }
    printCheckBox(&response, "MQTTSENA", "Enable MQTT SSL", _preferences->getBool(preference_mqtt_ssl_enabled, false), "");
    response.print("<tr><td>Set MQTT SSL CA Certificate</td><td><button title=\"Set MQTT SSL CA Certificate\" onclick=\" window.open('/get?page=mqttcaconfig', '_self'); return false;\">Change</button></td></tr>");
    response.print("<tr><td>Set MQTT SSL Client Certificate</td><td><button title=\"Set MQTT Client Certificate\" onclick=\" window.open('/get?page=mqttcrtconfig', '_self'); return false;\">Change</button></td></tr>");
    response.print("<tr><td>Set MQTT SSL Client Key</td><td><button title=\"Set MQTT SSL Client Key\" onclick=\" window.open('/get?page=mqttkeyconfig', '_self'); return false;\">Change</button></td></tr>");
    printInputField(&response, "NETTIMEOUT", "MQTT Timeout until restart (seconds; -1 to disable)", _preferences->getInt(preference_network_timeout), 5, "");
    printCheckBox(&response, "MQTTLOG", "Enable MQTT logging", _preferences->getBool(preference_mqtt_log_enabled), "");
    printCheckBox(&response, "UPDATEMQTT", "Allow updating using MQTT", _preferences->getBool(preference_update_from_mqtt), "");
    printCheckBox(&response, "DISNONJSON", "Disable some extraneous non-JSON topics", _preferences->getBool(preference_disable_non_json), "");
    printCheckBox(&response, "OFFHYBRID", "Enable hybrid official MQTT and Nuki Hub setup", _preferences->getBool(preference_official_hybrid_enabled), "");
    printCheckBox(&response, "HYBRIDACT", "Enable sending actions through official MQTT", _preferences->getBool(preference_official_hybrid_actions), "");
    printInputField(&response, "HYBRIDTIMER", "Time between status updates when official MQTT is offline (seconds)", _preferences->getInt(preference_query_interval_hybrid_lockstate), 5, "");
    printCheckBox(&response, "HYBRIDRETRY", "Retry command sent using official MQTT over BLE if failed", _preferences->getBool(preference_official_hybrid_retry), "");
    printCheckBox(&response, "HYBRIDREBOOT", "Reboot Nuki lock on official MQTT failure", _preferences->getBool(preference_hybrid_reboot_on_disconnect, false), "");
    response.print("</table>");
    response.print("* If no encryption is configured for the MQTT broker, leave empty.<br><br>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form>");
    response.print("</body>");
    response.print("<script>window.onload = function() { var hassChk; var hassTxt; for (var el of document.getElementsByClassName('chkHass')) { if (el.constructor.name === 'HTMLInputElement' && el.type === 'checkbox') { hassChk = el; el.addEventListener('change', hassChkChange); } else if (el.constructor.name==='HTMLInputElement' && el.type==='text') { hassTxt=el; el.addEventListener('keyup', hassTxtChange); } } function hassChkChange() { if(hassChk.checked == true) { if(hassTxt.value.length == 0) { hassTxt.value = 'homeassistant'; } } else { hassTxt.value = ''; } } function hassTxtChange() { if(hassTxt.value.length == 0) { hassChk.checked = false; } else { hassChk.checked = true; } } };</script>");
    response.print("</html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildMqttSSLConfigHtml(PsychicRequest *request, PsychicResponse* resp, int type)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response.print("<input type=\"hidden\" name=\"mqttssl\" value=\"1\">");
    response.print("<h3>MQTT SSL Configuration</h3>");
    response.print("<table>");

    if (type == 0)
    {
        bool found = false;

        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
        }
        else
        {
            File file = SPIFFS.open("/mqtt_ssl.ca");
            if (!file || file.isDirectory())
            {
                Log->println("mqtt_ssl.ca not found");
            }
            else
            {
                Log->println("Reading mqtt_ssl.ca");
                size_t filesize = file.size();
                char ca[filesize + 1];

                file.read((uint8_t *)ca, sizeof(ca));
                file.close();
                ca[filesize] = '\0';

                printTextarea(&response, "MQTTCA", "MQTT SSL CA Certificate (*, optional)", "*", 2200, true, true);
                found = true;
            }
        }

        if (!found)
        {
            printTextarea(&response, "MQTTCA", "MQTT SSL CA Certificate (*, optional)", "", 2200, true, true);
        }
    }
    else if (type == 1)
    {
        bool found = false;

        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
        }
        else
        {
            File file = SPIFFS.open("/mqtt_ssl.crt");
            if (!file || file.isDirectory())
            {
                Log->println("mqtt_ssl.crt not found");
            }
            else
            {
                Log->println("Reading mqtt_ssl.crt");
                size_t filesize = file.size();
                char cert[filesize + 1];

                file.read((uint8_t *)cert, sizeof(cert));
                file.close();
                cert[filesize] = '\0';

                printTextarea(&response, "MQTTCRT", "MQTT SSL Client Certificate (*, optional)", "*", 2200, true, true);
                found = true;
            }
        }

        if (!found)
        {
            printTextarea(&response, "MQTTCRT", "MQTT SSL Client Certificate (*, optional)", "", 2200, true, true);
        }
    }
    else
    {
        bool found = false;

        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
        }
        else
        {
            File file = SPIFFS.open("/mqtt_ssl.key");
            if (!file || file.isDirectory())
            {
                Log->println("mqtt_ssl.key not found");
            }
            else
            {
                Log->println("Reading mqtt_ssl.key");
                size_t filesize = file.size();
                char key[filesize + 1];

                file.read((uint8_t *)key, sizeof(key));
                file.close();
                key[filesize] = '\0';

                printTextarea(&response, "MQTTKEY", "MQTT SSL Client Key (*, optional)", "*", 2200, true, true);
                found = true;
            }
        }

        if (!found)
        {
            printTextarea(&response, "MQTTKEY", "MQTT SSL Client Key (*, optional)", "", 2200, true, true);
        }
    }
    response.print("</table>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form>");
    response.print("</body>");
    response.print("</html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildHttpSSLConfigHtml(PsychicRequest *request, PsychicResponse* resp, int type)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response.print("<input type=\"hidden\" name=\"httpssl\" value=\"1\">");
    response.print("<h3>HTTP SSL Configuration</h3>");
    response.print("<table>");

    if (type == 1)
    {
        bool found = false;

        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
        }
        else
        {
            File file = SPIFFS.open("/http_ssl.crt");
            if (!file || file.isDirectory())
            {
                Log->println("http_ssl.crt not found");
            }
            else
            {
                Log->println("Reading http_ssl.crt");
                size_t filesize = file.size();
                char cert[filesize + 1];

                file.read((uint8_t *)cert, sizeof(cert));
                file.close();
                cert[filesize] = '\0';

                printTextarea(&response, "HTTPCRT", "HTTP SSL Certificate (*, optional)", "*", 4400, true, true);
                found = true;
            }
        }

        if (!found)
        {
            printTextarea(&response, "HTTPCRT", "HTTP SSL Certificate (*, optional)", "", 4400, true, true);
        }
    }
    else if (type == 2)
    {
        bool found = false;

        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
        }
        else
        {
            File file = SPIFFS.open("/http_ssl.key");
            if (!file || file.isDirectory())
            {
                Log->println("http_ssl.key not found");
            }
            else
            {
                Log->println("Reading http_ssl.key");
                size_t filesize = file.size();
                char key[filesize + 1];

                file.read((uint8_t *)key, sizeof(key));
                file.close();
                key[filesize] = '\0';

                printTextarea(&response, "HTTPKEY", "HTTP SSL Key (*, optional)", "*", 2200, true, true);
                found = true;
            }
        }

        if (!found)
        {
            printTextarea(&response, "HTTPKEY", "HTTP SSL Key (*, optional)", "", 2200, true, true);
        }
    }
    else
    {
        response.print("<input type=\"hidden\" name=\"HTTPGEN\" value=\"1\">");
        response.print("<tr><td>Click save to generate a HTTPS SSL Certificate and key</td></tr>");
    }
    response.print("</table>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form>");
    response.print("</body>");
    response.print("</html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildAdvancedConfigHtml(PsychicRequest *request, PsychicResponse* resp)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response.print("<h3>Advanced Configuration</h3>");
    response.print("<h4 class=\"warning\">Warning: Changing these settings can lead to bootloops that might require you to erase the ESP32 and reflash Nuki Hub using USB/serial</h4>");
    response.print("<table>");
    response.print("<tr><td>Current bootloop prevention state</td><td>");
    response.print(_preferences->getBool(preference_enable_bootloop_reset, false) ? "Enabled" : "Disabled");
    response.print("</td></tr>");
    printCheckBox(&response, "DISNTWNOCON", "Disable Network if not connected within 60s", _preferences->getBool(preference_disable_network_not_connected, false), "");
    printCheckBox(&response, "WEBLOG", "Enable WebSerial logging", _preferences->getBool(preference_webserial_enabled), "");
    printCheckBox(&response, "BTLPRST", "Enable Bootloop prevention (Try to reset these settings to default on bootloop)", true, "");
    printInputField(&response, "BUFFSIZE", "Char buffer size (min 4096, max 65536)", _preferences->getInt(preference_buffer_size, CHAR_BUFFER_SIZE), 6, "");
    response.print("<tr><td>Advised minimum char buffer size based on current settings</td><td id=\"mincharbuffer\"></td>");
    printInputField(&response, "TSKNTWK", "Task size Network (min 12288, max 65536)", _preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE), 6, "");
    response.print("<tr><td>Advised minimum network task size based on current settings</td><td id=\"minnetworktask\"></td>");
    printInputField(&response, "TSKNUKI", "Task size Nuki (min 8192, max 65536)", _preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE), 6, "");
    printInputField(&response, "ALMAX", "Max auth log entries (min 1, max 100)", _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG), 3, "id=\"inputmaxauthlog\"");
    printInputField(&response, "KPMAX", "Max keypad entries (min 1, max 200)", _preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD), 3, "id=\"inputmaxkeypad\"");
    printInputField(&response, "TCMAX", "Max timecontrol entries (min 1, max 100)", _preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL), 3, "id=\"inputmaxtimecontrol\"");
    printInputField(&response, "AUTHMAX", "Max authorization entries (min 1, max 100)", _preferences->getInt(preference_auth_max_entries, MAX_AUTH), 3, "id=\"inputmaxauth\"");
    printCheckBox(&response, "SHOWSECRETS", "Show Pairing secrets on Info page", _preferences->getBool(preference_show_secrets), "");
    if(_preferences->getBool(preference_lock_enabled, true))
    {
        printCheckBox(&response, "LCKMANPAIR", "Manually set lock pairing data (enable to save values below)", false, "");
        printInputField(&response, "LCKBLEADDR", "currentBleAddress", "", 12, "");
        printInputField(&response, "LCKSECRETK", "secretKeyK", "", 64, "");
        printInputField(&response, "LCKAUTHID", "authorizationId", "", 8, "");
        printCheckBox(&response, "LCKISULTRA", "isUltra", false, "");
    }
    if(_preferences->getBool(preference_opener_enabled, false))
    {
        printCheckBox(&response, "OPNMANPAIR", "Manually set opener pairing data (enable to save values below)", false, "");
        printInputField(&response, "OPNBLEADDR", "currentBleAddress", "", 12, "");
        printInputField(&response, "OPNSECRETK", "secretKeyK", "", 64, "");
        printInputField(&response, "OPNAUTHID", "authorizationId", "", 8, "");
    }
    printInputField(&response, "OTAUPD", "Custom URL to update Nuki Hub updater", "", 255, "");
    printInputField(&response, "OTAMAIN", "Custom URL to update Nuki Hub", "", 255, "");

    if(_nuki != nullptr)
    {
        char uidString[20];
        itoa(_preferences->getUInt(preference_nuki_id_lock, 0), uidString, 16);
        printCheckBox(&response, "LCKFORCEID", ((String)"Force Lock ID to current ID (" + uidString + ")").c_str(), _preferences->getBool(preference_lock_force_id, false), "");
        printCheckBox(&response, "LCKFORCEKP", "Force Lock Keypad connected", _preferences->getBool(preference_lock_force_keypad, false), "");
        printCheckBox(&response, "LCKFORCEDS", "Force Lock Doorsensor connected", _preferences->getBool(preference_lock_force_doorsensor, false), "");
    }

    if(_nukiOpener != nullptr)
    {
        char uidString[20];
        itoa(_preferences->getUInt(preference_nuki_id_opener, 0), uidString, 16);
        printCheckBox(&response, "OPFORCEID", ((String)"Force Opener ID to current ID (" + uidString + ")").c_str(), _preferences->getBool(preference_opener_force_id, false), "");
        printCheckBox(&response, "OPFORCEKP", "Force Opener Keypad", _preferences->getBool(preference_opener_force_keypad, false), "");
    }

    printCheckBox(&response, "DBGCONN", "Enable Nuki connect debug logging", _preferences->getBool(preference_debug_connect, false), "");
    printCheckBox(&response, "DBGCOMMU", "Enable Nuki communication debug logging", _preferences->getBool(preference_debug_communication, false), "");
    printCheckBox(&response, "DBGREAD", "Enable Nuki readable data debug logging", _preferences->getBool(preference_debug_readable_data, false), "");
    printCheckBox(&response, "DBGHEX", "Enable Nuki hex data debug logging", _preferences->getBool(preference_debug_hex_data, false), "");
    printCheckBox(&response, "DBGCOMM", "Enable Nuki command debug logging", _preferences->getBool(preference_debug_command, false), "");
    printCheckBox(&response, "DBGHEAP", "Pubish free heap over MQTT", _preferences->getBool(preference_publish_debug_info, false), "");
    response.print("</table>");

    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form>");
    response.print("</body><script>window.onload = function() { document.getElementById(\"inputmaxauthlog\").addEventListener(\"keyup\", calculate);document.getElementById(\"inputmaxkeypad\").addEventListener(\"keyup\", calculate);document.getElementById(\"inputmaxtimecontrol\").addEventListener(\"keyup\", calculate);document.getElementById(\"inputmaxauth\").addEventListener(\"keyup\", calculate); calculate(); }; function calculate() { var auth = document.getElementById(\"inputmaxauth\").value; var authlog = document.getElementById(\"inputmaxauthlog\").value; var keypad = document.getElementById(\"inputmaxkeypad\").value; var timecontrol = document.getElementById(\"inputmaxtimecontrol\").value; var charbuf = 0; var networktask = 0; var sizeauth = 0; var sizeauthlog = 0; var sizekeypad = 0; var sizetimecontrol = 0; if(auth > 0) { sizeauth = 300 * auth; } if(authlog > 0) { sizeauthlog = 280 * authlog; } if(keypad > 0) { sizekeypad = 350 * keypad; } if(timecontrol > 0) { sizetimecontrol = 120 * timecontrol; } charbuf = sizetimecontrol; networktask = 10240 + sizetimecontrol; if(sizeauthlog>sizekeypad && sizeauthlog>sizetimecontrol && sizeauthlog>sizeauth) { charbuf = sizeauthlog; networktask = 10240 + sizeauthlog;} else if(sizekeypad>sizeauthlog && sizekeypad>sizetimecontrol && sizekeypad>sizeauth) { charbuf = sizekeypad; networktask = 10240 + sizekeypad;} else if(sizeauth>sizeauthlog && sizeauth>sizetimecontrol && sizeauth>sizekeypad) { charbuf = sizeauth; networktask = 10240 + sizeauth;} if(charbuf<4096) { charbuf = 4096; } else if (charbuf>65536) { charbuf = 65536; } if(networktask<12288) { networktask = 12288; } else if (networktask>65536) { networktask = 65536; } document.getElementById(\"mincharbuffer\").innerHTML = charbuf; document.getElementById(\"minnetworktask\").innerHTML = networktask; }</script></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildStatusHtml(PsychicRequest *request, PsychicResponse* resp)
{
    JsonDocument json;
    String jsonStr;
    bool mqttDone = false;
    bool lockDone = false;
    bool openerDone = false;

    json["stop"] = 0;

    if(_network->mqttConnectionState() > 0)
    {
        json["mqttState"] = "Yes";
        mqttDone = true;
    }
    else
    {
        json["mqttState"] = "No";
    }

    if(_nuki != nullptr)
    {
        char lockStateArr[20];
        NukiLock::lockstateToString(_nuki->keyTurnerState().lockState, lockStateArr);
        String lockState = lockStateArr;
        String LockPaired = (_nuki->isPaired() ? ("Yes (BLE Address " + _nuki->getBleAddress().toString() + ")").c_str() : "No");
        json["lockPaired"] = LockPaired;
        json["lockState"] = lockState;

        if(_nuki->isPaired())
        {
            json["lockPin"] = pinStateToString((NukiPinState)_preferences->getInt(preference_lock_pin_status, (int)NukiPinState::NotConfigured));
            if(strcmp(lockStateArr, "undefined") != 0)
            {
                lockDone = true;
            }
        }
        else
        {
            json["lockPin"] = "Not Paired";
        }
    }
    else
    {
        lockDone = true;
    }
    if(_nukiOpener != nullptr)
    {
        char openerStateArr[20];
        NukiOpener::lockstateToString(_nukiOpener->keyTurnerState().lockState, openerStateArr);
        String openerState = openerStateArr;
        String openerPaired = (_nukiOpener->isPaired() ? ("Yes (BLE Address " + _nukiOpener->getBleAddress().toString() + ")").c_str() : "No");
        json["openerPaired"] = openerPaired;

        if(_nukiOpener->keyTurnerState().nukiState == NukiOpener::State::ContinuousMode)
        {
            json["openerState"] = "Open (Continuous Mode)";
        }
        else
        {
            json["openerState"] = openerState;
        }

        if(_nukiOpener->isPaired())
        {
            json["openerPin"] = pinStateToString((NukiPinState)_preferences->getInt(preference_opener_pin_status, (int)NukiPinState::NotConfigured));
            if(strcmp(openerStateArr, "undefined") != 0)
            {
                openerDone = true;
            }
        }
        else
        {
            json["openerPin"] = "Not Paired";
        }
    }
    else
    {
        openerDone = true;
    }

    if(_preferences->getBool(preference_check_updates))
    {
        json["latestFirmware"] = _preferences->getString(preference_latest_version);
    }

    if(mqttDone && lockDone && openerDone)
    {
        json["stop"] = 1;
    }

    serializeJson(json, jsonStr);
    resp->setCode(200);
    resp->setContentType("application/json");
    resp->setContent(jsonStr.c_str());
    return resp->send();
}

const String WebCfgServer::pinStateToString(const NukiPinState& value) const
{
    switch(value)
    {
    case NukiPinState::NotSet:
        return String("PIN not set");
    case NukiPinState::Valid:
        return String("PIN valid");
    case NukiPinState::Invalid:
        return String("PIN set but invalid");
    case NukiPinState::NotConfigured:
    default:
        return String("Unknown");
    }
}

esp_err_t WebCfgServer::buildAccLvlHtml(PsychicRequest *request, PsychicResponse* resp)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);

    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    response.print("<form method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response.print("<input type=\"hidden\" name=\"ACLLVLCHANGED\" value=\"1\">");
    response.print("<h3>Nuki General Access Control</h3>");
    response.print("<table><tr><th>Setting</th><th>Enabled</th></tr>");
    printCheckBox(&response, "CONFNHPUB", "Publish Nuki Hub configuration information", _preferences->getBool(preference_publish_config, false), "");
    printCheckBox(&response, "CONFNHCTRL", "Modify Nuki Hub configuration over MQTT", _preferences->getBool(preference_config_from_mqtt, false), "");
    printCheckBox(&response, "CONFPUB", "Publish Nuki configuration information", _preferences->getBool(preference_conf_info_enabled, true), "");

    if((_nuki != nullptr && _nuki->hasKeypad()) || (_nukiOpener != nullptr && _nukiOpener->hasKeypad()))
    {
        printCheckBox(&response, "KPPUB", "Publish keypad entries information", _preferences->getBool(preference_keypad_info_enabled), "");
        printCheckBox(&response, "KPPER", "Publish a topic per keypad entry and create HA sensor", _preferences->getBool(preference_keypad_topic_per_entry), "");
        printCheckBox(&response, "KPCODE", "Also publish keypad codes (<span class=\"warning\">Disadvised for security reasons</span>)", _preferences->getBool(preference_keypad_publish_code, false), "");
        printCheckBox(&response, "KPENA", "Add, modify and delete keypad codes", _preferences->getBool(preference_keypad_control_enabled), "");
        printCheckBox(&response, "KPCHECK", "Allow checking if keypad codes are valid (<span class=\"warning\">Disadvised for security reasons</span>)", _preferences->getBool(preference_keypad_check_code_enabled, false), "");
    }
    printCheckBox(&response, "TCPUB", "Publish time control entries information", _preferences->getBool(preference_timecontrol_info_enabled), "");
    printCheckBox(&response, "TCPER", "Publish a topic per time control entry and create HA sensor", _preferences->getBool(preference_timecontrol_topic_per_entry), "");
    printCheckBox(&response, "TCENA", "Add, modify and delete time control entries", _preferences->getBool(preference_timecontrol_control_enabled), "");
    printCheckBox(&response, "AUTHPUB", "Publish authorization entries information", _preferences->getBool(preference_auth_info_enabled), "");
    printCheckBox(&response, "AUTHPER", "Publish a topic per authorization entry and create HA sensor", _preferences->getBool(preference_auth_topic_per_entry), "");
    printCheckBox(&response, "AUTHENA", "Modify and delete authorization entries", _preferences->getBool(preference_auth_control_enabled), "");
    printCheckBox(&response, "PUBAUTH", "Publish authorization log", _preferences->getBool(preference_publish_authdata), "");
    response.print("</table><br>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");

    if(_nuki != nullptr)
    {
        uint32_t basicLockConfigAclPrefs[16];
        _preferences->getBytes(preference_conf_lock_basic_acl, &basicLockConfigAclPrefs, sizeof(basicLockConfigAclPrefs));
        uint32_t advancedLockConfigAclPrefs[25];
        _preferences->getBytes(preference_conf_lock_advanced_acl, &advancedLockConfigAclPrefs, sizeof(advancedLockConfigAclPrefs));

        response.print("<h3>Nuki Lock Access Control</h3>");
        response.print("<input type=\"button\" value=\"Allow all\" style=\"margin-right: 10px;\" onclick=\"");
        response.print("for(el of document.getElementsByClassName('chk_access_lock')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=true;}\">");
        response.print("<input type=\"button\" value=\"Disallow all\" onclick=\"");
        response.print("for(el of document.getElementsByClassName('chk_access_lock')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=false;}\">");
        response.print("<table><tr><th>Action</th><th>Allowed</th></tr>");

        printCheckBox(&response, "ACLLCKLCK", "Lock", ((int)aclPrefs[0] == 1), "chk_access_lock");
        printCheckBox(&response, "ACLLCKUNLCK", "Unlock", ((int)aclPrefs[1] == 1), "chk_access_lock");
        printCheckBox(&response, "ACLLCKUNLTCH", "Unlatch", ((int)aclPrefs[2] == 1), "chk_access_lock");
        printCheckBox(&response, "ACLLCKLNG", "Lock N Go", ((int)aclPrefs[3] == 1), "chk_access_lock");
        printCheckBox(&response, "ACLLCKLNGU", "Lock N Go Unlatch", ((int)aclPrefs[4] == 1), "chk_access_lock");
        printCheckBox(&response, "ACLLCKFLLCK", "Full Lock", ((int)aclPrefs[5] == 1), "chk_access_lock");
        printCheckBox(&response, "ACLLCKFOB1", "Fob Action 1", ((int)aclPrefs[6] == 1), "chk_access_lock");
        printCheckBox(&response, "ACLLCKFOB2", "Fob Action 2", ((int)aclPrefs[7] == 1), "chk_access_lock");
        printCheckBox(&response, "ACLLCKFOB3", "Fob Action 3", ((int)aclPrefs[8] == 1), "chk_access_lock");
        response.print("</table><br>");

        response.print("<h3>Nuki Lock Config Control (Requires PIN to be set)</h3>");
        response.print("<input type=\"button\" value=\"Allow all\" style=\"margin-right: 10px;\" onclick=\"");
        response.print("for(el of document.getElementsByClassName('chk_config_lock')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=true;}\">");
        response.print("<input type=\"button\" value=\"Disallow all\" onclick=\"");
        response.print("for(el of document.getElementsByClassName('chk_config_lock')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=false;}\">");
        response.print("<table><tr><th>Change</th><th>Allowed</th></tr>");

        printCheckBox(&response, "CONFLCKNAME", "Name", ((int)basicLockConfigAclPrefs[0] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKLAT", "Latitude", ((int)basicLockConfigAclPrefs[1] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKLONG", "Longitude", ((int)basicLockConfigAclPrefs[2] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKAUNL", "Auto unlatch", ((int)basicLockConfigAclPrefs[3] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKPRENA", "Pairing enabled", ((int)basicLockConfigAclPrefs[4] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKBTENA", "Button enabled", ((int)basicLockConfigAclPrefs[5] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKLEDENA", "LED flash enabled", ((int)basicLockConfigAclPrefs[6] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKLEDBR", "LED brightness", ((int)basicLockConfigAclPrefs[7] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKTZOFF", "Timezone offset", ((int)basicLockConfigAclPrefs[8] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKDSTM", "DST mode", ((int)basicLockConfigAclPrefs[9] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKFOB1", "Fob Action 1", ((int)basicLockConfigAclPrefs[10] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKFOB2", "Fob Action 2", ((int)basicLockConfigAclPrefs[11] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKFOB3", "Fob Action 3", ((int)basicLockConfigAclPrefs[12] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKSGLLCK", "Single Lock", ((int)basicLockConfigAclPrefs[13] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKADVM", "Advertising Mode", ((int)basicLockConfigAclPrefs[14] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKTZID", "Timezone ID", ((int)basicLockConfigAclPrefs[15] == 1), "chk_config_lock");

        printCheckBox(&response, "CONFLCKUPOD", "Unlocked Position Offset Degrees", ((int)advancedLockConfigAclPrefs[0] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKLPOD", "Locked Position Offset Degrees", ((int)advancedLockConfigAclPrefs[1] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKSLPOD", "Single Locked Position Offset Degrees", ((int)advancedLockConfigAclPrefs[2] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKUTLTOD", "Unlocked To Locked Transition Offset Degrees", ((int)advancedLockConfigAclPrefs[3] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKLNGT", "Lock n Go timeout", ((int)advancedLockConfigAclPrefs[4] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKSBPA", "Single button press action", ((int)advancedLockConfigAclPrefs[5] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKDBPA", "Double button press action", ((int)advancedLockConfigAclPrefs[6] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKDC", "Detached cylinder", ((int)advancedLockConfigAclPrefs[7] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKBATT", "Battery type", ((int)advancedLockConfigAclPrefs[8] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKABTD", "Automatic battery type detection", ((int)advancedLockConfigAclPrefs[9] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKUNLD", "Unlatch duration", ((int)advancedLockConfigAclPrefs[10] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKALT", "Auto lock timeout", ((int)advancedLockConfigAclPrefs[11] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKAUNLD", "Auto unlock disabled", ((int)advancedLockConfigAclPrefs[12] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKNMENA", "Nightmode enabled", ((int)advancedLockConfigAclPrefs[13] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKNMST", "Nightmode start time", ((int)advancedLockConfigAclPrefs[14] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKNMET", "Nightmode end time", ((int)advancedLockConfigAclPrefs[15] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKNMALENA", "Nightmode auto lock enabled", ((int)advancedLockConfigAclPrefs[16] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKNMAULD", "Nightmode auto unlock disabled", ((int)advancedLockConfigAclPrefs[17] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKNMLOS", "Nightmode immediate lock on start", ((int)advancedLockConfigAclPrefs[18] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKALENA", "Auto lock enabled", ((int)advancedLockConfigAclPrefs[19] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKIALENA", "Immediate auto lock enabled", ((int)advancedLockConfigAclPrefs[20] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKAUENA", "Auto update enabled", ((int)advancedLockConfigAclPrefs[21] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKRBTNUKI", "Reboot Nuki", ((int)advancedLockConfigAclPrefs[22] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKMTRSPD", "Motor speed", ((int)advancedLockConfigAclPrefs[23] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKESSDNM", "Enable slow speed during nightmode", ((int)advancedLockConfigAclPrefs[24] == 1), "chk_config_lock");
        response.print("</table><br>");
        response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    }
    if(_nukiOpener != nullptr)
    {
        uint32_t basicOpenerConfigAclPrefs[14];
        _preferences->getBytes(preference_conf_opener_basic_acl, &basicOpenerConfigAclPrefs, sizeof(basicOpenerConfigAclPrefs));
        uint32_t advancedOpenerConfigAclPrefs[21];
        _preferences->getBytes(preference_conf_opener_advanced_acl, &advancedOpenerConfigAclPrefs, sizeof(advancedOpenerConfigAclPrefs));

        response.print("<h3>Nuki Opener Access Control</h3>");
        response.print("<input type=\"button\" value=\"Allow all\" style=\"margin-right: 10px;\" onclick=\"");
        response.print("for(el of document.getElementsByClassName('chk_access_opener')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=true;}\">");
        response.print("<input type=\"button\" value=\"Disallow all\" onclick=\"");
        response.print("for(el of document.getElementsByClassName('chk_access_opener')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=false;}\">");
        response.print("<table><tr><th>Action</th><th>Allowed</th></tr>");

        printCheckBox(&response, "ACLOPNUNLCK", "Activate Ring-to-Open", ((int)aclPrefs[9] == 1), "chk_access_opener");
        printCheckBox(&response, "ACLOPNLCK", "Deactivate Ring-to-Open", ((int)aclPrefs[10] == 1), "chk_access_opener");
        printCheckBox(&response, "ACLOPNUNLTCH", "Electric Strike Actuation", ((int)aclPrefs[11] == 1), "chk_access_opener");
        printCheckBox(&response, "ACLOPNUNLCKCM", "Activate Continuous Mode", ((int)aclPrefs[12] == 1), "chk_access_opener");
        printCheckBox(&response, "ACLOPNLCKCM", "Deactivate Continuous Mode", ((int)aclPrefs[13] == 1), "chk_access_opener");
        printCheckBox(&response, "ACLOPNFOB1", "Fob Action 1", ((int)aclPrefs[14] == 1), "chk_access_opener");
        printCheckBox(&response, "ACLOPNFOB2", "Fob Action 2", ((int)aclPrefs[15] == 1), "chk_access_opener");
        printCheckBox(&response, "ACLOPNFOB3", "Fob Action 3", ((int)aclPrefs[16] == 1), "chk_access_opener");
        response.print("</table><br>");

        response.print("<h3>Nuki Opener Config Control (Requires PIN to be set)</h3>");
        response.print("<input type=\"button\" value=\"Allow all\" style=\"margin-right: 10px;\" onclick=\"");
        response.print("for(el of document.getElementsByClassName('chk_config_opener')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=true;}\">");
        response.print("<input type=\"button\" value=\"Disallow all\" onclick=\"");
        response.print("for(el of document.getElementsByClassName('chk_config_opener')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=false;}\">");
        response.print("<table><tr><th>Change</th><th>Allowed</th></tr>");

        printCheckBox(&response, "CONFOPNNAME", "Name", ((int)basicOpenerConfigAclPrefs[0] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNLAT", "Latitude", ((int)basicOpenerConfigAclPrefs[1] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNLONG", "Longitude", ((int)basicOpenerConfigAclPrefs[2] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNPRENA", "Pairing enabled", ((int)basicOpenerConfigAclPrefs[3] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNBTENA", "Button enabled", ((int)basicOpenerConfigAclPrefs[4] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNLEDENA", "LED flash enabled", ((int)basicOpenerConfigAclPrefs[5] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNTZOFF", "Timezone offset", ((int)basicOpenerConfigAclPrefs[6] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNDSTM", "DST mode", ((int)basicOpenerConfigAclPrefs[7] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNFOB1", "Fob Action 1", ((int)basicOpenerConfigAclPrefs[8] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNFOB2", "Fob Action 2", ((int)basicOpenerConfigAclPrefs[9] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNFOB3", "Fob Action 3", ((int)basicOpenerConfigAclPrefs[10] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNOPM", "Operating Mode", ((int)basicOpenerConfigAclPrefs[11] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNADVM", "Advertising Mode", ((int)basicOpenerConfigAclPrefs[12] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNTZID", "Timezone ID", ((int)basicOpenerConfigAclPrefs[13] == 1), "chk_config_opener");

        printCheckBox(&response, "CONFOPNICID", "Intercom ID", ((int)advancedOpenerConfigAclPrefs[0] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNBUSMS", "BUS mode Switch", ((int)advancedOpenerConfigAclPrefs[1] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNSCDUR", "Short Circuit Duration", ((int)advancedOpenerConfigAclPrefs[2] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNESD", "Eletric Strike Delay", ((int)advancedOpenerConfigAclPrefs[3] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNRESD", "Random Electric Strike Delay", ((int)advancedOpenerConfigAclPrefs[4] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNESDUR", "Electric Strike Duration", ((int)advancedOpenerConfigAclPrefs[5] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNDRTOAR", "Disable RTO after ring", ((int)advancedOpenerConfigAclPrefs[6] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNRTOT", "RTO timeout", ((int)advancedOpenerConfigAclPrefs[7] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNDRBSUP", "Doorbell suppression", ((int)advancedOpenerConfigAclPrefs[8] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNDRBSUPDUR", "Doorbell suppression duration", ((int)advancedOpenerConfigAclPrefs[9] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNSRING", "Sound Ring", ((int)advancedOpenerConfigAclPrefs[10] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNSOPN", "Sound Open", ((int)advancedOpenerConfigAclPrefs[11] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNSRTO", "Sound RTO", ((int)advancedOpenerConfigAclPrefs[12] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNSCM", "Sound CM", ((int)advancedOpenerConfigAclPrefs[13] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNSCFRM", "Sound confirmation", ((int)advancedOpenerConfigAclPrefs[14] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNSLVL", "Sound level", ((int)advancedOpenerConfigAclPrefs[15] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNSBPA", "Single button press action", ((int)advancedOpenerConfigAclPrefs[16] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNDBPA", "Double button press action", ((int)advancedOpenerConfigAclPrefs[17] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNBATT", "Battery type", ((int)advancedOpenerConfigAclPrefs[18] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNABTD", "Automatic battery type detection", ((int)advancedOpenerConfigAclPrefs[19] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNRBTNUKI", "Reboot Nuki", ((int)advancedOpenerConfigAclPrefs[20] == 1), "chk_config_opener");
        response.print("</table><br>");
        response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    }
    response.print("</form>");
    response.print("</body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildNukiConfigHtml(PsychicRequest *request, PsychicResponse* resp)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response.print("<h3>Basic Nuki Configuration</h3>");
    response.print("<table>");
    printCheckBox(&response, "LOCKENA", "Nuki Lock enabled", _preferences->getBool(preference_lock_enabled, true), "");
    printCheckBox(&response, "GEMINIENA", "Nuki Smartlock Ultra/Go/5th gen enabled", _preferences->getBool(preference_lock_gemini_enabled, false), "");
    printCheckBox(&response, "OPENA", "Nuki Opener enabled", _preferences->getBool(preference_opener_enabled, false), "");
    response.print("</table><br>");
    response.print("<h3>Advanced Nuki Configuration</h3>");
    response.print("<table>");

    printInputField(&response, "LSTINT", "Query interval lock state (seconds)", _preferences->getInt(preference_query_interval_lockstate), 10, "");
    printInputField(&response, "CFGINT", "Query interval configuration (seconds)", _preferences->getInt(preference_query_interval_configuration), 10, "");
    printInputField(&response, "BATINT", "Query interval battery (seconds)", _preferences->getInt(preference_query_interval_battery), 10, "");
    if((_nuki != nullptr && _nuki->hasKeypad()) || (_nukiOpener != nullptr && _nukiOpener->hasKeypad()))
    {
        printInputField(&response, "KPINT", "Query interval keypad (seconds)", _preferences->getInt(preference_query_interval_keypad), 10, "");
    }
    printInputField(&response, "NRTRY", "Number of retries if command failed", _preferences->getInt(preference_command_nr_of_retries), 10, "");
    printInputField(&response, "TRYDLY", "Delay between retries (milliseconds)", _preferences->getInt(preference_command_retry_delay), 10, "");
    if(_preferences->getBool(preference_lock_enabled, true) && !_preferences->getBool(preference_lock_gemini_enabled, false))
    {
        printCheckBox(&response, "REGAPP", "Lock: Nuki Bridge is running alongside Nuki Hub (needs re-pairing if changed)", _preferences->getBool(preference_register_as_app), "");
    }
    if(_preferences->getBool(preference_opener_enabled, false))
    {
        printCheckBox(&response, "REGAPPOPN", "Opener: Nuki Bridge is running alongside Nuki Hub (needs re-pairing if changed)", _preferences->getBool(preference_register_opener_as_app), "");
    }
    printInputField(&response, "RSBC", "Restart if bluetooth beacons not received (seconds; -1 to disable)", _preferences->getInt(preference_restart_ble_beacon_lost), 10, "");
#if defined(CONFIG_IDF_TARGET_ESP32)
    printInputField(&response, "TXPWR", "BLE transmit power in dB (minimum -12, maximum 9)", _preferences->getInt(preference_ble_tx_power, 9), 10, "");
#else
    printInputField(&response, "TXPWR", "BLE transmit power in dB (minimum -12, maximum 20)", _preferences->getInt(preference_ble_tx_power, 9), 10, "");
#endif
    printCheckBox(&response, "UPTIME", "Update Nuki Hub and Lock/Opener time using NTP", _preferences->getBool(preference_update_time, false), "");
    printInputField(&response, "TIMESRV", "NTP server", _preferences->getString(preference_time_server, "pool.ntp.org").c_str(), 255, "");

    response.print("</table>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form>");
    response.print("</body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildGpioConfigHtml(PsychicRequest *request, PsychicResponse* resp)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savegpiocfg\">");
    response.print("<h3>GPIO Configuration</h3>");
    response.print("<table>");
    printCheckBox(&response, "RETGPIO", "Retain Input GPIO MQTT state", _preferences->getBool(preference_retain_gpio, false), "");

    std::vector<std::pair<String, String>> options;
    String gpiopreselects = "var gpio = []; ";

    const auto& availablePins = _gpio->availablePins();
    const auto& disabledPins = _gpio->getDisabledPins();

    for(const auto& pin : availablePins)
    {
        String pinStr = String(pin);
        String pinDesc = "Gpio " + pinStr;
        printDropDown(&response, pinStr.c_str(), pinDesc.c_str(), "", options, "gpioselect");
        if(std::find(disabledPins.begin(), disabledPins.end(), pin) != disabledPins.end())
        {
            gpiopreselects.concat("gpio[" + pinStr + "] = '21';");
        }
        else
        {
            gpiopreselects.concat("gpio[" + pinStr + "] = '" + getPreselectionForGpio(pin) + "';");
        }
    }

    response.print("</table>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form>");

    options = getGpioOptions();

    response.print("<script type=\"text/javascript\">" + gpiopreselects + "var gpiooptions = '");

    for(const auto& option : options)
    {
        response.print("<option value=\"");
        response.print(option.first);
        response.print("\">");
        response.print(option.second);
        response.print("</option>");
    }

    response.print("'; var gpioselects = document.getElementsByClassName('gpioselect'); for (let i = 0; i < gpioselects.length; i++) { gpioselects[i].options.length = 0; gpioselects[i].innerHTML = gpiooptions; gpioselects[i].value = gpio[gpioselects[i].name]; if(gpioselects[i].value == 21) { gpioselects[i].disabled = true; } }</script>");
    response.print("</body></html>");
    return response.endSend();
}

#ifndef CONFIG_IDF_TARGET_ESP32H2
esp_err_t WebCfgServer::buildConfigureWifiHtml(PsychicRequest *request, PsychicResponse* resp)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form method=\"get\" action=\"get\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"wifimanager\">");
    response.print("<h3>Wi-Fi</h3>");
    response.print("Click confirm to remove saved WiFi settings and restart ESP into Wi-Fi configuration mode. After restart, connect to ESP access point to reconfigure Wi-Fi.<br><br><br>");
    response.print("<input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\" /><input type=\"submit\" value=\"Reboot\" /></form>");
    response.print("</form>");
    response.print("</body></html>");
    return response.endSend();
}
#endif

esp_err_t WebCfgServer::buildInfoHtml(PsychicRequest *request, PsychicResponse* resp)
{
    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<h3>System Information</h3><pre>");
    response.print("------------ NUKI HUB ------------");
    response.print("\nDevice: ");
    response.print(NUKI_HUB_HW);
    response.print("\nVersion: ");
    response.print(NUKI_HUB_VERSION);
    response.print("\nBuild: ");
    response.print(NUKI_HUB_BUILD);
#ifndef DEBUG_NUKIHUB
    response.print("\nBuild type: Release");
#else
    response.print("\nBuild type: Debug");
#endif
    response.print("\nBuild date: ");
    response.print(NUKI_HUB_DATE);
    response.print("\nUpdater version: ");
    response.print(_preferences->getString(preference_updater_version, ""));
    response.print("\nUpdater build: ");
    response.print(_preferences->getString(preference_updater_build, ""));
    response.print("\nUpdater build date: ");
    response.print(_preferences->getString(preference_updater_date, ""));
    response.print("\nUptime (min): ");
    response.print(espMillis() / 1000 / 60);
    response.print("\nConfig version: ");
    response.print(_preferences->getInt(preference_config_version));
    response.print("\nLast restart reason FW: ");
    response.print(getRestartReason());
    response.print("\nLast restart reason ESP: ");
    response.print(getEspRestartReason());
    response.print("\nFree internal heap: ");
    response.print(ESP.getFreeHeap());
    response.print("\nTotal internal heap: ");
    response.print(ESP.getHeapSize());
#if defined(CONFIG_SOC_SPIRAM_SUPPORTED) && defined(CONFIG_SPIRAM)
    if(esp_psram_get_size() > 0)
    {
        response.print("\nPSRAM Available: Yes");
        response.print("\nFree usable PSRAM: ");
        response.print(ESP.getFreePsram());
        response.print("\nTotal usable PSRAM: ");
        response.print(ESP.getPsramSize());
        response.print("\nTotal PSRAM: ");
        response.print(esp_psram_get_size());
        response.print("\nTotal free heap: ");
        response.print(esp_get_free_heap_size());
    }
    else
    {
        response.print("\nPSRAM Available: No");
    }
#else
    response.print("\nPSRAM Available: No");
#endif
    response.print("\nNetwork task stack high watermark: ");
    response.print(uxTaskGetStackHighWaterMark(networkTaskHandle));
    response.print("\nNuki task stack high watermark: ");
    response.print(uxTaskGetStackHighWaterMark(nukiTaskHandle));
    SPIFFS.begin(true);
    response.print("\n\n------------ SPIFFS ------------");
    response.printf("\nSPIFFS Total Bytes: %u", SPIFFS.totalBytes());
    response.printf("\nSPIFFS Used Bytes: %u", SPIFFS.usedBytes());
    response.printf("\nSPIFFS Free Bytes: %u", SPIFFS.totalBytes() - SPIFFS.usedBytes());
    response.print("\n\n------------ GENERAL SETTINGS ------------");
    response.print("\nNetwork task stack size: ");
    response.print(_preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE));
    response.print("\nNuki task stack size: ");
    response.print(_preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE));
    response.print("\nCheck for updates: ");
    response.print(_preferences->getBool(preference_check_updates, false) ? "Yes" : "No");
    response.print("\nLatest version: ");
    response.print(_preferences->getString(preference_latest_version, ""));
    response.print("\nAllow update from MQTT: ");
    response.print(_preferences->getBool(preference_update_from_mqtt, false) ? "Yes" : "No");
    response.print("\nUpdate Nuki Hub and Nuki devices time using NTP: ");
    response.print(_preferences->getBool(preference_update_time, false) ? "Yes" : "No");
    response.print("\nWeb configurator username: ");
    response.print(_preferences->getString(preference_cred_user, "").length() > 0 ? "***" : "Not set");
    response.print("\nWeb configurator password: ");
    response.print(_preferences->getString(preference_cred_password, "").length() > 0 ? "***" : "Not set");
    response.print("\nWeb configurator bypass for proxy IP: ");
    response.print(_preferences->getString(preference_bypass_proxy, "").length() > 0 ? "***" : "Not set");
    response.print("\nWeb configurator authentication: ");
    response.print(_preferences->getInt(preference_http_auth_type, 0) == 0 ? "Basic" : _preferences->getInt(preference_http_auth_type, 0) == 1 ? "Digest" : "Form");
    response.print("\nSession validity (in seconds): ");
    response.print(_preferences->getInt(preference_cred_session_lifetime, 3600));
    response.print("\nSession validity remember (in hours): ");
    response.print(_preferences->getInt(preference_cred_session_lifetime_remember, 720));
    response.print("\nDuo Push MFA enabled: ");
    response.print(_preferences->getBool(preference_cred_duo_enabled, false) ? "Yes" : "No");

    if (_preferences->getBool(preference_cred_duo_enabled, false))
    {
        response.print("\nDuo Host: ");
        response.print(_preferences->getString(preference_cred_duo_host, "").length() > 0 ? "***" : "Not set");
        response.print("\nDuo IKey: ");
        response.print(_preferences->getString(preference_cred_duo_ikey, "").length() > 0 ? "***" : "Not set");
        response.print("\nDuo SKey: ");
        response.print(_preferences->getString(preference_cred_duo_skey, "").length() > 0 ? "***" : "Not set");
        response.print("\nDuo User: ");
        response.print(_preferences->getString(preference_cred_duo_user, "").length() > 0 ? "***" : "Not set");
        response.print("\nDuo Session validity (in seconds): ");
        response.print(_preferences->getInt(preference_cred_session_lifetime_duo, 3600));
        response.print("\nDuo Session validity remember (in hours): ");
        response.print(_preferences->getInt(preference_cred_session_lifetime_duo_remember, 720));
    }

    response.print("\nTOTP MFA enabled: ");
    response.print(_importExport->getTOTPEnabled() ? "Yes" : "No");

    if (_importExport->getTOTPEnabled())
    {
        response.print("\nTOTP Session validity (in seconds): ");
        response.print(_preferences->getInt(preference_cred_session_lifetime_totp, 3600));
        response.print("\nTOTP Session validity remember (in hours): ");
        response.print(_preferences->getInt(preference_cred_session_lifetime_totp_remember, 720));
    }

    response.print("\nWeb configurator enabled: ");
    response.print(_preferences->getBool(preference_webserver_enabled, true) ? "Yes" : "No");
    response.print("\nHTTP SSL: ");
    if (!SPIFFS.begin(true) || !nuki_hub_https_server_enabled)
    {
        response.print("Disabled");
    }
    else
    {
        File file = SPIFFS.open("/http_ssl.crt");
        File file2 = SPIFFS.open("/http_ssl.key");
        response.print((!file || file.isDirectory() || !file2 || file2.isDirectory()) ? "Disabled" : "Enabled");
        file.close();
        file2.close();
        response.print("\nNuki Hub FQDN for HTTP redirect: ");
        response.print(_preferences->getString(preference_https_fqdn, "").length() > 0 ? "***" : "Not set");
    }
    response.print("\nAdvanced menu enabled: ");
    response.print(_preferences->getBool(preference_enable_debug_mode, false) ? "Yes" : "No");
    response.print("\nPublish free heap over MQTT: ");
    response.print(_preferences->getBool(preference_publish_debug_info, false) ? "Yes" : "No");
    response.print("\nNuki connect debug logging enabled: ");
    response.print(_preferences->getBool(preference_debug_connect, false) ? "Yes" : "No");
    response.print("\nNuki communication debug logging enabled: ");
    response.print(_preferences->getBool(preference_debug_communication, false) ? "Yes" : "No");
    response.print("\nNuki readable data debug logging enabled: ");
    response.print(_preferences->getBool(preference_debug_readable_data, false) ? "Yes" : "No");
    response.print("\nNuki hex data debug logging enabled: ");
    response.print(_preferences->getBool(preference_debug_hex_data, false) ? "Yes" : "No");
    response.print("\nNuki command debug logging enabled: ");
    response.print(_preferences->getBool(preference_debug_command, false) ? "Yes" : "No");
    response.print("\nMQTT log enabled: ");
    response.print(_preferences->getBool(preference_mqtt_log_enabled, false) ? "Yes" : "No");
    response.print("\nWebserial enabled: ");
    response.print(_preferences->getBool(preference_webserial_enabled, false) ? "Yes" : "No");
    response.print("\nBootloop protection enabled: ");
    response.print(_preferences->getBool(preference_enable_bootloop_reset, false) ? "Yes" : "No");
    response.print("\n\n------------ NETWORK ------------");
    response.print("\nNetwork device: ");
    response.print(_network->networkDeviceName());
    response.print("\nNetwork connected: ");
    response.print(_network->isConnected() ? "Yes" : "No");
    if(_network->isConnected())
    {
        response.print("\nInternet connected: ");
        response.print(_network->isInternetConnected() ? "Yes" : "No");

        response.print("\nIP Address: ");
        response.print(_network->localIP());

        if(_network->networkDeviceName() == "Built-in Wi-Fi")
        {
#ifndef CONFIG_IDF_TARGET_ESP32H2
            response.print("\nSSID: ");
            response.print(WiFi.SSID());
            response.print("\nBSSID of AP: ");
            response.print(_network->networkBSSID());
            response.print("\nESP32 MAC address: ");
            response.print(WiFi.macAddress());
#endif
        }
        else
        {
            //Ethernet info
        }
    }
    response.print("\n\n------------ NETWORK SETTINGS ------------");
    response.print("\nNuki Hub hostname: ");
    response.print(_preferences->getString(preference_hostname, ""));
    if(_preferences->getBool(preference_ip_dhcp_enabled, true))
    {
        response.print("\nDHCP enabled: Yes");
    }
    else
    {
        response.print("\nDHCP enabled: No");
        response.print("\nStatic IP address: ");
        response.print(_preferences->getString(preference_ip_address, ""));
        response.print("\nStatic IP subnet: ");
        response.print(_preferences->getString(preference_ip_subnet, ""));
        response.print("\nStatic IP gateway: ");
        response.print(_preferences->getString(preference_ip_gateway, ""));
        response.print("\nStatic IP DNS server: ");
        response.print(_preferences->getString(preference_ip_dns_server, ""));
    }
    if(_network->networkDeviceName() == "Built-in Wi-Fi")
    {
        response.print("\nRSSI Publish interval (s): ");

        if(_preferences->getInt(preference_rssi_publish_interval, 60) < 0)
        {
            response.print("Disabled");
        }
        else
        {
            response.print(_preferences->getInt(preference_rssi_publish_interval, 60));
        }

        response.print("\nFind WiFi AP with strongest signal: ");
        response.print(_preferences->getBool(preference_find_best_rssi, false) ? "Yes" : "No");
    }
    /*
    else if(network->networkDeviceType() == NetworkDeviceType::CUSTOM)
    {

    }
    */
    response.print("\nRestart ESP32 on network disconnect enabled: ");
    response.print(_preferences->getBool(preference_restart_on_disconnect, false) ? "Yes" : "No");
    response.print("\nDisable Network if not connected within 60s: ");
    response.print(_preferences->getBool(preference_disable_network_not_connected, false) ? "Yes" : "No");
    response.print("\nMQTT Timeout until restart (s): ");
    if(_preferences->getInt(preference_network_timeout, 60) < 0)
    {
        response.print("Disabled");
    }
    else
    {
        response.print(_preferences->getInt(preference_network_timeout, 60));
    }
    response.print("\n\n------------ MQTT ------------");
    response.print("\nMQTT connected: ");
    response.print(_network->mqttConnectionState() > 0 ? "Yes" : "No");
    response.print("\nMQTT broker address: ");
    response.print(_preferences->getString(preference_mqtt_broker, ""));
    response.print("\nMQTT broker port: ");
    response.print(_preferences->getInt(preference_mqtt_broker_port, 1883));
    response.print("\nMQTT username: ");
    response.print(_preferences->getString(preference_mqtt_user, "").length() > 0 ? "***" : "Not set");
    response.print("\nMQTT password: ");
    response.print(_preferences->getString(preference_mqtt_password, "").length() > 0 ? "***" : "Not set");
    response.print("\nMQTT base topic: ");
    response.print(_preferences->getString(preference_mqtt_lock_path, ""));
    response.print("\nMQTT SSL: ");
    if(_preferences->getBool(preference_mqtt_ssl_enabled, false))
    {
        if (!SPIFFS.begin(true))
        {
            response.print("Disabled");
        }
        else
        {
            File file = SPIFFS.open("/mqtt_ssl.ca");
            if (!file || file.isDirectory())
            {
                response.print("Disabled");
            }
            else
            {
                response.print("Enabled");
                response.print("\nMQTT SSL CA: ***");
                File file2 = SPIFFS.open("/mqtt_ssl.crt");
                File file3 = SPIFFS.open("/mqtt_ssl.key");
                response.print("\nMQTT SSL CRT: ");
                response.print((!file2 || file2.isDirectory()) ? "Not set" : "***");
                response.print("\nMQTT SSL Key: ");
                response.print((!file3 || file3.isDirectory()) ? "Not set" : "***");
                file2.close();
                file3.close();
            }
            file.close();
        }
    }
    else
    {
        response.print("Disabled");
    }
    response.print("\n\n------------ BLUETOOTH ------------");
    response.print("\nBluetooth TX power (dB): ");
    response.print(_preferences->getInt(preference_ble_tx_power, 9));
    response.print("\nBluetooth command nr of retries: ");
    response.print(_preferences->getInt(preference_command_nr_of_retries, 3));
    response.print("\nBluetooth command retry delay (ms): ");
    response.print(_preferences->getInt(preference_command_retry_delay, 100));
    response.print("\nSeconds until reboot when no BLE beacons received: ");
    response.print(_preferences->getInt(preference_restart_ble_beacon_lost, 60));
    response.print("\n\n------------ QUERY / PUBLISH SETTINGS ------------");
    response.print("\nLock/Opener state query interval (s): ");
    response.print(_preferences->getInt(preference_query_interval_lockstate, 1800));
    response.print("\nPublish Nuki device authorization log: ");
    response.print(_preferences->getBool(preference_publish_authdata, false) ? "Yes" : "No");
    response.print("\nMax authorization log entries to retrieve: ");
    response.print(_preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG));
    response.print("\nBattery state query interval (s): ");
    response.print(_preferences->getInt(preference_query_interval_battery, 1800));
    response.print("\nMost non-JSON MQTT topics disabled: ");
    response.print(_preferences->getBool(preference_disable_non_json, false) ? "Yes" : "No");
    response.print("\nPublish Nuki device config: ");
    response.print(_preferences->getBool(preference_conf_info_enabled, false) ? "Yes" : "No");
    response.print("\nConfig query interval (s): ");
    response.print(_preferences->getInt(preference_query_interval_configuration, 3600));
    response.print("\nPublish Keypad info: ");
    response.print(_preferences->getBool(preference_keypad_info_enabled, false) ? "Yes" : "No");
    response.print("\nKeypad query interval (s): ");
    response.print(_preferences->getInt(preference_query_interval_keypad, 1800));
    response.print("\nEnable Keypad control: ");
    response.print(_preferences->getBool(preference_keypad_control_enabled, false) ? "Yes" : "No");
    response.print("\nPublish Keypad topic per entry: ");
    response.print(_preferences->getBool(preference_keypad_topic_per_entry, false) ? "Yes" : "No");
    response.print("\nPublish Keypad codes: ");
    response.print(_preferences->getBool(preference_keypad_publish_code, false) ? "Yes" : "No");
    response.print("\nAllow checking Keypad codes: ");
    response.print(_preferences->getBool(preference_keypad_check_code_enabled, false) ? "Yes" : "No");
    response.print("\nMax keypad entries to retrieve: ");
    response.print(_preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD));
    response.print("\nPublish timecontrol info: ");
    response.print(_preferences->getBool(preference_timecontrol_info_enabled, false) ? "Yes" : "No");
    response.print("\nKeypad query interval (s): ");
    response.print(_preferences->getInt(preference_query_interval_keypad, 1800));
    response.print("\nEnable timecontrol control: ");
    response.print(_preferences->getBool(preference_timecontrol_control_enabled, false) ? "Yes" : "No");
    response.print("\nPublish timecontrol topic per entry: ");
    response.print(_preferences->getBool(preference_timecontrol_topic_per_entry, false) ? "Yes" : "No");
    response.print("\nMax timecontrol entries to retrieve: ");
    response.print(_preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL));
    response.print("\nEnable authorization control: ");
    response.print(_preferences->getBool(preference_auth_info_enabled, false) ? "Yes" : "No");
    response.print("\nPublish authorization topic per entry: ");
    response.print(_preferences->getBool(preference_auth_topic_per_entry, false) ? "Yes" : "No");
    response.print("\nMax authorization entries to retrieve: ");
    response.print(_preferences->getInt(preference_auth_max_entries, MAX_AUTH));
    response.print("\n\n------------ HOME ASSISTANT ------------");
    response.print("\nHome Assistant auto discovery enabled: ");
    if(_preferences->getBool(preference_mqtt_hass_enabled, false) && _preferences->getString(preference_mqtt_hass_discovery, "").length() > 0)
    {
        response.print("Yes");
        response.print("\nHome Assistant auto discovery topic: ");
        response.print(_preferences->getString(preference_mqtt_hass_discovery, "") + "/");
        response.print("\nNuki Hub configuration URL for HA: ");
        response.print(_preferences->getString(preference_mqtt_hass_cu_url, "").length() > 0 ? _preferences->getString(preference_mqtt_hass_cu_url, "") : "http://" + _network->localIP());
        response.print("\nNuki Hub ID: ");
        response.print(_preferences->getULong64(preference_nukihub_id, 0));
    }
    else
    {
        response.print("No");
    }
    response.print("\n\n------------ NUKI LOCK ------------");
    if(_nuki == nullptr || !_preferences->getBool(preference_lock_enabled, true))
    {
        response.print("\nLock enabled: No");
    }
    else
    {
        response.print("\nLock enabled: Yes");
        response.print("\nLock Ultra/Go/5th gen enabled: ");
        response.print(_preferences->getBool(preference_lock_gemini_enabled, false) ? "Yes" : "No");
        response.print("\nPaired: ");
        response.print(_nuki->isPaired() ? "Yes" : "No");
        response.print("\nNuki Hub device ID: ");
        response.print(_preferences->getUInt(preference_device_id_lock, 0));
        response.print("\nNuki device ID: ");
        response.print(_preferences->getUInt(preference_nuki_id_lock, 0) > 0 ? "***" : "Not set");
        response.print("\nFirmware version: ");
        response.print(_nuki->firmwareVersion().c_str());
        response.print("\nHardware version: ");
        response.print(_nuki->hardwareVersion().c_str());
        response.print("\nValid PIN set: ");
        response.print(_nuki->isPaired() ? _nuki->isPinValid() ? "Yes" : "No" : "-");
        response.print("\nHas door sensor: ");
        response.print(_nuki->hasDoorSensor() ? "Yes" : "No");
        response.print("\nHas keypad: ");
        response.print(_nuki->hasKeypad() ? "Yes" : "No");
        if(_nuki->hasKeypad())
        {
            response.print("\nKeypad highest entries count: ");
            response.print(_preferences->getInt(preference_lock_max_keypad_code_count, 0));
        }
        response.print("\nTimecontrol highest entries count: ");
        response.print(_preferences->getInt(preference_lock_max_timecontrol_entry_count, 0));
        response.print("\nAuthorizations highest entries count: ");
        response.print(_preferences->getInt(preference_lock_max_auth_entry_count, 0));
        response.print("\nRegister as: ");
        response.print(_preferences->getBool(preference_register_as_app, false) ? "App" : "Bridge");
        response.print("\nForce Lock ID: ");
        response.print(_preferences->getBool(preference_lock_force_id, false) ? "Yes" : "No");
        response.print("\nForce Lock Keypad: ");
        response.print(_preferences->getBool(preference_lock_force_keypad, false) ? "Yes" : "No");
        response.print("\nForce Lock Doorsensor: ");
        response.print(_preferences->getBool(preference_lock_force_doorsensor, false) ? "Yes" : "No");
        response.print("\n\n------------ HYBRID MODE ------------");
        if(!_preferences->getBool(preference_official_hybrid_enabled, false))
        {
            response.print("\nHybrid mode enabled: No");
        }
        else
        {
            response.print("\nHybrid mode enabled: Yes");
            response.print("\nHybrid mode connected: ");
            response.print(_nuki->offConnected() ? "Yes": "No");
            response.print("\nReboot Nuki lock on official MQTT failure: ");
            response.print(_preferences->getBool(preference_hybrid_reboot_on_disconnect, false) ? "Yes" : "No");
            response.print("\nSending actions through official MQTT enabled: ");
            response.print(_preferences->getBool(preference_official_hybrid_actions, false) ? "Yes" : "No");
            if(_preferences->getBool(preference_official_hybrid_actions, false))
            {
                response.print("\nRetry actions through BLE enabled: ");
                response.print(_preferences->getBool(preference_official_hybrid_retry, false) ? "Yes" : "No");
            }
            response.print("\nTime between status updates when official MQTT is offline (s): ");
            response.print(_preferences->getInt(preference_query_interval_hybrid_lockstate, 600));
        }
        uint32_t basicLockConfigAclPrefs[16];
        _preferences->getBytes(preference_conf_lock_basic_acl, &basicLockConfigAclPrefs, sizeof(basicLockConfigAclPrefs));
        uint32_t advancedLockConfigAclPrefs[25];
        _preferences->getBytes(preference_conf_lock_advanced_acl, &advancedLockConfigAclPrefs, sizeof(advancedLockConfigAclPrefs));
        response.print("\n\n------------ NUKI LOCK ACL ------------");
        response.print("\nLock: ");
        response.print((int)aclPrefs[0] ? "Allowed" : "Disallowed");
        response.print("\nUnlock: ");
        response.print((int)aclPrefs[1] ? "Allowed" : "Disallowed");
        response.print("\nUnlatch: ");
        response.print((int)aclPrefs[2] ? "Allowed" : "Disallowed");
        response.print("\nLock N Go: ");
        response.print((int)aclPrefs[3] ? "Allowed" : "Disallowed");
        response.print("\nLock N Go Unlatch: ");
        response.print((int)aclPrefs[4] ? "Allowed" : "Disallowed");
        response.print("\nFull Lock: ");
        response.print((int)aclPrefs[5] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 1: ");
        response.print((int)aclPrefs[6] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 2: ");
        response.print((int)aclPrefs[7] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 3: ");
        response.print((int)aclPrefs[8] ? "Allowed" : "Disallowed");
        response.print("\n\n------------ NUKI LOCK CONFIG ACL ------------");
        response.print("\nName: ");
        response.print((int)basicLockConfigAclPrefs[0] ? "Allowed" : "Disallowed");
        response.print("\nLatitude: ");
        response.print((int)basicLockConfigAclPrefs[1] ? "Allowed" : "Disallowed");
        response.print("\nLongitude: ");
        response.print((int)basicLockConfigAclPrefs[2] ? "Allowed" : "Disallowed");
        response.print("\nAuto Unlatch: ");
        response.print((int)basicLockConfigAclPrefs[3] ? "Allowed" : "Disallowed");
        response.print("\nPairing enabled: ");
        response.print((int)basicLockConfigAclPrefs[4] ? "Allowed" : "Disallowed");
        response.print("\nButton enabled: ");
        response.print((int)basicLockConfigAclPrefs[5] ? "Allowed" : "Disallowed");
        response.print("\nLED flash enabled: ");
        response.print((int)basicLockConfigAclPrefs[6] ? "Allowed" : "Disallowed");
        response.print("\nLED brightness: ");
        response.print((int)basicLockConfigAclPrefs[7] ? "Allowed" : "Disallowed");
        response.print("\nTimezone offset: ");
        response.print((int)basicLockConfigAclPrefs[8] ? "Allowed" : "Disallowed");
        response.print("\nDST mode: ");
        response.print((int)basicLockConfigAclPrefs[9] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 1: ");
        response.print((int)basicLockConfigAclPrefs[10] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 2: ");
        response.print((int)basicLockConfigAclPrefs[11] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 3: ");
        response.print((int)basicLockConfigAclPrefs[12] ? "Allowed" : "Disallowed");
        response.print("\nSingle Lock: ");
        response.print((int)basicLockConfigAclPrefs[13] ? "Allowed" : "Disallowed");
        response.print("\nAdvertising Mode: ");
        response.print((int)basicLockConfigAclPrefs[14] ? "Allowed" : "Disallowed");
        response.print("\nTimezone ID: ");
        response.print((int)basicLockConfigAclPrefs[15] ? "Allowed" : "Disallowed");
        response.print("\nUnlocked Position Offset Degrees: ");
        response.print((int)advancedLockConfigAclPrefs[0] ? "Allowed" : "Disallowed");
        response.print("\nLocked Position Offset Degrees: ");
        response.print((int)advancedLockConfigAclPrefs[1] ? "Allowed" : "Disallowed");
        response.print("\nSingle Locked Position Offset Degrees: ");
        response.print((int)advancedLockConfigAclPrefs[2] ? "Allowed" : "Disallowed");
        response.print("\nUnlocked To Locked Transition Offset Degrees: ");
        response.print((int)advancedLockConfigAclPrefs[3] ? "Allowed" : "Disallowed");
        response.print("\nLock n Go timeout: ");
        response.print((int)advancedLockConfigAclPrefs[4] ? "Allowed" : "Disallowed");
        response.print("\nSingle button press action: ");
        response.print((int)advancedLockConfigAclPrefs[5] ? "Allowed" : "Disallowed");
        response.print("\nDouble button press action: ");
        response.print((int)advancedLockConfigAclPrefs[6] ? "Allowed" : "Disallowed");
        response.print("\nDetached cylinder: ");
        response.print((int)advancedLockConfigAclPrefs[7] ? "Allowed" : "Disallowed");
        response.print("\nBattery type: ");
        response.print((int)advancedLockConfigAclPrefs[8] ? "Allowed" : "Disallowed");
        response.print("\nAutomatic battery type detection: ");
        response.print((int)advancedLockConfigAclPrefs[9] ? "Allowed" : "Disallowed");
        response.print("\nUnlatch duration: ");
        response.print((int)advancedLockConfigAclPrefs[10] ? "Allowed" : "Disallowed");
        response.print("\nAuto lock timeout: ");
        response.print((int)advancedLockConfigAclPrefs[11] ? "Allowed" : "Disallowed");
        response.print("\nAuto unlock disabled: ");
        response.print((int)advancedLockConfigAclPrefs[12] ? "Allowed" : "Disallowed");
        response.print("\nNightmode enabled: ");
        response.print((int)advancedLockConfigAclPrefs[13] ? "Allowed" : "Disallowed");
        response.print("\nNightmode start time: ");
        response.print((int)advancedLockConfigAclPrefs[14] ? "Allowed" : "Disallowed");
        response.print("\nNightmode end time: ");
        response.print((int)advancedLockConfigAclPrefs[15] ? "Allowed" : "Disallowed");
        response.print("\nNightmode auto lock enabled: ");
        response.print((int)advancedLockConfigAclPrefs[16] ? "Allowed" : "Disallowed");
        response.print("\nNightmode auto unlock disabled: ");
        response.print((int)advancedLockConfigAclPrefs[17] ? "Allowed" : "Disallowed");
        response.print("\nNightmode immediate lock on start: ");
        response.print((int)advancedLockConfigAclPrefs[18] ? "Allowed" : "Disallowed");
        response.print("\nAuto lock enabled: ");
        response.print((int)advancedLockConfigAclPrefs[19] ? "Allowed" : "Disallowed");
        response.print("\nImmediate auto lock enabled: ");
        response.print((int)advancedLockConfigAclPrefs[20] ? "Allowed" : "Disallowed");
        response.print("\nAuto update enabled: ");
        response.print((int)advancedLockConfigAclPrefs[21] ? "Allowed" : "Disallowed");
        response.print("\nReboot Nuki: ");
        response.print((int)advancedLockConfigAclPrefs[22] ? "Allowed" : "Disallowed");
        response.print("\nMotor speed: ");
        response.print((int)advancedLockConfigAclPrefs[23] ? "Allowed" : "Disallowed");
        response.print("\nEnable slow speed during nightmode: ");
        response.print((int)advancedLockConfigAclPrefs[24] ? "Allowed" : "Disallowed");

        if(_preferences->getBool(preference_show_secrets))
        {
            char tmp[16];
            unsigned char currentBleAddress[6];
            unsigned char authorizationId[4] = {0x00};
            unsigned char secretKeyK[32] = {0x00};
            Preferences nukiBlePref;
            nukiBlePref.begin("NukiHub", false);
            nukiBlePref.getBytes("bleAddress", currentBleAddress, 6);
            nukiBlePref.getBytes("secretKeyK", secretKeyK, 32);
            nukiBlePref.getBytes("authorizationId", authorizationId, 4);
            nukiBlePref.end();
            response.print("\n\n------------ NUKI LOCK PAIRING ------------");
            response.print("\nBLE Address: ");
            for (int i = 0; i < 6; i++)
            {
                sprintf(tmp, "%02x", currentBleAddress[i]);
                response.print(tmp);
            }
            response.print("\nSecretKeyK: ");
            for (int i = 0; i < 32; i++)
            {
                sprintf(tmp, "%02x", secretKeyK[i]);
                response.print(tmp);
            }
            response.print("\nAuthorizationId: ");
            for (int i = 0; i < 4; i++)
            {
                sprintf(tmp, "%02x", authorizationId[i]);
                response.print(tmp);
            }
            uint32_t authorizationIdInt = authorizationId[0] + 256U*authorizationId[1] + 65536U*authorizationId[2] + 16777216U*authorizationId[3];
            response.print("\nAuthorizationId (UINT32_T): ");
            response.print(authorizationIdInt);
            response.print("\nPaired to Nuki Lock Ultra/Go/5th gen: ");
            response.print(nukiBlePref.getBool("isUltra", false) ? "Yes" : "No");
        }
    }

    response.print("\n\n------------ NUKI OPENER ------------");
    if(_nukiOpener == nullptr || !_preferences->getBool(preference_opener_enabled, false))
    {
        response.print("\nOpener enabled: No");
    }
    else
    {
        response.print("\nOpener enabled: Yes");
        response.print("\nPaired: ");
        response.print(_nukiOpener->isPaired() ? "Yes" : "No");
        response.print("\nNuki Hub device ID: ");
        response.print(_preferences->getUInt(preference_device_id_opener, 0));
        response.print("\nNuki device ID: ");
        response.print(_preferences->getUInt(preference_nuki_id_opener, 0) > 0 ? "***" : "Not set");
        response.print("\nFirmware version: ");
        response.print(_nukiOpener->firmwareVersion().c_str());
        response.print("\nHardware version: ");
        response.print(_nukiOpener->hardwareVersion().c_str());
        response.print("\nOpener valid PIN set: ");
        response.print(_nukiOpener->isPaired() ? _nukiOpener->isPinValid() ? "Yes" : "No" : "-");
        response.print("\nOpener has keypad: ");
        response.print(_nukiOpener->hasKeypad() ? "Yes" : "No");
        if(_nukiOpener->hasKeypad())
        {
            response.print("\nKeypad highest entries count: ");
            response.print(_preferences->getInt(preference_opener_max_keypad_code_count, 0));
        }
        response.print("\nTimecontrol highest entries count: ");
        response.print(_preferences->getInt(preference_opener_max_timecontrol_entry_count, 0));
        response.print("\nAuthorizations highest entries count: ");
        response.print(_preferences->getInt(preference_opener_max_auth_entry_count, 0));
        response.print("\nRegister as: ");
        response.print(_preferences->getBool(preference_register_opener_as_app, false) ? "App" : "Bridge");
        response.print("\nNuki Opener Lock/Unlock action set to Continuous mode in Home Assistant: ");
        response.print(_preferences->getBool(preference_opener_continuous_mode, false) ? "Yes" : "No");
        response.print("\nForce Opener ID: ");
        response.print(_preferences->getBool(preference_opener_force_id, false) ? "Yes" : "No");
        response.print("\nForce Opener Keypad: ");
        response.print(_preferences->getBool(preference_opener_force_keypad, false) ? "Yes" : "No");
        uint32_t basicOpenerConfigAclPrefs[14];
        _preferences->getBytes(preference_conf_opener_basic_acl, &basicOpenerConfigAclPrefs, sizeof(basicOpenerConfigAclPrefs));
        uint32_t advancedOpenerConfigAclPrefs[21];
        _preferences->getBytes(preference_conf_opener_advanced_acl, &advancedOpenerConfigAclPrefs, sizeof(advancedOpenerConfigAclPrefs));
        response.print("\n\n------------ NUKI OPENER ACL ------------");
        response.print("\nActivate Ring-to-Open: ");
        response.print((int)aclPrefs[9] ? "Allowed" : "Disallowed");
        response.print("\nDeactivate Ring-to-Open: ");
        response.print((int)aclPrefs[10] ? "Allowed" : "Disallowed");
        response.print("\nElectric Strike Actuation: ");
        response.print((int)aclPrefs[11] ? "Allowed" : "Disallowed");
        response.print("\nActivate Continuous Mode: ");
        response.print((int)aclPrefs[12] ? "Allowed" : "Disallowed");
        response.print("\nDeactivate Continuous Mode: ");
        response.print((int)aclPrefs[13] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 1: ");
        response.print((int)aclPrefs[14] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 2: ");
        response.print((int)aclPrefs[15] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 3: ");
        response.print((int)aclPrefs[16] ? "Allowed" : "Disallowed");
        response.print("\n\n------------ NUKI OPENER CONFIG ACL ------------");
        response.print("\nName: ");
        response.print((int)basicOpenerConfigAclPrefs[0] ? "Allowed" : "Disallowed");
        response.print("\nLatitude: ");
        response.print((int)basicOpenerConfigAclPrefs[1] ? "Allowed" : "Disallowed");
        response.print("\nLongitude: ");
        response.print((int)basicOpenerConfigAclPrefs[2] ? "Allowed" : "Disallowed");
        response.print("\nPairing enabled: ");
        response.print((int)basicOpenerConfigAclPrefs[3] ? "Allowed" : "Disallowed");
        response.print("\nButton enabled: ");
        response.print((int)basicOpenerConfigAclPrefs[4] ? "Allowed" : "Disallowed");
        response.print("\nLED flash enabled: ");
        response.print((int)basicOpenerConfigAclPrefs[5] ? "Allowed" : "Disallowed");
        response.print("\nTimezone offset: ");
        response.print((int)basicOpenerConfigAclPrefs[6] ? "Allowed" : "Disallowed");
        response.print("\nDST mode: ");
        response.print((int)basicOpenerConfigAclPrefs[7] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 1: ");
        response.print((int)basicOpenerConfigAclPrefs[8] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 2: ");
        response.print((int)basicOpenerConfigAclPrefs[9] ? "Allowed" : "Disallowed");
        response.print("\nFob Action 3: ");
        response.print((int)basicOpenerConfigAclPrefs[10] ? "Allowed" : "Disallowed");
        response.print("\nOperating Mode: ");
        response.print((int)basicOpenerConfigAclPrefs[11] ? "Allowed" : "Disallowed");
        response.print("\nAdvertising Mode: ");
        response.print((int)basicOpenerConfigAclPrefs[12] ? "Allowed" : "Disallowed");
        response.print("\nTimezone ID: ");
        response.print((int)basicOpenerConfigAclPrefs[13] ? "Allowed" : "Disallowed");
        response.print("\nIntercom ID: ");
        response.print((int)advancedOpenerConfigAclPrefs[0] ? "Allowed" : "Disallowed");
        response.print("\nBUS mode Switch: ");
        response.print((int)advancedOpenerConfigAclPrefs[1] ? "Allowed" : "Disallowed");
        response.print("\nShort Circuit Duration: ");
        response.print((int)advancedOpenerConfigAclPrefs[2] ? "Allowed" : "Disallowed");
        response.print("\nEletric Strike Delay: ");
        response.print((int)advancedOpenerConfigAclPrefs[3] ? "Allowed" : "Disallowed");
        response.print("\nRandom Electric Strike Delay: ");
        response.print((int)advancedOpenerConfigAclPrefs[4] ? "Allowed" : "Disallowed");
        response.print("\nElectric Strike Duration: ");
        response.print((int)advancedOpenerConfigAclPrefs[5] ? "Allowed" : "Disallowed");
        response.print("\nDisable RTO after ring: ");
        response.print((int)advancedOpenerConfigAclPrefs[6] ? "Allowed" : "Disallowed");
        response.print("\nRTO timeout: ");
        response.print((int)advancedOpenerConfigAclPrefs[7] ? "Allowed" : "Disallowed");
        response.print("\nDoorbell suppression: ");
        response.print((int)advancedOpenerConfigAclPrefs[8] ? "Allowed" : "Disallowed");
        response.print("\nDoorbell suppression duration: ");
        response.print((int)advancedOpenerConfigAclPrefs[9] ? "Allowed" : "Disallowed");
        response.print("\nSound Ring: ");
        response.print((int)advancedOpenerConfigAclPrefs[10] ? "Allowed" : "Disallowed");
        response.print("\nSound Open: ");
        response.print((int)advancedOpenerConfigAclPrefs[11] ? "Allowed" : "Disallowed");
        response.print("\nSound RTO: ");
        response.print((int)advancedOpenerConfigAclPrefs[12] ? "Allowed" : "Disallowed");
        response.print("\nSound CM: ");
        response.print((int)advancedOpenerConfigAclPrefs[13] ? "Allowed" : "Disallowed");
        response.print("\nSound confirmation: ");
        response.print((int)advancedOpenerConfigAclPrefs[14] ? "Allowed" : "Disallowed");
        response.print("\nSound level: ");
        response.print((int)advancedOpenerConfigAclPrefs[15] ? "Allowed" : "Disallowed");
        response.print("\nSingle button press action: ");
        response.print((int)advancedOpenerConfigAclPrefs[16] ? "Allowed" : "Disallowed");
        response.print("\nDouble button press action: ");
        response.print((int)advancedOpenerConfigAclPrefs[17] ? "Allowed" : "Disallowed");
        response.print("\nBattery type: ");
        response.print((int)advancedOpenerConfigAclPrefs[18] ? "Allowed" : "Disallowed");
        response.print("\nAutomatic battery type detection: ");
        response.print((int)advancedOpenerConfigAclPrefs[19] ? "Allowed" : "Disallowed");
        response.print("\nReboot Nuki: ");
        response.print((int)advancedOpenerConfigAclPrefs[20] ? "Allowed" : "Disallowed");
        if(_preferences->getBool(preference_show_secrets))
        {
            char tmp[16];
            unsigned char currentBleAddressOpn[6];
            unsigned char authorizationIdOpn[4] = {0x00};
            unsigned char secretKeyKOpn[32] = {0x00};
            Preferences nukiBlePref;
            nukiBlePref.begin("NukiHubopener", false);
            nukiBlePref.getBytes("bleAddress", currentBleAddressOpn, 6);
            nukiBlePref.getBytes("secretKeyK", secretKeyKOpn, 32);
            nukiBlePref.getBytes("authorizationId", authorizationIdOpn, 4);
            nukiBlePref.end();
            response.print("\n\n------------ NUKI OPENER PAIRING ------------");
            response.print("\nBLE Address: ");
            for (int i = 0; i < 6; i++)
            {
                sprintf(tmp, "%02x", currentBleAddressOpn[i]);
                response.print(tmp);
            }
            response.print("\nSecretKeyK: ");
            for (int i = 0; i < 32; i++)
            {
                sprintf(tmp, "%02x", secretKeyKOpn[i]);
                response.print(tmp);
            }
            response.print("\nAuthorizationId: ");
            for (int i = 0; i < 4; i++)
            {
                sprintf(tmp, "%02x", authorizationIdOpn[i]);
                response.print(tmp);
            }
            uint32_t authorizationIdOpnInt = authorizationIdOpn[0] + 256U*authorizationIdOpn[1] + 65536U*authorizationIdOpn[2] + 16777216U*authorizationIdOpn[3];
            response.print("\nAuthorizationId (UINT32_T): ");
            response.print(authorizationIdOpnInt);
        }
    }

    response.print("\n\n------------ GPIO ------------\n");
    response.print("\nRetain Input GPIO MQTT state: ");
    response.print(_preferences->getBool(preference_retain_gpio, false) ? "Yes\n" : "No\n");
    String gpioStr = "";
    _gpio->getConfigurationText(gpioStr, _gpio->pinConfiguration());
    response.print(gpioStr);
    response.print("</pre></body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::processUnpair(PsychicRequest *request, PsychicResponse* resp, bool opener)
{
    String value = "";
    if(request->hasParam("CONFIRMTOKEN"))
    {
        const PsychicWebParameter* p = request->getParam("CONFIRMTOKEN");
        if(p->value() != "")
        {
            value = p->value();
        }
    }

    if(value != _confirmCode)
    {
        return buildConfirmHtml(request, resp, "Confirm code is invalid.", 3, true);
    }

    esp_err_t res = buildConfirmHtml(request, resp, opener ? "Unpairing Nuki Opener." : "Unpairing Nuki Lock.", 3, true);

    if(!opener && _nuki != nullptr)
    {
        _nuki->unpair();
        _preferences->putInt(preference_lock_pin_status, (int)NukiPinState::NotConfigured);
    }
    if(opener && _nukiOpener != nullptr)
    {
        _nukiOpener->unpair();
        _preferences->putInt(preference_opener_pin_status, (int)NukiPinState::NotConfigured);
    }

    _network->disableHASS();
    waitAndProcess(false, 1000);
    _network->setRestartServices(true);
    return res;
}

esp_err_t WebCfgServer::processUpdate(PsychicRequest *request, PsychicResponse* resp)
{
    esp_err_t res;
    String value = "";
    if(request->hasParam("CONFIRMTOKEN"))
    {
        const PsychicWebParameter* p = request->getParam("CONFIRMTOKEN");
        if(p->value() != "")
        {
            value = p->value();
        }
    }

    if(value != _confirmCode)
    {
        return buildConfirmHtml(request, resp, "Confirm code is invalid.", 3, true);
    }

    if(request->hasParam("beta"))
    {
        /*
        if(request->hasParam("debug"))
        {
            res = buildConfirmHtml(request, resp, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest DEBUG BETA version", 2, true);
            _preferences->putString(preference_ota_updater_url, GITHUB_BETA_UPDATER_BINARY_URL_DBG);
            _preferences->putString(preference_ota_main_url, GITHUB_BETA_RELEASE_BINARY_URL_DBG);
        }
        else
        {
        */
        res = buildConfirmHtml(request, resp, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest BETA version", 2, true);
        _preferences->putString(preference_ota_updater_url, GITHUB_BETA_UPDATER_BINARY_URL);
        _preferences->putString(preference_ota_main_url, GITHUB_BETA_RELEASE_BINARY_URL);
        //}
    }
    else if(request->hasParam("master"))
    {
        /*
        if(request->hasParam("debug"))
        {
            res = buildConfirmHtml(request, resp, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest DEBUG DEVELOPMENT version", 2, true);
            _preferences->putString(preference_ota_updater_url, GITHUB_MASTER_UPDATER_BINARY_URL_DBG);
            _preferences->putString(preference_ota_main_url, GITHUB_MASTER_RELEASE_BINARY_URL_DBG);
        }
        else
        {
        */
        res = buildConfirmHtml(request, resp, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest DEVELOPMENT version", 2, true);
        _preferences->putString(preference_ota_updater_url, GITHUB_MASTER_UPDATER_BINARY_URL);
        _preferences->putString(preference_ota_main_url, GITHUB_MASTER_RELEASE_BINARY_URL);
        //}
    }
#if (defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32)) && !defined(CONFIG_FREERTOS_UNICORE)
    else if(request->hasParam("other"))
    {
        res = buildConfirmHtml(request, resp, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest RELEASE version", 2, true);
        _preferences->putString(preference_ota_updater_url, GITHUB_LATEST_UPDATER_BINARY_URL_OTHER);
        _preferences->putString(preference_ota_main_url, GITHUB_LATEST_RELEASE_BINARY_URL_OTHER);
    }
#endif
    else
    {
        /*
        if(request->hasParam("debug"))
        {
            res = buildConfirmHtml(request, resp, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest DEBUG RELEASE version", 2, true);
            _preferences->putString(preference_ota_updater_url, GITHUB_LATEST_UPDATER_BINARY_URL_DBG);
            _preferences->putString(preference_ota_main_url, GITHUB_LATEST_UPDATER_BINARY_URL_DBG);
        }
        else
        {
        */
        res = buildConfirmHtml(request, resp, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest RELEASE version", 2, true);
        _preferences->putString(preference_ota_updater_url, GITHUB_LATEST_UPDATER_BINARY_URL);
        _preferences->putString(preference_ota_main_url, GITHUB_LATEST_RELEASE_BINARY_URL);
        //}
    }
    waitAndProcess(true, 1000);
    restartEsp(RestartReason::OTAReboot);
    return res;
}

esp_err_t WebCfgServer::processFactoryReset(PsychicRequest *request, PsychicResponse* resp)
{
    esp_err_t res;
    String value = "";
    if(request->hasParam("CONFIRMTOKEN"))
    {
        const PsychicWebParameter* p = request->getParam("CONFIRMTOKEN");
        if(p->value() != "")
        {
            value = p->value();
        }
    }

    bool resetWifi = false;
    if(value.length() == 0 || value != _confirmCode)
    {
        return buildConfirmHtml(request, resp, "Confirm code is invalid.", 3, true);
    }
    else
    {
        String value2 = "";
        if(request->hasParam("WIFI"))
        {
            const PsychicWebParameter* p = request->getParam("WIFI");
            if(p->value() != "")
            {
                value = p->value();
            }
        }

        if(value2 == "1")
        {
            resetWifi = true;
            res = buildConfirmHtml(request, resp, "Factory resetting Nuki Hub, unpairing Nuki Lock and Nuki Opener and resetting WiFi.", 3, true);
        }
        else
        {
            res = buildConfirmHtml(request, resp, "Factory resetting Nuki Hub, unpairing Nuki Lock and Nuki Opener.", 3, true);
        }
    }

    waitAndProcess(false, 2000);

    if(_nuki != nullptr)
    {
        _nuki->unpair();
    }
    if(_nukiOpener != nullptr)
    {
        _nukiOpener->unpair();
    }

    String ssid = _preferences->getString(preference_wifi_ssid, "");
    String pass = _preferences->getString(preference_wifi_pass, "");

    _network->disableHASS();
    _preferences->clear();

#ifndef CONFIG_IDF_TARGET_ESP32H2
    if(!resetWifi)
    {
        _preferences->putString(preference_wifi_ssid, ssid);
        _preferences->putString(preference_wifi_pass, pass);
    }
#endif

    waitAndProcess(false, 3000);
    restartEsp(RestartReason::NukiHubReset);
    return res;
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

const std::vector<std::pair<String, String>> WebCfgServer::getNetworkDetectionOptions() const
{
    std::vector<std::pair<String, String>> options;

    options.push_back(std::make_pair("1", "Wi-Fi"));
    options.push_back(std::make_pair("2", "Generic W5500"));
    options.push_back(std::make_pair("3", "M5Stack Atom POE (W5500)"));
    options.push_back(std::make_pair("10", "M5Stack Atom POE S3 (W5500)"));
    options.push_back(std::make_pair("4", "Olimex ESP32-POE / ESP-POE-ISO"));
    options.push_back(std::make_pair("5", "WT32-ETH01"));
    options.push_back(std::make_pair("6", "M5STACK PoESP32 Unit"));
    options.push_back(std::make_pair("7", "LilyGO T-ETH-POE"));
    options.push_back(std::make_pair("12", "LilyGO T-ETH ELite"));
    options.push_back(std::make_pair("8", "GL-S10"));
    options.push_back(std::make_pair("9", "ETH01-Evo"));
    options.push_back(std::make_pair("13", "Waveshare ESP32-S3-ETH / ESP32-S3-ETH-POE"));
    options.push_back(std::make_pair("14", "LilyGO T-ETH-Lite-ESP32S3"));
    options.push_back(std::make_pair("15", "Waveshare ESP32-P4-NANO"));
    options.push_back(std::make_pair("16", "Waveshare ESP32-P4-Module-DEV-KIT"));
    options.push_back(std::make_pair("17", "ESP32-P4-Function-EV-Board"));
    options.push_back(std::make_pair("11", "Custom LAN module"));

    return options;
}

const std::vector<std::pair<String, String>> WebCfgServer::getNetworkCustomPHYOptions() const
{
    std::vector<std::pair<String, String>> options;
    options.push_back(std::make_pair("0", "Disabled"));
    options.push_back(std::make_pair("1", "W5500"));
    options.push_back(std::make_pair("2", "DN9051"));
    options.push_back(std::make_pair("3", "KSZ8851SNL"));
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
    options.push_back(std::make_pair("4", "LAN8720"));
    options.push_back(std::make_pair("5", "RTL8201"));
    options.push_back(std::make_pair("6", "TLK110/IP101"));
    options.push_back(std::make_pair("7", "DP83848"));
    options.push_back(std::make_pair("8", "KSZ8041"));
    options.push_back(std::make_pair("9", "KSZ8081"));
#endif

    return options;
}
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
const std::vector<std::pair<String, String>> WebCfgServer::getNetworkCustomCLKOptions() const
{
    std::vector<std::pair<String, String>> options;
    options.push_back(std::make_pair("0", "GPIO0 IN"));
    options.push_back(std::make_pair("1", "GPIO0 OUT"));
    options.push_back(std::make_pair("2", "GPIO16 OUT"));
    options.push_back(std::make_pair("3", "GPIO17 OUT"));
    return options;
}
#endif

const std::vector<std::pair<String, String>> WebCfgServer::getGpioOptions() const
{
    std::vector<std::pair<String, String>> options;

    const auto& roles = _gpio->getAllRoles();

    for(const auto& role : roles)
    {
        options.push_back( std::make_pair(String((int)role), _gpio->getRoleDescription(role)));
    }

    return options;
}

const String WebCfgServer::getPreselectionForGpio(const uint8_t &pin) const
{
    const std::vector<PinEntry>& pinConfiguration = _gpio->pinConfiguration();

    for(const auto& entry : pinConfiguration)
    {
        if(pin == entry.pin)
        {
            return String((int8_t)entry.role);
        }
    }

    return String((int8_t)PinRole::Disabled);
}

#ifdef NUKI_HUB_HTTPS_SERVER
void WebCfgServer::createSSLCertificate()
{
    SSLCert* cert;
    cert = new SSLCert();
    int createCertResult = createSelfSignedCert(
                               *cert,
                               KEYSIZE_2048,
                               "CN=nukihub.local,O=NukiHub,C=DE",
                               "20250101000000",
                               "20350101000000"
                           );

    if (createCertResult == 0)
    {
        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
        }
        else
        {
            File file = SPIFFS.open("/http_ssl.crt", FILE_WRITE);
            if (!file)
            {
                Log->println("Failed to open /http_ssl.crt for writing");
            }
            else
            {
                if (!file.print(cert->getCertPEM()))
                {
                    Log->println("Failed to write /http_ssl.crt");
                }
                file.close();
            }

            File file2 = SPIFFS.open("/http_ssl.key", FILE_WRITE);
            if (!file2)
            {
                Log->println("Failed to open /http_ssl.key for writing");
            }
            else
            {
                if (!file2.print(cert->getKeyPEM()))
                {
                    Log->println("Failed to write /http_ssl.key");
                }
                file2.close();
            }
        }
    }
    else
    {
        Log->print("SSL Self sign failed: ");
        Log->println(createCertResult);
    }
}
#endif

#endif
