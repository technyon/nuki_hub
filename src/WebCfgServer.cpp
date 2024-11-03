#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include <esp_task_wdt.h>
#ifdef CONFIG_SOC_SPIRAM_SUPPORTED
#include <esp_psram.h>
#endif
#ifndef CONFIG_IDF_TARGET_ESP32H2
#include <esp_wifi.h>
#include <WiFi.h>
#endif
#include <Update.h>

extern const uint8_t x509_crt_imported_bundle_bin_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_imported_bundle_bin_end[]   asm("_binary_x509_crt_bundle_end");

#ifndef NUKI_HUB_UPDATER
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include "ArduinoJson.h"

WebCfgServer::WebCfgServer(NukiWrapper* nuki, NukiOpenerWrapper* nukiOpener, NukiNetwork* network, Gpio* gpio, Preferences* preferences, bool allowRestartToPortal, uint8_t partitionType, PsychicHttpServer* psychicServer)
    : _nuki(nuki),
      _nukiOpener(nukiOpener),
      _network(network),
      _gpio(gpio),
      _preferences(preferences),
      _allowRestartToPortal(allowRestartToPortal),
      _partitionType(partitionType),
      _psychicServer(psychicServer)
#else
WebCfgServer::WebCfgServer(NukiNetwork* network, Preferences* preferences, bool allowRestartToPortal, uint8_t partitionType, PsychicHttpServer* psychicServer)
    : _network(network),
      _preferences(preferences),
      _allowRestartToPortal(allowRestartToPortal),
      _partitionType(partitionType),
      _psychicServer(psychicServer)
#endif
{
    _hostname = _preferences->getString(preference_hostname, "");
    String str = _preferences->getString(preference_cred_user, "");

    if(str.length() > 0)
    {
        memset(&_credUser, 0, sizeof(_credUser));
        memset(&_credPassword, 0, sizeof(_credPassword));

        _hasCredentials = true;
        const char *user = str.c_str();
        memcpy(&_credUser, user, str.length());

        str = _preferences->getString(preference_cred_password, "");
        const char *pass = str.c_str();
        memcpy(&_credPassword, pass, str.length());
    }

    _confirmCode = generateConfirmCode();

#ifndef NUKI_HUB_UPDATER
    _pinsConfigured = true;

    if(_nuki != nullptr && !_nuki->isPinSet())
    {
        _pinsConfigured = false;
    }
    if(_nukiOpener != nullptr && !_nukiOpener->isPinSet())
    {
        _pinsConfigured = false;
    }

    _brokerConfigured = _preferences->getString(preference_mqtt_broker).length() > 0 && _preferences->getInt(preference_mqtt_broker_port) > 0;
#endif
}

void WebCfgServer::initialize()
{
    _psychicServer->on("/", HTTP_GET, [&](PsychicRequest *request)
    {
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
        {
            return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
        }
        if(!_network->isApOpen())
        {
#ifndef NUKI_HUB_UPDATER
            return buildHtml(request);
#else
            return buildOtaHtml(request);
#endif
        }
#ifndef CONFIG_IDF_TARGET_ESP32H2
        else
        {
            return buildWifiConnectHtml(request);
        }
#endif
    });

    _psychicServer->on("/style.css", HTTP_GET, [&](PsychicRequest *request)
    {
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
        {
            return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
        }
        return sendCss(request);
    });
    _psychicServer->on("/favicon.ico", HTTP_GET, [&](PsychicRequest *request)
    {
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
        {
            return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
        }
        return sendFavicon(request);
    });
    _psychicServer->on("/reboot", HTTP_GET, [&](PsychicRequest *request)
    {
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
        {
            return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
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
            return buildConfirmHtml(request, "No confirm code set.", 3, true);
        }

        if(value != _confirmCode)
        {
            return request->redirect("/");
        }
        esp_err_t res = buildConfirmHtml(request, "Rebooting...", 2, true);
        waitAndProcess(true, 1000);
        restartEsp(RestartReason::RequestedViaWebServer);
        return res;
    });

    if(_network->isApOpen())
    {
#ifndef CONFIG_IDF_TARGET_ESP32H2
        _psychicServer->on("/ssidlist", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return buildSSIDListHtml(request);
        });
        _psychicServer->on("/savewifi", HTTP_POST, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
                {
                    return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
                }
            String message = "";
            bool connected = processWiFi(request, message);
            esp_err_t res = buildConfirmHtml(request, message, 10, true);

            if(connected)
            {
                waitAndProcess(true, 3000);
                restartEsp(RestartReason::ReconfigureWifi);
                //abort();
            }
            return res;
        });
#endif
    }
    else
    {
#ifndef NUKI_HUB_UPDATER
        _psychicServer->on("/import", HTTP_POST, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            String message = "";
            bool restart = processImport(request, message);
            return buildConfirmHtml(request, message, 3, true);
        });
        _psychicServer->on("/export", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return sendSettings(request);
        });
        _psychicServer->on("/impexpcfg", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return buildImportExportHtml(request);
        });
        _psychicServer->on("/status", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return buildStatusHtml(request);
        });
        _psychicServer->on("/acclvl", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return buildAccLvlHtml(request);
        });
        _psychicServer->on("/custntw", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return buildCustomNetworkConfigHtml(request);
        });
        _psychicServer->on("/advanced", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return buildAdvancedConfigHtml(request);
        });
        _psychicServer->on("/cred", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return buildCredHtml(request);
        });
        _psychicServer->on("/ntwconfig", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return buildNetworkConfigHtml(request);
        });
        _psychicServer->on("/mqttconfig", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return buildMqttConfigHtml(request);
        });
        _psychicServer->on("/nukicfg", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return buildNukiConfigHtml(request);
        });
        _psychicServer->on("/gpiocfg", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return buildGpioConfigHtml(request);
        });
#ifndef CONFIG_IDF_TARGET_ESP32H2
        _psychicServer->on("/wifi", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return buildConfigureWifiHtml(request);
        });
        _psychicServer->on("/wifimanager", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            if(_allowRestartToPortal)
            {
                esp_err_t res = buildConfirmHtml(request, "Restarting. Connect to ESP access point (\"NukiHub\" with password \"NukiHubESP32\") to reconfigure Wi-Fi.", 0);
                waitAndProcess(false, 1000);
                _network->reconfigureDevice();
                return res;
            }
            return(ESP_OK);
        });
#endif
        _psychicServer->on("/unpairlock", HTTP_POST, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return processUnpair(request, false);
        });
        _psychicServer->on("/unpairopener", HTTP_POST, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return processUnpair(request, true);
        });
        _psychicServer->on("/factoryreset", HTTP_POST, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return processFactoryReset(request);
        });
        _psychicServer->on("/info", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return buildInfoHtml(request);
        });
        _psychicServer->on("/debugon", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            _preferences->putBool(preference_publish_debug_info, true);
            return buildConfirmHtml(request, "Debug On", 3, true);
        });
        _psychicServer->on("/debugoff", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            _preferences->putBool(preference_publish_debug_info, false);
            return buildConfirmHtml(request, "Debug Off", 3, true);
        });
        _psychicServer->on("/savecfg", HTTP_POST, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            String message = "";
            bool restart = processArgs(request, message);
            return buildConfirmHtml(request, message, 3, true);
        });
        _psychicServer->on("/savegpiocfg", HTTP_POST, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            processGpioArgs(request);
            esp_err_t res = buildConfirmHtml(request, "Saving GPIO configuration. Restarting.", 3, true);
            Log->println(F("Restarting"));
            waitAndProcess(true, 1000);
            restartEsp(RestartReason::GpioConfigurationUpdated);
            return res;
        });
#endif
        _psychicServer->on("/ota", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return buildOtaHtml(request);
        });
        _psychicServer->on("/otadebug", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return buildOtaHtml(request, true);
        });
        _psychicServer->on("/reboottoota", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
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
                return buildConfirmHtml(request, "No confirm code set.", 3, true);
            }

            if(value != _confirmCode)
            {
                return request->redirect("/");
            }
            esp_err_t res = buildConfirmHtml(request, "Rebooting to other partition...", 2, true);
            waitAndProcess(true, 1000);
            esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL));
            restartEsp(RestartReason::OTAReboot);
            return res;
        });
        _psychicServer->on("/autoupdate", HTTP_GET, [&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
#ifndef NUKI_HUB_UPDATER
            return processUpdate(request);
#else
            return request->redirect("/");
#endif
        });

        PsychicUploadHandler *updateHandler = new PsychicUploadHandler();
        updateHandler->onUpload([&](PsychicRequest *request, const String& filename, uint64_t index, uint8_t *data, size_t len, bool final)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }
            return handleOtaUpload(request, filename, index, data, len, final);
        }
                               );

        updateHandler->onRequest([&](PsychicRequest *request)
        {
            if(strlen(_credUser) > 0 && strlen(_credPassword) > 0 && !request->authenticate(_credUser, _credPassword))
            {
                return request->requestAuthentication(BASIC_AUTH, "Nuki Hub", "You must log in.");
            }

            String result;
            if (!Update.hasError())
            {
                Log->print("Update code or data OK Update.errorString() ");
                Log->println(Update.errorString());
                result = "<b style='color:green'>Update OK.</b>";
                esp_err_t res = request->reply(200,"text/html",result.c_str());
                restartEsp(RestartReason::OTACompleted);
                return res;
            }
            else
            {
                result = " Update.errorString() " + String(Update.errorString());
                Log->print("ERROR : error ");
                Log->println(result.c_str());
                esp_err_t res = request->reply(500, "text/html", result.c_str());
                restartEsp(RestartReason::OTAAborted);
                return res;
            }
        });

        _psychicServer->on("/uploadota", HTTP_POST, updateHandler);
        //Update.onProgress(printProgress);
    }
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
esp_err_t WebCfgServer::buildSSIDListHtml(PsychicRequest *request)
{
    _network->scan(true, false);
    createSsidList();

    PsychicStreamResponse response(request, "text/plain");
    response.beginSend();

    for (int i = 0; i < _ssidList.size(); i++)
    {
        response.print("<tr class=\"trssid\" onclick=\"document.getElementById('inputssid').value = '" + _ssidList[i] + "';\"><td colspan=\"2\">" + _ssidList[i] + String(F(" (")) + String(_rssiList[i]) + String(F(" %)")) + "</td></tr>");
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

esp_err_t WebCfgServer::buildWifiConnectHtml(PsychicRequest *request)
{
    String header = "<style>.trssid:hover { cursor: pointer; color: blue; }</style><script>let intervalId; window.onload = function() { intervalId = setInterval(updateSSID, 3000); }; function updateSSID() { var request = new XMLHttpRequest(); request.open('GET', '/ssidlist', true); request.onload = () => { if (document.getElementById(\"aplist\") !== null) { document.getElementById(\"aplist\").innerHTML = request.responseText; } }; request.send(); }</script>";
    PsychicStreamResponse response(request, "text/plain");
    response.beginSend();
    buildHtmlHeader(&response, header);
    response.print("<h3>Available WiFi networks</h3>");
    response.print("<table id=\"aplist\">");
    createSsidList();
    for (int i = 0; i < _ssidList.size(); i++)
    {
        response.print("<tr class=\"trssid\" onclick=\"document.getElementById('inputssid').value = '" + _ssidList[i] + "';\"><td colspan=\"2\">" + _ssidList[i] + String(F(" (")) + String(_rssiList[i]) + String(F(" %)")) + "</td></tr>");
    }
    response.print("</table>");
    response.print("<form class=\"adapt\" method=\"post\" action=\"savewifi\">");
    response.print("<h3>WiFi credentials</h3>");
    response.print("<table>");
    printInputField(&response, "WIFISSID", "SSID", "", 32, "id=\"inputssid\"", false, true);
    printInputField(&response, "WIFIPASS", "Secret key", "", 63, "id=\"inputpass\"", false, true);
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

bool WebCfgServer::processWiFi(PsychicRequest *request, String& message)
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
            delay(100);
            loop++;
        }

        if (!_network->isConnected())
        {
            message = "Failed to connect to the given SSID with the given secret key, credentials not saved<br/>";
            return res;
        }
        else
        {
            if(_network->isConnected())
            {
                message = "Connection successful. Rebooting Nuki Hub.<br/>";
                _preferences->putString(preference_wifi_ssid, ssid);
                _preferences->putString(preference_wifi_pass, pass);
                res = true;
            }
            else
            {
                message = "Failed to connect to the given SSID, no IP received, credentials not saved<br/>";
                return res;
            }
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

esp_err_t WebCfgServer::buildOtaHtml(PsychicRequest *request, bool debug)
{
    PsychicStreamResponse response(request, "text/plain");
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
    response.print("<form onsubmit=\"if(document.getElementById('currentver').innerHTML == document.getElementById('latestver').innerHTML && '" + release_type + "' == '" + build_type + "') { alert('You are already on this version, build and build type'); return false; } else { return confirm('Do you really want to update to the latest release?'); } \" action=\"/autoupdate\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"release\" value=\"1\" /><input type=\"hidden\" name=\"" + release_type + "\" value=\"1\" /><input type=\"hidden\" name=\"token\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"background: green\" value=\"Update to latest release\"></form>");
    response.print("<form onsubmit=\"if(document.getElementById('currentver').innerHTML == document.getElementById('betaver').innerHTML && '" + release_type + "' == '" + build_type + "') { alert('You are already on this version, build and build type'); return false; } else { return confirm('Do you really want to update to the latest beta? This version could contain breaking bugs and necessitate downgrading to the latest release version using USB/Serial'); }\" action=\"/autoupdate\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"beta\" value=\"1\" /><input type=\"hidden\" name=\"" + release_type + "\" value=\"1\" /><input type=\"hidden\" name=\"token\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"color: black; background: yellow\"  value=\"Update to latest beta\"></form>");
    response.print("<form onsubmit=\"if(document.getElementById('currentver').innerHTML == document.getElementById('devver').innerHTML && '" + release_type + "' == '" + build_type + "') { alert('You are already on this version, build and build type'); return false; } else { return confirm('Do you really want to update to the latest development version? This version could contain breaking bugs and necessitate downgrading to the latest release version using USB/Serial'); }\" action=\"/autoupdate\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"master\" value=\"1\" /><input type=\"hidden\" name=\"" + release_type + "\" value=\"1\" /><input type=\"hidden\" name=\"token\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"background: red\"  value=\"Update to latest development version\"></form>");
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

        if(atof(doc["release"]["version"]) >= atof(currentVersion.c_str()))
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
        response.print("<form action=\"/reboottoota\" method=\"get\"><br>");
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
        response.print("<form action=\"/reboottoota\" method=\"get\"><br>");
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

esp_err_t WebCfgServer::buildOtaCompletedHtml(PsychicRequest *request)
{
    PsychicStreamResponse response(request, "text/plain");
    response.beginSend();
    buildHtmlHeader(&response);

    response.print("<div>Over-the-air update completed.<br>You will be forwarded automatically.</div>");
    response.print("<script type=\"text/javascript\">");
    response.print("window.addEventListener('load', function () {");
    response.print("   setTimeout(\"location.href = '/';\",10000);");
    response.print("});");
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
            vTaskDelay( 50 / portTICK_PERIOD_MS);
        }
    }
}

void WebCfgServer::printProgress(size_t prg, size_t sz)
{
    Log->printf("Progress: %d%%\n", (prg*100)/_otaContentLen);
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

            _otaStartTs = espMillis();
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
        Log->print(F("Progress: 100%"));
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

esp_err_t WebCfgServer::buildConfirmHtml(PsychicRequest *request, const String &message, uint32_t redirectDelay, bool redirect)
{
    PsychicStreamResponse response(request, "text/plain");
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
        header = "<script type=\"text/JavaScript\">function Redirect() { window.location.href = \"/\"; } setTimeout(function() { Redirect(); }, " + delay + "); </script>";
    }
    buildHtmlHeader(&response, header);
    response.print(message);
    response.print("</body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::sendCss(PsychicRequest *request)
{
    // escaped by https://www.cescaper.com/
    PsychicResponse response(request);
    response.addHeader("Cache-Control", "public, max-age=3600");
    response.setCode(200);
    response.setContentType("text/css");
    response.setContent(stylecss);
    return response.send();
}

esp_err_t WebCfgServer::sendFavicon(PsychicRequest *request)
{
    PsychicResponse response(request);
    response.addHeader("Cache-Control", "public, max-age=604800");
    response.setCode(200);
    response.setContentType("image/png");
    response.setContent((const char*)favicon_32x32);
    return response.send();
}

String WebCfgServer::generateConfirmCode()
{
    int code = random(1000,9999);
    return String(code);
}

void WebCfgServer::printInputField(PsychicStreamResponse *response,
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
esp_err_t WebCfgServer::sendSettings(PsychicRequest *request)
{
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

    JsonDocument json;
    String jsonPretty;

    DebugPreferences debugPreferences;

    const std::vector<char*> keysPrefs = debugPreferences.getPreferencesKeys();
    const std::vector<char*> boolPrefs = debugPreferences.getPreferencesBoolKeys();
    const std::vector<char*> redactedPrefs = debugPreferences.getPreferencesRedactedKeys();
    const std::vector<char*> bytePrefs = debugPreferences.getPreferencesByteKeys();

    for(const auto& key : keysPrefs)
    {
        if(strcmp(key, preference_show_secrets) == 0)
        {
            continue;
        }
        if(strcmp(key, preference_latest_version) == 0)
        {
            continue;
        }
        if(strcmp(key, preference_device_id_lock) == 0)
        {
            continue;
        }
        if(strcmp(key, preference_device_id_opener) == 0)
        {
            continue;
        }
        if(!redacted) if(std::find(redactedPrefs.begin(), redactedPrefs.end(), key) != redactedPrefs.end())
            {
                continue;
            }
        if(!_preferences->isKey(key))
        {
            json[key] = "";
        }
        else if(std::find(boolPrefs.begin(), boolPrefs.end(), key) != boolPrefs.end())
        {
            json[key] = _preferences->getBool(key) ? "1" : "0";
        }
        else
        {
            switch(_preferences->getType(key))
            {
            case PT_I8:
                json[key] = String(_preferences->getChar(key));
                break;
            case PT_I16:
                json[key] = String(_preferences->getShort(key));
                break;
            case PT_I32:
                json[key] = String(_preferences->getInt(key));
                break;
            case PT_I64:
                json[key] = String(_preferences->getLong64(key));
                break;
            case PT_U8:
                json[key] = String(_preferences->getUChar(key));
                break;
            case PT_U16:
                json[key] = String(_preferences->getUShort(key));
                break;
            case PT_U32:
                json[key] = String(_preferences->getUInt(key));
                break;
            case PT_U64:
                json[key] = String(_preferences->getULong64(key));
                break;
            case PT_STR:
                json[key] = _preferences->getString(key);
                break;
            default:
                json[key] = _preferences->getString(key);
                break;
            }
        }
    }

    if(pairing)
    {
        if(_nuki != nullptr)
        {
            unsigned char currentBleAddress[6];
            unsigned char authorizationId[4] = {0x00};
            unsigned char secretKeyK[32] = {0x00};
            uint16_t storedPincode = 0000;
            Preferences nukiBlePref;
            nukiBlePref.begin("NukiHub", false);
            nukiBlePref.getBytes("bleAddress", currentBleAddress, 6);
            nukiBlePref.getBytes("secretKeyK", secretKeyK, 32);
            nukiBlePref.getBytes("authorizationId", authorizationId, 4);
            nukiBlePref.getBytes("securityPinCode", &storedPincode, 2);
            nukiBlePref.end();
            char text[255];
            text[0] = '\0';
            for(int i = 0 ; i < 6 ; i++)
            {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", currentBleAddress[i]);
            }
            json["bleAddressLock"] = text;
            memset(text, 0, sizeof(text));
            text[0] = '\0';
            for(int i = 0 ; i < 32 ; i++)
            {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", secretKeyK[i]);
            }
            json["secretKeyKLock"] = text;
            memset(text, 0, sizeof(text));
            text[0] = '\0';
            for(int i = 0 ; i < 4 ; i++)
            {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", authorizationId[i]);
            }
            json["authorizationIdLock"] = text;
            memset(text, 0, sizeof(text));
            json["securityPinCodeLock"] = storedPincode;
        }
        if(_nukiOpener != nullptr)
        {
            unsigned char currentBleAddressOpn[6];
            unsigned char authorizationIdOpn[4] = {0x00};
            unsigned char secretKeyKOpn[32] = {0x00};
            uint16_t storedPincodeOpn = 0000;
            Preferences nukiBlePref;
            nukiBlePref.begin("NukiHubopener", false);
            nukiBlePref.getBytes("bleAddress", currentBleAddressOpn, 6);
            nukiBlePref.getBytes("secretKeyK", secretKeyKOpn, 32);
            nukiBlePref.getBytes("authorizationId", authorizationIdOpn, 4);
            nukiBlePref.getBytes("securityPinCode", &storedPincodeOpn, 2);
            nukiBlePref.end();
            char text[255];
            text[0] = '\0';
            for(int i = 0 ; i < 6 ; i++)
            {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", currentBleAddressOpn[i]);
            }
            json["bleAddressOpener"] = text;
            memset(text, 0, sizeof(text));
            text[0] = '\0';
            for(int i = 0 ; i < 32 ; i++)
            {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", secretKeyKOpn[i]);
            }
            json["secretKeyKOpener"] = text;
            memset(text, 0, sizeof(text));
            text[0] = '\0';
            for(int i = 0 ; i < 4 ; i++)
            {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", authorizationIdOpn[i]);
            }
            json["authorizationIdOpener"] = text;
            memset(text, 0, sizeof(text));
            json["securityPinCodeOpener"] = storedPincodeOpn;
        }
    }

    for(const auto& key : bytePrefs)
    {
        size_t storedLength = _preferences->getBytesLength(key);
        if(storedLength == 0)
        {
            continue;
        }
        uint8_t serialized[storedLength];
        memset(serialized, 0, sizeof(serialized));
        size_t size = _preferences->getBytes(key, serialized, sizeof(serialized));
        if(size == 0)
        {
            continue;
        }
        char text[255];
        text[0] = '\0';
        for(int i = 0 ; i < size ; i++)
        {
            size_t offset = strlen(text);
            sprintf(&(text[offset]), "%02x", serialized[i]);
        }
        json[key] = text;
        memset(text, 0, sizeof(text));
    }

    serializeJsonPretty(json, jsonPretty);

    return request->reply(200, "application/json", jsonPretty.c_str());
}

bool WebCfgServer::processArgs(PsychicRequest *request, String& message)
{
    bool configChanged = false;
    bool aclLvlChanged = false;
    bool clearMqttCredentials = false;
    bool clearCredentials = false;
    bool manPairLck = false;
    bool manPairOpn = false;
    bool networkReconfigure = false;
    unsigned char currentBleAddress[6];
    unsigned char authorizationId[4] = {0x00};
    unsigned char secretKeyK[32] = {0x00};
    unsigned char pincode[2] = {0x00};
    unsigned char currentBleAddressOpn[6];
    unsigned char authorizationIdOpn[4] = {0x00};
    unsigned char secretKeyKOpn[32] = {0x00};

    uint32_t aclPrefs[17] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t basicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t basicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t advancedLockConfigAclPrefs[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t advancedOpenerConfigAclPrefs[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

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
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "MQTTPORT")
        {
            if(_preferences->getInt(preference_mqtt_broker_port, 0) !=  value.toInt())
            {
                _preferences->putInt(preference_mqtt_broker_port,  value.toInt());
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
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
                    Log->print(F("Setting changed: "));
                    Log->println(key);
                    configChanged = true;
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
                    Log->print(F("Setting changed: "));
                    Log->println(key);
                    configChanged = true;
                }
            }
        }
        else if(key == "MQTTPATH")
        {
            if(_preferences->getString(preference_mqtt_lock_path, "") != value)
            {
                _preferences->putString(preference_mqtt_lock_path, value);
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "MQTTCA")
        {
            if(_preferences->getString(preference_mqtt_ca, "") != value)
            {
                _preferences->putString(preference_mqtt_ca, value);
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "MQTTCRT")
        {
            if(_preferences->getString(preference_mqtt_crt, "") != value)
            {
                _preferences->putString(preference_mqtt_crt, value);
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "MQTTKEY")
        {
            if(_preferences->getString(preference_mqtt_key, "") != value)
            {
                _preferences->putString(preference_mqtt_key, value);
                Log->print(F("Setting changed: "));
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
                Log->print(F("Setting changed: "));
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
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "NWCUSTADDR")
        {
            if(_preferences->getInt(preference_network_custom_addr, 0) != value.toInt())
            {
                networkReconfigure = true;
                _preferences->putInt(preference_network_custom_addr, value.toInt());
                Log->print(F("Setting changed: "));
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
                Log->print(F("Setting changed: "));
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
                Log->print(F("Setting changed: "));
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
                Log->print(F("Setting changed: "));
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
                Log->print(F("Setting changed: "));
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
                Log->print(F("Setting changed: "));
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
                Log->print(F("Setting changed: "));
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
                Log->print(F("Setting changed: "));
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
                Log->print(F("Setting changed: "));
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
                Log->print(F("Setting changed: "));
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
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "NWHWWIFIFB")
        {
            if(_preferences->getBool(preference_network_wifi_fallback_disabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_network_wifi_fallback_disabled, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "RSSI")
        {
            if(_preferences->getInt(preference_rssi_publish_interval, 60) != value.toInt())
            {
                _preferences->putInt(preference_rssi_publish_interval, value.toInt());
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "HASSDISCOVERY")
        {
            if(_preferences->getString(preference_mqtt_hass_discovery, "") != value)
            {
                if (_nuki != nullptr)
                {
                    _nuki->disableHASS();
                }
                if (_nukiOpener != nullptr)
                {
                    _nukiOpener->disableHASS();
                }
                _preferences->putString(preference_mqtt_hass_discovery, value);
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "OPENERCONT")
        {
            if(_preferences->getBool(preference_opener_continuous_mode, false) != (value == "1"))
            {
                _preferences->putBool(preference_opener_continuous_mode, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "HASSCUURL")
        {
            if(_preferences->getString(preference_mqtt_hass_cu_url, "") != value)
            {
                _preferences->putString(preference_mqtt_hass_cu_url, value);
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "HOSTNAME")
        {
            if(_preferences->getString(preference_hostname, "") != value)
            {
                _preferences->putString(preference_hostname, value);
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "NETTIMEOUT")
        {
            if(_preferences->getInt(preference_network_timeout, 60) != value.toInt())
            {
                _preferences->putInt(preference_network_timeout, value.toInt());
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "RSTDISC")
        {
            if(_preferences->getBool(preference_restart_on_disconnect, false) != (value == "1"))
            {
                _preferences->putBool(preference_restart_on_disconnect, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "MQTTLOG")
        {
            if(_preferences->getBool(preference_mqtt_log_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_mqtt_log_enabled, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "WEBLOG")
        {
            if(_preferences->getBool(preference_webserial_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_webserial_enabled, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "CHECKUPDATE")
        {
            if(_preferences->getBool(preference_check_updates, false) != (value == "1"))
            {
                _preferences->putBool(preference_check_updates, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "UPDATEMQTT")
        {
            if(_preferences->getBool(preference_update_from_mqtt, false) != (value == "1"))
            {
                _preferences->putBool(preference_update_from_mqtt, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
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
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
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
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "HYBRIDTIMER")
        {
            if(_preferences->getInt(preference_query_interval_hybrid_lockstate, 600) != value.toInt())
            {
                _preferences->putInt(preference_query_interval_hybrid_lockstate, value.toInt());
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "HYBRIDRETRY")
        {
            if(_preferences->getBool(preference_official_hybrid_retry, false) != (value == "1"))
            {
                _preferences->putBool(preference_official_hybrid_retry, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "DISNONJSON")
        {
            if(_preferences->getBool(preference_disable_non_json, false) != (value == "1"))
            {
                _preferences->putBool(preference_disable_non_json, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "DHCPENA")
        {
            if(_preferences->getBool(preference_ip_dhcp_enabled, true) != (value == "1"))
            {
                _preferences->putBool(preference_ip_dhcp_enabled, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "IPADDR")
        {
            if(_preferences->getString(preference_ip_address, "") != value)
            {
                _preferences->putString(preference_ip_address, value);
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "IPSUB")
        {
            if(_preferences->getString(preference_ip_subnet, "") != value)
            {
                _preferences->putString(preference_ip_subnet, value);
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "IPGTW")
        {
            if(_preferences->getString(preference_ip_gateway, "") != value)
            {
                _preferences->putString(preference_ip_gateway, value);
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "DNSSRV")
        {
            if(_preferences->getString(preference_ip_dns_server, "") != value)
            {
                _preferences->putString(preference_ip_dns_server, value);
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "LSTINT")
        {
            if(_preferences->getInt(preference_query_interval_lockstate, 1800) != value.toInt())
            {
                _preferences->putInt(preference_query_interval_lockstate, value.toInt());
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "CFGINT")
        {
            if(_preferences->getInt(preference_query_interval_configuration, 3600) != value.toInt())
            {
                _preferences->putInt(preference_query_interval_configuration, value.toInt());
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "BATINT")
        {
            if(_preferences->getInt(preference_query_interval_battery, 1800) != value.toInt())
            {
                _preferences->putInt(preference_query_interval_battery, value.toInt());
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "KPINT")
        {
            if(_preferences->getInt(preference_query_interval_keypad, 1800) != value.toInt())
            {
                _preferences->putInt(preference_query_interval_keypad, value.toInt());
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "NRTRY")
        {
            if(_preferences->getInt(preference_command_nr_of_retries, 3) != value.toInt())
            {
                _preferences->putInt(preference_command_nr_of_retries, value.toInt());
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "TRYDLY")
        {
            if(_preferences->getInt(preference_command_retry_delay, 100) != value.toInt())
            {
                _preferences->putInt(preference_command_retry_delay, value.toInt());
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "TXPWR")
        {
            if(value.toInt() >= -12 && value.toInt() <= 9)
            {
                if(_preferences->getInt(preference_ble_tx_power, 9) != value.toInt())
                {
                    _preferences->putInt(preference_ble_tx_power, value.toInt());
                    Log->print(F("Setting changed: "));
                    Log->println(key);
                    //configChanged = true;
                }
            }
        }
        else if(key == "RSBC")
        {
            if(_preferences->getInt(preference_restart_ble_beacon_lost, 60) != value.toInt())
            {
                _preferences->putInt(preference_restart_ble_beacon_lost, value.toInt());
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "TSKNTWK")
        {
            if(value.toInt() > 12287 && value.toInt() < 32769)
            {
                if(_preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE) != value.toInt())
                {
                    _preferences->putInt(preference_task_size_network, value.toInt());
                    Log->print(F("Setting changed: "));
                    Log->println(key);
                    configChanged = true;
                }
            }
        }
        else if(key == "TSKNUKI")
        {
            if(value.toInt() > 8191 && value.toInt() < 32769)
            {
                if(_preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE) != value.toInt())
                {
                    _preferences->putInt(preference_task_size_nuki, value.toInt());
                    Log->print(F("Setting changed: "));
                    Log->println(key);
                    configChanged = true;
                }
            }
        }
        else if(key == "ALMAX")
        {
            if(value.toInt() > 0 && value.toInt() < 51)
            {
                if(_preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG) != value.toInt())
                {
                    _preferences->putInt(preference_authlog_max_entries, value.toInt());
                    Log->print(F("Setting changed: "));
                    Log->println(key);
                    //configChanged = true;
                }
            }
        }
        else if(key == "KPMAX")
        {
            if(value.toInt() > 0 && value.toInt() < 101)
            {
                if(_preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD) != value.toInt())
                {
                    _preferences->putInt(preference_keypad_max_entries, value.toInt());
                    Log->print(F("Setting changed: "));
                    Log->println(key);
                    //configChanged = true;
                }
            }
        }
        else if(key == "TCMAX")
        {
            if(value.toInt() > 0 && value.toInt() < 51)
            {
                if(_preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL) != value.toInt())
                {
                    _preferences->putInt(preference_timecontrol_max_entries, value.toInt());
                    Log->print(F("Setting changed: "));
                    Log->println(key);
                    //configChanged = true;
                }
            }
        }
        else if(key == "AUTHMAX")
        {
            if(value.toInt() > 0 && value.toInt() < 51)
            {
                if(_preferences->getInt(preference_auth_max_entries, MAX_AUTH) != value.toInt())
                {
                    _preferences->putInt(preference_auth_max_entries, value.toInt());
                    Log->print(F("Setting changed: "));
                    Log->println(key);
                    //configChanged = true;
                }
            }
        }
        else if(key == "BUFFSIZE")
        {
            if(value.toInt() > 4095 && value.toInt() < 32769)
            {
                if(_preferences->getInt(preference_buffer_size, CHAR_BUFFER_SIZE) != value.toInt())
                {
                    _preferences->putInt(preference_buffer_size, value.toInt());
                    Log->print(F("Setting changed: "));
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
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "DISNTWNOCON")
        {
            if(_preferences->getBool(preference_disable_network_not_connected, false) != (value == "1"))
            {
                _preferences->putBool(preference_disable_network_not_connected, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "OTAUPD")
        {
            if(_preferences->getString(preference_ota_updater_url, "") != value)
            {
                _preferences->putString(preference_ota_updater_url, value);
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "OTAMAIN")
        {
            if(_preferences->getString(preference_ota_main_url, "") != value)
            {
                _preferences->putString(preference_ota_main_url, value);
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "SHOWSECRETS")
        {
            if(_preferences->getBool(preference_show_secrets, false) != (value == "1"))
            {
                _preferences->putBool(preference_show_secrets, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
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
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "KPPUB")
        {
            if(_preferences->getBool(preference_keypad_info_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_keypad_info_enabled, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "KPCODE")
        {
            if(_preferences->getBool(preference_keypad_publish_code, false) != (value == "1"))
            {
                _preferences->putBool(preference_keypad_publish_code, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "KPCHECK")
        {
            if(_preferences->getBool(preference_keypad_check_code_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_keypad_check_code_enabled, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "KPENA")
        {
            if(_preferences->getBool(preference_keypad_control_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_keypad_control_enabled, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "TCPUB")
        {
            if(_preferences->getBool(preference_timecontrol_info_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_timecontrol_info_enabled, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "AUTHPUB")
        {
            if(_preferences->getBool(preference_auth_info_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_auth_info_enabled, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "KPPER")
        {
            if(_preferences->getBool(preference_keypad_topic_per_entry, false) != (value == "1"))
            {
                _preferences->putBool(preference_keypad_topic_per_entry, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "TCPER")
        {
            if(_preferences->getBool(preference_timecontrol_topic_per_entry, false) != (value == "1"))
            {
                _preferences->putBool(preference_timecontrol_topic_per_entry, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "TCENA")
        {
            if(_preferences->getBool(preference_timecontrol_control_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_timecontrol_control_enabled, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "AUTHPER")
        {
            if(_preferences->getBool(preference_auth_topic_per_entry, false) != (value == "1"))
            {
                _preferences->putBool(preference_auth_topic_per_entry, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "AUTHENA")
        {
            if(_preferences->getBool(preference_auth_control_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_auth_control_enabled, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "PUBAUTH")
        {
            if(_preferences->getBool(preference_publish_authdata, false) != (value == "1"))
            {
                _preferences->putBool(preference_publish_authdata, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
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
        else if(key == "REGAPP")
        {
            if(_preferences->getBool(preference_register_as_app, false) != (value == "1"))
            {
                _preferences->putBool(preference_register_as_app, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "REGAPPOPN")
        {
            if(_preferences->getBool(preference_register_opener_as_app, false) != (value == "1"))
            {
                _preferences->putBool(preference_register_opener_as_app, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                //configChanged = true;
            }
        }
        else if(key == "LOCKENA")
        {
            if(_preferences->getBool(preference_lock_enabled, true) != (value == "1"))
            {
                _preferences->putBool(preference_lock_enabled, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
        }
        else if(key == "OPENA")
        {
            if(_preferences->getBool(preference_opener_enabled, false) != (value == "1"))
            {
                _preferences->putBool(preference_opener_enabled, (value == "1"));
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
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
                    Log->print(F("Setting changed: "));
                    Log->println(key);
                    configChanged = true;
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
        else if(key == "NUKIPIN" && _nuki != nullptr)
        {
            if(value == "#")
            {
                message = "Nuki Lock PIN cleared";
                _nuki->setPin(0xffff);
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
            else
            {
                if(_nuki->getPin() != value.toInt())
                {
                    message = "Nuki Lock PIN saved";
                    _nuki->setPin(value.toInt());
                    Log->print(F("Setting changed: "));
                    Log->println(key);
                    configChanged = true;
                }
            }
        }
        else if(key == "NUKIOPPIN" && _nukiOpener != nullptr)
        {
            if(value == "#")
            {
                message = "Nuki Opener PIN cleared";
                _nukiOpener->setPin(0xffff);
                Log->print(F("Setting changed: "));
                Log->println(key);
                configChanged = true;
            }
            else
            {
                if(_nukiOpener->getPin() != value.toInt())
                {
                    message = "Nuki Opener PIN saved";
                    _nukiOpener->setPin(value.toInt());
                    Log->print(F("Setting changed: "));
                    Log->println(key);
                    configChanged = true;
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
        Log->println(F("Changing lock pairing"));
        Preferences nukiBlePref;
        nukiBlePref.begin("NukiHub", false);
        nukiBlePref.putBytes("bleAddress", currentBleAddress, 6);
        nukiBlePref.putBytes("secretKeyK", secretKeyK, 32);
        nukiBlePref.putBytes("authorizationId", authorizationId, 4);
        nukiBlePref.putBytes("securityPinCode", pincode, 2);
        nukiBlePref.end();
        Log->print(F("Setting changed: "));
        Log->println("Lock pairing data");
        configChanged = true;
    }

    if(manPairOpn)
    {
        Log->println(F("Changing opener pairing"));
        Preferences nukiBlePref;
        nukiBlePref.begin("NukiHubopener", false);
        nukiBlePref.putBytes("bleAddress", currentBleAddressOpn, 6);
        nukiBlePref.putBytes("secretKeyK", secretKeyKOpn, 32);
        nukiBlePref.putBytes("authorizationId", authorizationIdOpn, 4);
        nukiBlePref.putBytes("securityPinCode", pincode, 2);
        nukiBlePref.end();
        Log->print(F("Setting changed: "));
        Log->println("Opener pairing data");
        configChanged = true;
    }

    if(pass1 != "" && pass1 == pass2)
    {
        if(_preferences->getString(preference_cred_password, "") != pass1)
        {
            _preferences->putString(preference_cred_password, pass1);
            Log->print(F("Setting changed: "));
            Log->println("CREDPASS");
            configChanged = true;
        }
    }

    if(clearMqttCredentials)
    {
        if(_preferences->getString(preference_mqtt_user, "") != "")
        {
            _preferences->putString(preference_mqtt_user, "");
            Log->print(F("Setting changed: "));
            Log->println("MQTTUSER");
            configChanged = true;
        }
        if(_preferences->getString(preference_mqtt_password, "") != "")
        {
            _preferences->putString(preference_mqtt_password, "");
            Log->print(F("Setting changed: "));
            Log->println("MQTTPASS");
            configChanged = true;
        }
    }

    if(clearCredentials)
    {
        if(_preferences->getString(preference_cred_user, "") != "")
        {
            _preferences->putString(preference_cred_user, "");
            Log->print(F("Setting changed: "));
            Log->println("CREDUSER");
            configChanged = true;
        }
        if(_preferences->getString(preference_cred_password, "") != "")
        {
            _preferences->putString(preference_cred_password, "");
            Log->print(F("Setting changed: "));
            Log->println("CREDPASS");
            configChanged = true;
        }
    }

    if(aclLvlChanged)
    {
        uint32_t curAclPrefs[17] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        uint32_t curBasicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        uint32_t curAdvancedLockConfigAclPrefs[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        uint32_t curBasicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        uint32_t curAdvancedOpenerConfigAclPrefs[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
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
                Log->print(F("Setting changed: "));
                Log->println("ACLPREFS");
                //configChanged = true;
                break;
            }
        }
        for(int i=0; i < 16; i++)
        {
            if(curBasicLockConfigAclPrefs[i] != basicLockConfigAclPrefs[i])
            {
                _preferences->putBytes(preference_conf_lock_basic_acl, (byte*)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
                Log->print(F("Setting changed: "));
                Log->println("ACLCONFBASICLOCK");
                //configChanged = true;
                break;
            }
        }
        for(int i=0; i < 22; i++)
        {
            if(curAdvancedLockConfigAclPrefs[i] != advancedLockConfigAclPrefs[i])
            {
                _preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
                Log->print(F("Setting changed: "));
                Log->println("ACLCONFADVANCEDLOCK");
                //configChanged = true;
                break;

            }
        }
        for(int i=0; i < 14; i++)
        {
            if(curBasicOpenerConfigAclPrefs[i] != basicOpenerConfigAclPrefs[i])
            {
                _preferences->putBytes(preference_conf_opener_basic_acl, (byte*)(&basicOpenerConfigAclPrefs), sizeof(basicOpenerConfigAclPrefs));
                Log->print(F("Setting changed: "));
                Log->println("ACLCONFBASICOPENER");
                //configChanged = true;
                break;
            }
        }
        for(int i=0; i < 20; i++)
        {
            if(curAdvancedOpenerConfigAclPrefs[i] != advancedOpenerConfigAclPrefs[i])
            {
                _preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));
                Log->print(F("Setting changed: "));
                Log->println("ACLCONFADVANCEDOPENER");
                //configChanged = true;
                break;
            }
        }
    }

    if(configChanged)
    {
        message = "Configuration saved, reboot required to apply";
        _rebootRequired = true;
    }
    else
    {
        message = "Configuration saved.";
    }

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

bool WebCfgServer::processImport(PsychicRequest *request, String& message)
{
    bool configChanged = false;
    unsigned char currentBleAddress[6];
    unsigned char authorizationId[4] = {0x00};
    unsigned char secretKeyK[32] = {0x00};
    unsigned char currentBleAddressOpn[6];
    unsigned char authorizationIdOpn[4] = {0x00};
    unsigned char secretKeyKOpn[32] = {0x00};

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

            DebugPreferences debugPreferences;

            const std::vector<char*> keysPrefs = debugPreferences.getPreferencesKeys();
            const std::vector<char*> boolPrefs = debugPreferences.getPreferencesBoolKeys();
            const std::vector<char*> bytePrefs = debugPreferences.getPreferencesByteKeys();
            const std::vector<char*> intPrefs = debugPreferences.getPreferencesIntKeys();

            for(const auto& key : keysPrefs)
            {
                if(doc[key].isNull())
                {
                    continue;
                }
                if(strcmp(key, preference_show_secrets) == 0)
                {
                    continue;
                }
                if(strcmp(key, preference_latest_version) == 0)
                {
                    continue;
                }
                if(strcmp(key, preference_device_id_lock) == 0)
                {
                    continue;
                }
                if(strcmp(key, preference_device_id_opener) == 0)
                {
                    continue;
                }
                if(std::find(boolPrefs.begin(), boolPrefs.end(), key) != boolPrefs.end())
                {
                    if (doc[key].as<String>().length() > 0)
                    {
                        _preferences->putBool(key, (doc[key].as<String>() == "1" ? true : false));
                    }
                    else
                    {
                        _preferences->remove(key);
                    }
                    continue;
                }
                if(std::find(intPrefs.begin(), intPrefs.end(), key) != intPrefs.end())
                {
                    if (doc[key].as<String>().length() > 0)
                    {
                        _preferences->putInt(key, doc[key].as<int>());
                    }
                    else
                    {
                        _preferences->remove(key);
                    }
                    continue;
                }

                if (doc[key].as<String>().length() > 0)
                {
                    _preferences->putString(key, doc[key].as<String>());
                }
                else
                {
                    _preferences->remove(key);
                }
            }

            for(const auto& key : bytePrefs)
            {
                if(!doc[key].isNull() && doc[key].is<JsonVariant>())
                {
                    String value = doc[key].as<String>();
                    unsigned char tmpchar[32];
                    for(int i=0; i<value.length(); i+=2)
                    {
                        tmpchar[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                    }
                    _preferences->putBytes(key, (byte*)(&tmpchar), (value.length() / 2));
                    memset(tmpchar, 0, sizeof(tmpchar));
                }
            }

            Preferences nukiBlePref;
            nukiBlePref.begin("NukiHub", false);

            if(!doc["bleAddressLock"].isNull())
            {
                if (doc["bleAddressLock"].as<String>().length() == 12)
                {
                    String value = doc["bleAddressLock"].as<String>();
                    for(int i=0; i<value.length(); i+=2)
                    {
                        currentBleAddress[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                    }
                    nukiBlePref.putBytes("bleAddress", currentBleAddress, 6);
                }
            }
            if(!doc["secretKeyKLock"].isNull())
            {
                if (doc["secretKeyKLock"].as<String>().length() == 64)
                {
                    String value = doc["secretKeyKLock"].as<String>();
                    for(int i=0; i<value.length(); i+=2)
                    {
                        secretKeyK[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                    }
                    nukiBlePref.putBytes("secretKeyK", secretKeyK, 32);
                }
            }
            if(!doc["authorizationIdLock"].isNull())
            {
                if (doc["authorizationIdLock"].as<String>().length() == 8)
                {
                    String value = doc["authorizationIdLock"].as<String>();
                    for(int i=0; i<value.length(); i+=2)
                    {
                        authorizationId[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                    }
                    nukiBlePref.putBytes("authorizationId", authorizationId, 4);
                }
            }
            nukiBlePref.end();
            if(!doc["securityPinCodeLock"].isNull() && _nuki != nullptr)
            {
                if(doc["securityPinCodeLock"].as<String>().length() > 0)
                {
                    _nuki->setPin(doc["securityPinCodeLock"].as<int>());
                }
                else
                {
                    _nuki->setPin(0xffff);
                }
            }
            nukiBlePref.begin("NukiHubopener", false);
            if(!doc["bleAddressOpener"].isNull())
            {
                if (doc["bleAddressOpener"].as<String>().length() == 12)
                {
                    String value = doc["bleAddressOpener"].as<String>();
                    for(int i=0; i<value.length(); i+=2)
                    {
                        currentBleAddressOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                    }
                    nukiBlePref.putBytes("bleAddress", currentBleAddressOpn, 6);
                }
            }
            if(!doc["secretKeyKOpener"].isNull())
            {
                if (doc["secretKeyKOpener"].as<String>().length() == 64)
                {
                    String value = doc["secretKeyKOpener"].as<String>();
                    for(int i=0; i<value.length(); i+=2)
                    {
                        secretKeyKOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                    }
                    nukiBlePref.putBytes("secretKeyK", secretKeyKOpn, 32);
                }
            }
            if(!doc["authorizationIdOpener"].isNull())
            {
                if (doc["authorizationIdOpener"].as<String>().length() == 8)
                {
                    String value = doc["authorizationIdOpener"].as<String>();
                    for(int i=0; i<value.length(); i+=2)
                    {
                        authorizationIdOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                    }
                    nukiBlePref.putBytes("authorizationId", authorizationIdOpn, 4);
                }
            }
            nukiBlePref.end();
            if(!doc["securityPinCodeOpener"].isNull() && _nukiOpener != nullptr)
            {
                if(doc["securityPinCodeOpener"].as<String>().length() > 0)
                {
                    _nukiOpener->setPin(doc["securityPinCodeOpener"].as<int>());
                }
                else
                {
                    _nukiOpener->setPin(0xffff);
                }
            }

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

void WebCfgServer::processGpioArgs(PsychicRequest *request)
{
    int params = request->params();
    std::vector<PinEntry> pinConfiguration;

    for(int index = 0; index < params; index++)
    {
        const PsychicWebParameter* p = request->getParam(index);
        PinRole role = (PinRole)p->value().toInt();
        if(role != PinRole::Disabled)
        {
            PinEntry entry;
            entry.pin = p->name().toInt();
            entry.role = role;
            pinConfiguration.push_back(entry);
        }
    }

    _gpio->savePinConfiguration(pinConfiguration);
}

esp_err_t WebCfgServer::buildImportExportHtml(PsychicRequest *request)
{
    PsychicStreamResponse response(request, "text/plain");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<div id=\"upform\"><h4>Import configuration</h4>");
    response.print("<form method=\"post\" action=\"import\"><textarea id=\"importjson\" name=\"importjson\" rows=\"10\" cols=\"50\"></textarea><br/>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Import\"></form><br><br></div>");
    response.print("<div id=\"gitdiv\">");
    response.print("<h4>Export configuration</h4><br>");
    response.print("<button title=\"Basic export\" onclick=\" window.open('/export', '_self'); return false;\">Basic export</button>");
    response.print("<br><br><button title=\"Export with redacted settings\" onclick=\" window.open('/export?redacted=1'); return false;\">Export with redacted settings</button>");
    response.print("<br><br><button title=\"Export with redacted settings and pairing data\" onclick=\" window.open('/export?redacted=1&pairing=1'); return false;\">Export with redacted settings and pairing data</button>");
    response.print("</div></body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildCustomNetworkConfigHtml(PsychicRequest *request)
{
    String header = "<script>window.onload=function(){var physelect=document.getElementsByName('NWCUSTPHY')[0];hideshowopt(physelect.value);physelect.addEventListener('change', function(event){var select=event.target;var selectedOption=select.options[select.selectedIndex];hideshowopt(selectedOption.getAttribute('value'));});};function hideshowopt(value){if(value>=1&&value<=3){hideopt('internalopt',true);hideopt('externalopt',false);}else if(value>=4&&value<=9){hideopt('internalopt', false);hideopt('externalopt', true);}else {hideopt('internalopt', true);hideopt('externalopt', true);}}function hideopt(opts,hide){var hideopts = document.getElementsByClassName(opts);for(var i=0;i<hideopts.length;i++){if(hide==true){hideopts[i].style.display='none';}else{hideopts[i].style.display='block';}}}</script>";
    PsychicStreamResponse response(request, "text/plain");
    response.beginSend();
    buildHtmlHeader(&response, header);
    response.print("<form class=\"adapt\" method=\"post\" action=\"savecfg\">");
    response.print("<h3>Custom Ethernet Configuration</h3>");
    response.print("<table>");
    printDropDown(&response, "NWCUSTPHY", "PHY", String(_preferences->getInt(preference_network_custom_phy)), getNetworkCustomPHYOptions(), "");
    printInputField(&response, "NWCUSTADDR", "ADDR", _preferences->getInt(preference_network_custom_addr, 1), 6, "");
#if defined(CONFIG_IDF_TARGET_ESP32)
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

esp_err_t WebCfgServer::buildHtml(PsychicRequest *request)
{
    String header = "<script>let intervalId; window.onload = function() { updateInfo(); intervalId = setInterval(updateInfo, 3000); }; function updateInfo() { var request = new XMLHttpRequest(); request.open('GET', '/status', true); request.onload = () => { const obj = JSON.parse(request.responseText); if (obj.stop == 1) { clearInterval(intervalId); } for (var key of Object.keys(obj)) { if(key=='ota' && document.getElementById(key) !== null) { document.getElementById(key).innerText = \"<a href='/ota'>\" + obj[key] + \"</a>\"; } else if(document.getElementById(key) !== null) { document.getElementById(key).innerText = obj[key]; } } }; request.send(); }</script>";
    PsychicStreamResponse response(request, "text/plain");
    response.beginSend();
    buildHtmlHeader(&response, header);
    if(_rebootRequired)
    {
        response.print("<table><tbody><tr><td colspan=\"2\" style=\"border: 0; color: red; font-size: 32px; font-weight: bold; text-align: center;\">REBOOT REQUIRED TO APPLY SETTINGS</td></tr></tbody></table>");
    }
    if(_preferences->getBool(preference_webserial_enabled, false))
    {
        response.print("<table><tbody><tr><td colspan=\"2\" style=\"border: 0; color: red; font-size: 32px; font-weight: bold; text-align: center;\">WEBSERIAL IS ENABLED, ONLY ENABLE WHEN DEBUGGING AND DISABLE ASAP</td></tr></tbody></table>");
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
            String lockState = pinStateToString(_preferences->getInt(preference_lock_pin_status, 4));
            printParameter(&response, "Nuki Lock PIN status", lockState.c_str(), "", "lockPin");

            if(_preferences->getBool(preference_official_hybrid_enabled, false))
            {
                String offConnected = _nuki->offConnected() ? "Yes": "No";
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
            String openerState = pinStateToString(_preferences->getInt(preference_opener_pin_status, 4));
            printParameter(&response, "Nuki Opener PIN status", openerState.c_str(), "", "openerPin");
        }
    }
    printParameter(&response, "Firmware", NUKI_HUB_VERSION, "/info", "firmware");
    if(_preferences->getBool(preference_check_updates))
    {
        printParameter(&response, "Latest Firmware", _preferences->getString(preference_latest_version).c_str(), "/ota", "ota");
    }
    response.print("</table><br>");
    response.print("<ul id=\"tblnav\">");
    buildNavigationMenuEntry(&response, "Network Configuration", "/ntwconfig");
    buildNavigationMenuEntry(&response, "MQTT Configuration", "/mqttconfig",  _brokerConfigured ? "" : "Please configure MQTT broker");
    buildNavigationMenuEntry(&response, "Nuki Configuration", "/nukicfg");
    buildNavigationMenuEntry(&response, "Access Level Configuration", "/acclvl");
    buildNavigationMenuEntry(&response, "Credentials", "/cred", _pinsConfigured ? "" : "Please configure PIN");
    buildNavigationMenuEntry(&response, "GPIO Configuration", "/gpiocfg");
    buildNavigationMenuEntry(&response, "Firmware update", "/ota");
    buildNavigationMenuEntry(&response, "Import/Export Configuration", "/impexpcfg");
    if(_preferences->getInt(preference_network_hardware, 0) == 11)
    {
        buildNavigationMenuEntry(&response, "Custom Ethernet Configuration", "/custntw");
    }
    if (_preferences->getBool(preference_publish_debug_info, false))
    {
        buildNavigationMenuEntry(&response, "Advanced Configuration", "/advanced");
    }
    if(_preferences->getBool(preference_webserial_enabled, false))
    {
        buildNavigationMenuEntry(&response, "Open Webserial", "/webserial");
    }
#ifndef CONFIG_IDF_TARGET_ESP32H2
    if(_allowRestartToPortal)
    {
        buildNavigationMenuEntry(&response, "Configure Wi-Fi", "/wifi");
    }
#endif
    String rebooturl = "/reboot?CONFIRMTOKEN=" + _confirmCode;
    buildNavigationMenuEntry(&response, "Reboot Nuki Hub", rebooturl.c_str());
    response.print("</ul></body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildCredHtml(PsychicRequest *request)
{
    PsychicStreamResponse response(request, "text/plain");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form id=\"credfrm\" class=\"adapt\" onsubmit=\"return testcreds();\" method=\"post\" action=\"savecfg\">");
    response.print("<h3>Credentials</h3>");
    response.print("<table>");
    printInputField(&response, "CREDUSER", "User (# to clear)", _preferences->getString(preference_cred_user).c_str(), 30, "id=\"inputuser\"", false, true);
    printInputField(&response, "CREDPASS", "Password", "*", 30, "id=\"inputpass\"", true, true);
    printInputField(&response, "CREDPASSRE", "Retype password", "*", 30, "id=\"inputpass2\"", true);
    response.print("</table>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form><script>function testcreds() { var input_user = document.getElementById(\"inputuser\").value; var input_pass = document.getElementById(\"inputpass\").value; var input_pass2 = document.getElementById(\"inputpass2\").value; var pattern = /^[ -~]*$/; if(input_user == '#' || input_user == '') { return true; } if (input_pass != input_pass2) { alert('Passwords do not match'); return false;} if(!pattern.test(input_user) || !pattern.test(input_pass)) { alert('Only non unicode characters are allowed in username and password'); return false;} else { return true; } }</script>");
    if(_nuki != nullptr)
    {
        response.print("<br><br><form class=\"adapt\" method=\"post\" action=\"savecfg\">");
        response.print("<h3>Nuki Lock PIN</h3>");
        response.print("<table>");
        printInputField(&response, "NUKIPIN", "PIN Code (# to clear)", "*", 20, "", true);
        response.print("</table>");
        response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
        response.print("</form>");
    }
    if(_nukiOpener != nullptr)
    {
        response.print("<br><br><form class=\"adapt\" method=\"post\" action=\"savecfg\">");
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
        response.print("<form class=\"adapt\" method=\"post\" action=\"/unpairlock\">");
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
        response.print("<form class=\"adapt\" method=\"post\" action=\"/unpairopener\">");
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
    response.print("Optionally will also reset WiFi settings and reopen WiFi manager portal.");
#endif
    response.print("</h4>");
    response.print("<form class=\"adapt\" method=\"post\" action=\"/factoryreset\">");
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

esp_err_t WebCfgServer::buildNetworkConfigHtml(PsychicRequest *request)
{
    PsychicStreamResponse response(request, "text/plain");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form class=\"adapt\" method=\"post\" action=\"savecfg\">");
    response.print("<h3>Network Configuration</h3>");
    response.print("<table>");
    printInputField(&response, "HOSTNAME", "Host name", _preferences->getString(preference_hostname).c_str(), 100, "");
    printDropDown(&response, "NWHW", "Network hardware", String(_preferences->getInt(preference_network_hardware)), getNetworkDetectionOptions(), "");
    printInputField(&response, "HASSCUURL", "Home Assistant device configuration URL (empty to use http://LOCALIP; fill when using a reverse proxy for example)", _preferences->getString(preference_mqtt_hass_cu_url).c_str(), 261, "");
#ifndef CONFIG_IDF_TARGET_ESP32H2
    printInputField(&response, "RSSI", "RSSI Publish interval (seconds; -1 to disable)", _preferences->getInt(preference_rssi_publish_interval), 6, "");
#endif
    printCheckBox(&response, "RSTDISC", "Restart on disconnect", _preferences->getBool(preference_restart_on_disconnect), "");
    printCheckBox(&response, "CHECKUPDATE", "Check for Firmware Updates every 24h", _preferences->getBool(preference_check_updates), "");
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

esp_err_t WebCfgServer::buildMqttConfigHtml(PsychicRequest *request)
{
    PsychicStreamResponse response(request, "text/plain");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form class=\"adapt\" method=\"post\" action=\"savecfg\">");
    response.print("<h3>Basic MQTT Configuration</h3>");
    response.print("<table>");
    printInputField(&response, "MQTTSERVER", "MQTT Broker", _preferences->getString(preference_mqtt_broker).c_str(), 100, "");
    printInputField(&response, "MQTTPORT", "MQTT Broker port", _preferences->getInt(preference_mqtt_broker_port), 5, "");
    printInputField(&response, "MQTTUSER", "MQTT User (# to clear)", _preferences->getString(preference_mqtt_user).c_str(), 30, "", false, true);
    printInputField(&response, "MQTTPASS", "MQTT Password", "*", 30, "", true, true);
    printInputField(&response, "MQTTPATH", "MQTT NukiHub Path", _preferences->getString(preference_mqtt_lock_path).c_str(), 180, "");
    response.print("</table><br>");

    response.print("<h3>Advanced MQTT Configuration</h3>");
    response.print("<table>");
    printInputField(&response, "HASSDISCOVERY", "Home Assistant discovery topic (empty to disable; usually homeassistant)", _preferences->getString(preference_mqtt_hass_discovery).c_str(), 30, "");
    if(_preferences->getBool(preference_opener_enabled, false))
    {
        printCheckBox(&response, "OPENERCONT", "Set Nuki Opener Lock/Unlock action in Home Assistant to Continuous mode", _preferences->getBool(preference_opener_continuous_mode), "");
    }
    printTextarea(&response, "MQTTCA", "MQTT SSL CA Certificate (*, optional)", _preferences->getString(preference_mqtt_ca).c_str(), TLS_CA_MAX_SIZE, true, true);
    printTextarea(&response, "MQTTCRT", "MQTT SSL Client Certificate (*, optional)", _preferences->getString(preference_mqtt_crt).c_str(), TLS_CERT_MAX_SIZE, true, true);
    printTextarea(&response, "MQTTKEY", "MQTT SSL Client Key (*, optional)", _preferences->getString(preference_mqtt_key).c_str(), TLS_KEY_MAX_SIZE, true, true);
    printInputField(&response, "NETTIMEOUT", "MQTT Timeout until restart (seconds; -1 to disable)", _preferences->getInt(preference_network_timeout), 5, "");
    printCheckBox(&response, "MQTTLOG", "Enable MQTT logging", _preferences->getBool(preference_mqtt_log_enabled), "");
    printCheckBox(&response, "UPDATEMQTT", "Allow updating using MQTT", _preferences->getBool(preference_update_from_mqtt), "");
    printCheckBox(&response, "DISNONJSON", "Disable some extraneous non-JSON topics", _preferences->getBool(preference_disable_non_json), "");
    printCheckBox(&response, "OFFHYBRID", "Enable hybrid official MQTT and Nuki Hub setup", _preferences->getBool(preference_official_hybrid_enabled), "");
    printCheckBox(&response, "HYBRIDACT", "Enable sending actions through official MQTT", _preferences->getBool(preference_official_hybrid_actions), "");
    printInputField(&response, "HYBRIDTIMER", "Time between status updates when official MQTT is offline (seconds)", _preferences->getInt(preference_query_interval_hybrid_lockstate), 5, "");
    // printCheckBox(&response, "HYBRIDRETRY", "Retry command sent using official MQTT over BLE if failed", _preferences->getBool(preference_official_hybrid_retry), ""); // NOT IMPLEMENTED (YET?)
    response.print("</table>");
    response.print("* If no encryption is configured for the MQTT broker, leave empty.<br><br>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form>");
    response.print("</body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildAdvancedConfigHtml(PsychicRequest *request)
{
    PsychicStreamResponse response(request, "text/plain");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form class=\"adapt\" method=\"post\" action=\"savecfg\">");
    response.print("<h3>Advanced Configuration</h3>");
    response.print("<h4 class=\"warning\">Warning: Changing these settings can lead to bootloops that might require you to erase the ESP32 and reflash nukihub using USB/serial</h4>");
    response.print("<table>");
    response.print("<tr><td>Current bootloop prevention state</td><td>");
    response.print(_preferences->getBool(preference_enable_bootloop_reset, false) ? "Enabled" : "Disabled");
    response.print("</td></tr>");
    printCheckBox(&response, "DISNTWNOCON", "Disable Network if not connected within 60s", _preferences->getBool(preference_disable_network_not_connected, false), "");        
    printCheckBox(&response, "WEBLOG", "Enable WebSerial logging", _preferences->getBool(preference_webserial_enabled), "");
    printCheckBox(&response, "BTLPRST", "Enable Bootloop prevention (Try to reset these settings to default on bootloop)", true, "");
    printInputField(&response, "BUFFSIZE", "Char buffer size (min 4096, max 32768)", _preferences->getInt(preference_buffer_size, CHAR_BUFFER_SIZE), 6, "");
    response.print("<tr><td>Advised minimum char buffer size based on current settings</td><td id=\"mincharbuffer\"></td>");
    printInputField(&response, "TSKNTWK", "Task size Network (min 12288, max 32768)", _preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE), 6, "");
    response.print("<tr><td>Advised minimum network task size based on current settings</td><td id=\"minnetworktask\"></td>");
    printInputField(&response, "TSKNUKI", "Task size Nuki (min 8192, max 32768)", _preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE), 6, "");
    printInputField(&response, "ALMAX", "Max auth log entries (min 1, max 50)", _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG), 3, "id=\"inputmaxauthlog\"");
    printInputField(&response, "KPMAX", "Max keypad entries (min 1, max 100)", _preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD), 3, "id=\"inputmaxkeypad\"");
    printInputField(&response, "TCMAX", "Max timecontrol entries (min 1, max 50)", _preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL), 3, "id=\"inputmaxtimecontrol\"");
    printInputField(&response, "AUTHMAX", "Max authorization entries (min 1, max 50)", _preferences->getInt(preference_auth_max_entries, MAX_AUTH), 3, "id=\"inputmaxauth\"");
    printCheckBox(&response, "SHOWSECRETS", "Show Pairing secrets on Info page", _preferences->getBool(preference_show_secrets), "");
    if(_preferences->getBool(preference_lock_enabled, true))
    {
        printCheckBox(&response, "LCKMANPAIR", "Manually set lock pairing data (enable to save values below)", false, "");
        printInputField(&response, "LCKBLEADDR", "currentBleAddress", "", 12, "");
        printInputField(&response, "LCKSECRETK", "secretKeyK", "", 64, "");
        printInputField(&response, "LCKAUTHID", "authorizationId", "", 8, "");
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
    response.print("</table>");

    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form>");
    response.print("</body><script>window.onload = function() { document.getElementById(\"inputmaxauthlog\").addEventListener(\"keyup\", calculate);document.getElementById(\"inputmaxkeypad\").addEventListener(\"keyup\", calculate);document.getElementById(\"inputmaxtimecontrol\").addEventListener(\"keyup\", calculate);document.getElementById(\"inputmaxauth\").addEventListener(\"keyup\", calculate); calculate(); }; function calculate() { var auth = document.getElementById(\"inputmaxauth\").value; var authlog = document.getElementById(\"inputmaxauthlog\").value; var keypad = document.getElementById(\"inputmaxkeypad\").value; var timecontrol = document.getElementById(\"inputmaxtimecontrol\").value; var charbuf = 0; var networktask = 0; var sizeauth = 0; var sizeauthlog = 0; var sizekeypad = 0; var sizetimecontrol = 0; if(auth > 0) { sizeauth = 300 * auth; } if(authlog > 0) { sizeauthlog = 280 * authlog; } if(keypad > 0) { sizekeypad = 350 * keypad; } if(timecontrol > 0) { sizetimecontrol = 120 * timecontrol; } charbuf = sizetimecontrol; networktask = 10240 + sizetimecontrol; if(sizeauthlog>sizekeypad && sizeauthlog>sizetimecontrol && sizeauthlog>sizeauth) { charbuf = sizeauthlog; networktask = 10240 + sizeauthlog;} else if(sizekeypad>sizeauthlog && sizekeypad>sizetimecontrol && sizekeypad>sizeauth) { charbuf = sizekeypad; networktask = 10240 + sizekeypad;} else if(sizeauth>sizeauthlog && sizeauth>sizetimecontrol && sizeauth>sizekeypad) { charbuf = sizeauth; networktask = 10240 + sizeauth;} if(charbuf<4096) { charbuf = 4096; } else if (charbuf>32768) { charbuf = 32768; } if(networktask<12288) { networktask = 12288; } else if (networktask>32768) { networktask = 32768; } document.getElementById(\"mincharbuffer\").innerHTML = charbuf; document.getElementById(\"minnetworktask\").innerHTML = networktask; }</script></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildStatusHtml(PsychicRequest *request)
{
    JsonDocument json;
    String jsonStr;
    bool mqttDone = false;
    bool lockDone = false;
    bool openerDone = false;
    bool latestDone = false;

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
            json["lockPin"] = pinStateToString(_preferences->getInt(preference_lock_pin_status, 4));
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
            json["openerPin"] = pinStateToString(_preferences->getInt(preference_opener_pin_status, 4));
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
        latestDone = true;
    }
    else
    {
        latestDone = true;
    }

    if(mqttDone && lockDone && openerDone && latestDone)
    {
        json["stop"] = 1;
    }

    serializeJson(json, jsonStr);
    return request->reply(200, "application/json", jsonStr.c_str());
}

String WebCfgServer::pinStateToString(uint8_t value)
{
    switch(value)
    {
    case 0:
        return String("PIN not set");
    case 1:
        return String("PIN valid");
    case 2:
        return String("PIN set but invalid");
    default:
        return String("Unknown");
    }
}

esp_err_t WebCfgServer::buildAccLvlHtml(PsychicRequest *request)
{
    PsychicStreamResponse response(request, "text/plain");
    response.beginSend();
    buildHtmlHeader(&response);

    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    response.print("<form method=\"post\" action=\"savecfg\">");
    response.print("<input type=\"hidden\" name=\"ACLLVLCHANGED\" value=\"1\">");
    response.print("<h3>Nuki General Access Control</h3>");
    response.print("<table><tr><th>Setting</th><th>Enabled</th></tr>");
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
        uint32_t advancedLockConfigAclPrefs[22];
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
        response.print("</table><br>");
        response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    }
    if(_nukiOpener != nullptr)
    {
        uint32_t basicOpenerConfigAclPrefs[14];
        _preferences->getBytes(preference_conf_opener_basic_acl, &basicOpenerConfigAclPrefs, sizeof(basicOpenerConfigAclPrefs));
        uint32_t advancedOpenerConfigAclPrefs[20];
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
        response.print("</table><br>");
        response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    }
    response.print("</form>");
    response.print("</body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildNukiConfigHtml(PsychicRequest *request)
{
    PsychicStreamResponse response(request, "text/plain");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form class=\"adapt\" method=\"post\" action=\"savecfg\">");
    response.print("<h3>Basic Nuki Configuration</h3>");
    response.print("<table>");
    printCheckBox(&response, "LOCKENA", "Nuki Lock enabled", _preferences->getBool(preference_lock_enabled), "");
    printCheckBox(&response, "OPENA", "Nuki Opener enabled", _preferences->getBool(preference_opener_enabled), "");
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
    if(_preferences->getBool(preference_lock_enabled, true))
    {
        printCheckBox(&response, "REGAPP", "Lock: Nuki Bridge is running alongside Nuki Hub (needs re-pairing if changed)", _preferences->getBool(preference_register_as_app), "");
    }
    if(_preferences->getBool(preference_opener_enabled, false))
    {
        printCheckBox(&response, "REGAPPOPN", "Opener: Nuki Bridge is running alongside Nuki Hub (needs re-pairing if changed)", _preferences->getBool(preference_register_opener_as_app), "");
    }
    printInputField(&response, "RSBC", "Restart if bluetooth beacons not received (seconds; -1 to disable)", _preferences->getInt(preference_restart_ble_beacon_lost), 10, "");
    printInputField(&response, "TXPWR", "BLE transmit power in dB (minimum -12, maximum 9)", _preferences->getInt(preference_ble_tx_power, 9), 10, "");

    response.print("</table>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form>");
    response.print("</body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildGpioConfigHtml(PsychicRequest *request)
{
    PsychicStreamResponse response(request, "text/plain");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form method=\"post\" action=\"savegpiocfg\">");
    response.print("<h3>GPIO Configuration</h3>");
    response.print("<table>");
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
esp_err_t WebCfgServer::buildConfigureWifiHtml(PsychicRequest *request)
{
    PsychicStreamResponse response(request, "text/plain");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<h3>Wi-Fi</h3>");
    response.print("Click confirm to remove saved WiFi settings and restart ESP into Wi-Fi configuration mode. After restart, connect to ESP access point to reconfigure Wi-Fi.<br><br>");
    buildNavigationButton(&response, "Confirm", "/wifimanager");
    response.print("</body></html>");
    return response.endSend();
}
#endif

esp_err_t WebCfgServer::buildInfoHtml(PsychicRequest *request)
{
    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));
    PsychicStreamResponse response(request, "text/plain");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<h3>System Information</h3><pre>");
    response.print("------------ NUKI HUB ------------");
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
#ifdef CONFIG_SOC_SPIRAM_SUPPORTED
    if(esp_psram_get_size() > 0)
    {
        response.print("\nPSRAM Available: Yes");
        response.print("\nTotal PSRAM: ");
        response.print(esp_psram_get_size());
        response.print("\nFree PSRAM: ");
        response.print((esp_get_free_heap_size() - ESP.getFreeHeap()));
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
    response.print("\nWeb configurator username: ");
    response.print(_preferences->getString(preference_cred_user, "").length() > 0 ? "***" : "Not set");
    response.print("\nWeb configurator password: ");
    response.print(_preferences->getString(preference_cred_password, "").length() > 0 ? "***" : "Not set");
    response.print("\nWeb configurator enabled: ");
    response.print(_preferences->getBool(preference_webserver_enabled, true) ? "Yes" : "No");
    response.print("\nPublish debug information enabled: ");
    response.print(_preferences->getBool(preference_publish_debug_info, false) ? "Yes" : "No");
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

#ifndef CONFIG_IDF_TARGET_ESP32H2
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
    }
#endif
    response.print("\nRestart ESP32 on network disconnect enabled: ");
    response.print(_preferences->getBool(preference_restart_on_disconnect, false) ? "Yes" : "No");
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
    response.print("\nMQTT SSL CA: ");
    response.print(_preferences->getString(preference_mqtt_ca, "").length() > 0 ? "***" : "Not set");
    response.print("\nMQTT SSL CRT: ");
    response.print(_preferences->getString(preference_mqtt_crt, "").length() > 0 ? "***" : "Not set");
    response.print("\nMQTT SSL Key: ");
    response.print(_preferences->getString(preference_mqtt_key, "").length() > 0 ? "***" : "Not set");
    response.print("\n\n------------ BLUETOOTH ------------");
    response.print("\nBluetooth TX power (dB): ");
    response.print(_preferences->getInt(preference_ble_tx_power, 9));
    response.print("\nBluetooth command nr of retries: ");
    response.print(_preferences->getInt(preference_command_nr_of_retries, 3));
    response.print("\nBluetooth command retry delay (ms): ");
    response.print(_preferences->getInt(preference_command_retry_delay, 100));
    response.print("\nSeconds until reboot when no BLE beacons recieved: ");
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
    response.print("\n\n------------ HOME ASSISTANT ------------");
    response.print("\nHome Assistant auto discovery enabled: ");
    if(_preferences->getString(preference_mqtt_hass_discovery, "").length() > 0)
    {
        response.print("Yes");
        response.print("\nHome Assistant auto discovery topic: ");
        response.print(_preferences->getString(preference_mqtt_hass_discovery, "") + "/");
        response.print("\nNuki Hub configuration URL for HA: ");
        response.print(_preferences->getString(preference_mqtt_hass_cu_url, "").length() > 0 ? _preferences->getString(preference_mqtt_hass_cu_url, "") : "http://" + _network->localIP());
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
        response.print("\nRegister as: ");
        response.print(_preferences->getBool(preference_register_as_app, false) ? "App" : "Bridge");
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
            response.print("\nSending actions through official MQTT enabled: ");
            response.print(_preferences->getBool(preference_official_hybrid_actions, false) ? "Yes" : "No");
            /* NOT IMPLEMENTED (YET?)
            if(_preferences->getBool(preference_official_hybrid_actions, false))
            {
                response.print("\nRetry actions through BLE enabled: ");
                response.print(_preferences->getBool(preference_official_hybrid_retry, false) ? "Yes" : "No");
            }
            */
            response.print("\nTime between status updates when official MQTT is offline (s): ");
            response.print(_preferences->getInt(preference_query_interval_hybrid_lockstate, 600));
        }
        uint32_t basicLockConfigAclPrefs[16];
        _preferences->getBytes(preference_conf_lock_basic_acl, &basicLockConfigAclPrefs, sizeof(basicLockConfigAclPrefs));
        uint32_t advancedLockConfigAclPrefs[22];
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
        response.print("\nRegister as: ");
        response.print(_preferences->getBool(preference_register_opener_as_app, false) ? "App" : "Bridge");
        response.print("\nNuki Opener Lock/Unlock action set to Continuous mode in Home Assistant: ");
        response.print(_preferences->getBool(preference_opener_continuous_mode, false) ? "Yes" : "No");
        uint32_t basicOpenerConfigAclPrefs[14];
        _preferences->getBytes(preference_conf_opener_basic_acl, &basicOpenerConfigAclPrefs, sizeof(basicOpenerConfigAclPrefs));
        uint32_t advancedOpenerConfigAclPrefs[20];
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
        }
    }

    response.print("\n\n------------ GPIO ------------\n");
    String gpioStr = "";
    _gpio->getConfigurationText(gpioStr, _gpio->pinConfiguration());
    response.print(gpioStr);
    response.print("</pre></body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::processUnpair(PsychicRequest *request, bool opener)
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
        return buildConfirmHtml(request, "Confirm code is invalid.", 3, true);
    }

    esp_err_t res = buildConfirmHtml(request, opener ? "Unpairing Nuki Opener and restarting." : "Unpairing Nuki Lock and restarting.", 3, true);

    if(!opener && _nuki != nullptr)
    {
        _nuki->disableHASS();
        _nuki->unpair();
    }
    if(opener && _nukiOpener != nullptr)
    {
        _nukiOpener->disableHASS();
        _nukiOpener->unpair();
    }
    waitAndProcess(false, 1000);
    restartEsp(RestartReason::DeviceUnpaired);
    return res;
}

esp_err_t WebCfgServer::processUpdate(PsychicRequest *request)
{
    esp_err_t res;
    String value = "";
    if(request->hasParam("token"))
    {
        const PsychicWebParameter* p = request->getParam("token");
        if(p->value() != "")
        {
            value = p->value();
        }
    }

    if(value != _confirmCode)
    {
        return buildConfirmHtml(request, "Confirm code is invalid.", 3, true);
    }

    if(request->hasParam("beta"))
    {
        if(request->hasParam("debug"))
        {
            res = buildConfirmHtml(request, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest DEBUG BETA version", 2, true);
            _preferences->putString(preference_ota_updater_url, GITHUB_BETA_UPDATER_BINARY_URL_DBG);
            _preferences->putString(preference_ota_main_url, GITHUB_BETA_RELEASE_BINARY_URL_DBG);
        }
        else
        {
            res = buildConfirmHtml(request, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest BETA version", 2, true);
            _preferences->putString(preference_ota_updater_url, GITHUB_BETA_UPDATER_BINARY_URL);
            _preferences->putString(preference_ota_main_url, GITHUB_BETA_RELEASE_BINARY_URL);
        }
    }
    else if(request->hasParam("master"))
    {
        if(request->hasParam("debug"))
        {
            res = buildConfirmHtml(request, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest DEBUG DEVELOPMENT version", 2, true);
            _preferences->putString(preference_ota_updater_url, GITHUB_MASTER_UPDATER_BINARY_URL_DBG);
            _preferences->putString(preference_ota_main_url, GITHUB_MASTER_RELEASE_BINARY_URL_DBG);
        }
        else
        {
            res = buildConfirmHtml(request, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest DEVELOPMENT version", 2, true);
            _preferences->putString(preference_ota_updater_url, GITHUB_MASTER_UPDATER_BINARY_URL);
            _preferences->putString(preference_ota_main_url, GITHUB_MASTER_RELEASE_BINARY_URL);
        }
    }
    else
    {
        if(request->hasParam("debug"))
        {
            res = buildConfirmHtml(request, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest DEBUG RELEASE version", 2, true);
            _preferences->putString(preference_ota_updater_url, GITHUB_LATEST_UPDATER_BINARY_URL_DBG);
            _preferences->putString(preference_ota_main_url, GITHUB_LATEST_UPDATER_BINARY_URL_DBG);
        }
        else
        {
            res = buildConfirmHtml(request, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest RELEASE version", 2, true);
            _preferences->putString(preference_ota_updater_url, GITHUB_LATEST_UPDATER_BINARY_URL);
            _preferences->putString(preference_ota_main_url, GITHUB_LATEST_RELEASE_BINARY_URL);
        }
    }
    waitAndProcess(true, 1000);
    restartEsp(RestartReason::OTAReboot);
    return res;
}

esp_err_t WebCfgServer::processFactoryReset(PsychicRequest *request)
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
        return buildConfirmHtml(request, "Confirm code is invalid.", 3, true);
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
            res = buildConfirmHtml(request, "Factory resetting Nuki Hub, unpairing Nuki Lock and Nuki Opener and resetting WiFi.", 3, true);
        }
        else
        {
            res = buildConfirmHtml(request, "Factory resetting Nuki Hub, unpairing Nuki Lock and Nuki Opener.", 3, true);
        }
    }

    waitAndProcess(false, 2000);

    if(_nuki != nullptr)
    {
        _nuki->disableHASS();
        _nuki->unpair();
    }
    if(_nukiOpener != nullptr)
    {
        _nukiOpener->disableHASS();
        _nukiOpener->unpair();
    }

    _preferences->clear();

#ifndef CONFIG_IDF_TARGET_ESP32H2
    if(resetWifi)
    {
        _network->reconfigureDevice();
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

void WebCfgServer::buildNavigationButton(PsychicStreamResponse *response, const char *caption, const char *targetPath, const char* labelText)
{
    response->print("<form method=\"get\" action=\"");
    response->print(targetPath);
    response->print("\">");
    response->print("<button type=\"submit\">");
    response->print(caption);
    response->print("</button> ");
    response->print(labelText);
    response->print("</form>");
}

void WebCfgServer::buildNavigationMenuEntry(PsychicStreamResponse *response, const char *title, const char *targetPath, const char* warningMessage)
{
    response->print("<a href=\"");
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
    options.push_back(std::make_pair("8", "GL-S10"));
    options.push_back(std::make_pair("9", "ETH01-Evo"));
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
#if defined(CONFIG_IDF_TARGET_ESP32)
    options.push_back(std::make_pair("4", "LAN8720"));
    options.push_back(std::make_pair("5", "RTL8201"));
    options.push_back(std::make_pair("6", "TLK110"));
    options.push_back(std::make_pair("7", "DP83848"));
    options.push_back(std::make_pair("8", "KSZ8041"));
    options.push_back(std::make_pair("9", "KSZ8081"));
#endif

    return options;
}
#if defined(CONFIG_IDF_TARGET_ESP32)
const std::vector<std::pair<String, String>> WebCfgServer::getNetworkCustomCLKOptions() const
{
    std::vector<std::pair<String, String>> options;
    options.push_back(std::make_pair("0", "GPIO0 IN"));
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

String WebCfgServer::getPreselectionForGpio(const uint8_t &pin)
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
#endif
