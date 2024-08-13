#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include <esp_task_wdt.h>
#include <esp_wifi.h>
#include <Update.h>

#ifndef NUKI_HUB_UPDATER
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include "ArduinoJson.h"

WebCfgServer::WebCfgServer(NukiWrapper* nuki, NukiOpenerWrapper* nukiOpener, NukiNetwork* network, Gpio* gpio, Preferences* preferences, bool allowRestartToPortal, uint8_t partitionType, AsyncWebServer* asyncServer)
: _nuki(nuki),
  _nukiOpener(nukiOpener),
  _network(network),
  _gpio(gpio),
  _preferences(preferences),
  _allowRestartToPortal(allowRestartToPortal),
  _partitionType(partitionType),
  _asyncServer(asyncServer)
#else
WebCfgServer::WebCfgServer(NukiNetwork* network, Preferences* preferences, bool allowRestartToPortal, uint8_t partitionType, AsyncWebServer* asyncServer)
: _network(network),
  _preferences(preferences),
  _allowRestartToPortal(allowRestartToPortal),
  _partitionType(partitionType),
  _asyncServer(asyncServer)
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
    _asyncServer->on("/", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        #ifndef NUKI_HUB_UPDATER
        buildHtml(request);
        #else
        buildOtaHtml(request);
        #endif
    });
    _asyncServer->on("/style.css", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        sendCss(request);
    });
    _asyncServer->on("/favicon.ico", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        sendFavicon(request);
    });
    #ifndef NUKI_HUB_UPDATER
    _asyncServer->on("/import", [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        String message = "";
        bool restart = processImport(request, message);
        buildConfirmHtml(request, message, 3);        
        if(restart)
        {
            Log->println(F("Restarting"));
            waitAndProcess(false, 1000);
            restartEsp(RestartReason::ImportCompleted);
        }
    });
    _asyncServer->on("/export", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        sendSettings(request);
    });
    _asyncServer->on("/impexpcfg", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildImportExportHtml(request);
    });
    _asyncServer->on("/status", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildStatusHtml(request);
    });
    _asyncServer->on("/acclvl", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildAccLvlHtml(request, 0);
    });
    _asyncServer->on("/acllock", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildAccLvlHtml(request, 1);
    });
    _asyncServer->on("/aclopener", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildAccLvlHtml(request, 2);
    });

    _asyncServer->on("/advanced", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildAdvancedConfigHtml(request);
    });
    _asyncServer->on("/cred", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildCredHtml(request);
    });
    _asyncServer->on("/mqttconfig", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildMqttConfigHtml(request);
    });
    _asyncServer->on("/nukicfg", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildNukiConfigHtml(request);
    });
    _asyncServer->on("/gpiocfg", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildGpioConfigHtml(request);
    });
    _asyncServer->on("/wifi", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildConfigureWifiHtml(request);
    });
    _asyncServer->on("/unpairlock", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        processUnpair(request, false);
    });
    _asyncServer->on("/unpairopener", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        processUnpair(request, true);
    });
    _asyncServer->on("/factoryreset", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        processFactoryReset(request);
    });
    _asyncServer->on("/wifimanager", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        if(_allowRestartToPortal)
        {
            buildConfirmHtml(request, "Restarting. Connect to ESP access point to reconfigure Wi-Fi.", 0);
            waitAndProcess(false, 1000);
            _network->reconfigureDevice();
        }
    });
    _asyncServer->on("/info", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildInfoHtml(request);
    });
    _asyncServer->on("/debugon", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        _preferences->putBool(preference_publish_debug_info, true);
        buildConfirmHtml(request, "Debug On", 3);
        Log->println(F("Restarting"));
        waitAndProcess(true, 1000);
        restartEsp(RestartReason::ConfigurationUpdated);
    });
    _asyncServer->on("/debugoff", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        _preferences->putBool(preference_publish_debug_info, false);
        buildConfirmHtml(request, "Debug Off", 3);
        Log->println(F("Restarting"));
        waitAndProcess(true, 1000);
        restartEsp(RestartReason::ConfigurationUpdated);
    });
    _asyncServer->on("/savecfg", HTTP_POST, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        String message = "";
        bool restart = processArgs(request, message);
        buildConfirmHtml(request, message, 3);
        if(restart)
        {
            Log->println(F("Restarting"));
            waitAndProcess(false, 1000);
            restartEsp(RestartReason::ConfigurationUpdated);
        }
        else
        {
            waitAndProcess(false, 1000);
        }
    });
    _asyncServer->on("/savegpiocfg", HTTP_POST, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        processGpioArgs(request);
        buildConfirmHtml(request, "Saving GPIO configuration", 3);
        Log->println(F("Restarting"));
        waitAndProcess(true, 1000);
        restartEsp(RestartReason::GpioConfigurationUpdated);
    });
    #endif
    _asyncServer->on("/ota", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildOtaHtml(request);
    });
    _asyncServer->on("/otadebug", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildOtaHtml(request, true);
    });
    _asyncServer->on("/reboottoota", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildConfirmHtml(request, "Rebooting to other partition", 2);
        waitAndProcess(true, 1000);
        esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL));
        restartEsp(RestartReason::OTAReboot);
    });
    _asyncServer->on("/reboot", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildConfirmHtml(request, "Rebooting", 2);
        waitAndProcess(true, 1000);
        restartEsp(RestartReason::RequestedViaWebServer);
    });
    _asyncServer->on("/autoupdate", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        #ifndef NUKI_HUB_UPDATER
        processUpdate(request);
        #else
        request->redirect("/");
        #endif
    });
    _asyncServer->on("/uploadota", HTTP_POST,
      [&](AsyncWebServerRequest *request) {},
      [&](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
      {
          if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
          handleOtaUpload(request, filename, index, data, len, final);
      }
    );
    //Update.onProgress(printProgress);
}

void WebCfgServer::buildOtaHtml(AsyncWebServerRequest *request, bool debug)
{
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    buildHtmlHeader(response);

    bool errored = false;
    if(request->hasParam("errored"))
    {
        const AsyncWebParameter* p = request->getParam("errored");
        if(p->value() != "") errored = true;
    }

    if(errored) response->print("<div>Over-the-air update errored. Please check the logs for more info</div><br/>");

    if(_partitionType == 0)
    {
        response->print("<h4 class=\"warning\">You are currently running Nuki Hub with an outdated partition scheme. Because of this you cannot use OTA to update to 9.00 or higher. Please check GitHub for instructions on how to update to 9.00 and the new partition scheme</h4>");
        response->print("<button title=\"Open latest release on GitHub\" onclick=\" window.open('");
        response->print(GITHUB_LATEST_RELEASE_URL);
        response->print("', '_blank'); return false;\">Open latest release on GitHub</button>");
        return;
    }

    response->print("<div id=\"msgdiv\" style=\"visibility:hidden\">Initiating Over-the-air update. This will take about two minutes, please be patient.<br>You will be forwarded automatically when the update is complete.</div>");
    response->print("<div id=\"autoupdform\"><h4>Update Nuki Hub</h4>");
    response->print("Click on the button to reboot and automatically update Nuki Hub and the Nuki Hub updater to the latest versions from GitHub");
    response->print("<div style=\"clear: both\"></div>");

    String release_type;

    if(debug) release_type = "debug";
    else release_type = "release";

    #ifndef DEBUG_NUKIHUB
    String build_type = "release";
    #else
    String build_type = "debug";
    #endif

    response->print("<form onsubmit=\"if(document.getElementById('currentver') == document.getElementById('latestver') && '" + release_type + "' == '" + build_type + "') { alert('You are already on this version, build and build type'); return false; } else { return confirm('Do you really want to update to the latest release?'); } \" action=\"/autoupdate\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"release\" value=\"1\" /><input type=\"hidden\" name=\"" + release_type + "\" value=\"1\" /><input type=\"hidden\" name=\"token\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"background: green\" value=\"Update to latest release\"></form>");
    response->print("<form onsubmit=\"if(document.getElementById('currentver') == document.getElementById('betaver') && '" + release_type + "' == '" + build_type + "') { alert('You are already on this version, build and build type'); return false; } else { return confirm('Do you really want to update to the latest beta? This version could contain breaking bugs and necessitate downgrading to the latest release version using USB/Serial'); }\" action=\"/autoupdate\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"beta\" value=\"1\" /><input type=\"hidden\" name=\"" + release_type + "\" value=\"1\" /><input type=\"hidden\" name=\"token\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"color: black; background: yellow\"  value=\"Update to latest beta\"></form>");
    response->print("<form onsubmit=\"if(document.getElementById('currentver') == document.getElementById('devver') && '" + release_type + "' == '" + build_type + "') { alert('You are already on this version, build and build type'); return false; } else { return confirm('Do you really want to update to the latest development version? This version could contain breaking bugs and necessitate downgrading to the latest release version using USB/Serial'); }\" action=\"/autoupdate\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"master\" value=\"1\" /><input type=\"hidden\" name=\"" + release_type + "\" value=\"1\" /><input type=\"hidden\" name=\"token\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"background: red\"  value=\"Update to latest development version\"></form>");
    response->print("<div style=\"clear: both\"></div><br>");

    response->print("<b>Current version: </b><span id=\"currentver\">");
    response->print(NUKI_HUB_VERSION);
    response->print(" (");
    response->print(NUKI_HUB_BUILD);
    response->print(")</span>, ");
    response->print(NUKI_HUB_DATE);
    response->print("<br>");

    #ifndef NUKI_HUB_UPDATER
    bool manifestSuccess = false;

    NetworkClientSecure *client = new NetworkClientSecure;
    if (client) {
        client->setDefaultCACertBundle();
        {
            HTTPClient https;
            https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            https.setTimeout(2500);
            https.useHTTP10(true);

            if (https.begin(*client, GITHUB_OTA_MANIFEST_URL)) {
                int http_responseCode = https.GET();

                if (http_responseCode == HTTP_CODE_OK || http_responseCode == HTTP_CODE_MOVED_PERMANENTLY)
                {
                    JsonDocument doc;
                    DeserializationError jsonError = deserializeJson(doc, https.getStream());

                    if (!jsonError)
                    {
                        manifestSuccess = true;
                        response->print("<b>Latest release version: </b><span id=\"latestver\">");
                        response->print(doc["release"]["fullversion"].as<const char*>());
                        response->print(" (");
                        response->print(doc["release"]["build"].as<const char*>());
                        response->print(")</span>, ");
                        response->print(doc["release"]["time"].as<const char*>());
                        response->print("<br>");
                        response->print("<b>Latest beta version: </b><span id=\"betaver\">");
                        if(doc["beta"]["fullversion"] != "No beta available")
                        {
                            response->print(doc["beta"]["fullversion"].as<const char*>());
                            response->print(" (");
                            response->print(doc["beta"]["build"].as<const char*>());
                            response->print(")</span>, ");
                            response->print(doc["beta"]["time"].as<const char*>());
                        }
                        else
                        {
                            response->print(doc["beta"]["fullversion"].as<const char*>());
                            response->print("</span>");
                        }
                        response->print("<br>");
                        response->print("<b>Latest development version: </b><span id=\"devver\">");
                        response->print(doc["master"]["fullversion"].as<const char*>());
                        response->print(" (");
                        response->print(doc["master"]["build"].as<const char*>());
                        response->print(")</span>, ");
                        response->print(doc["master"]["time"].as<const char*>());
                        response->print("<br>");
                    }
                }
                https.end();
            }
        }
        delete client;
    }

    if(!manifestSuccess)
    {
        response->print("<span id=\"currentver\" style=\"display: none;\">currentver</span><span id=\"latestver\" style=\"display: none;\">latestver</span><span id=\"devver\" style=\"display: none;\">devver</span><span id=\"betaver\" style=\"display: none;\">betaver</span>");
    }
    #endif
    response->print("<br></div>");

    if(_partitionType == 1)
    {
        response->print("<h4><a onclick=\"hideshowmanual();\">Manually update Nuki Hub</a></h4><div id=\"manualupdate\" style=\"display: none\">");
        response->print("<div id=\"rebootform\"><h4>Reboot to Nuki Hub Updater</h4>");
        response->print("Click on the button to reboot to the Nuki Hub updater, where you can select the latest Nuki Hub binary to update");
        response->print("<form action=\"/reboottoota\" method=\"get\"><br><input type=\"submit\" value=\"Reboot to Nuki Hub Updater\" /></form><br><br></div>");
        response->print("<div id=\"upform\"><h4>Update Nuki Hub Updater</h4>");
        response->print("Select the latest Nuki Hub updater binary to update the Nuki Hub updater");
        response->print("<form enctype=\"multipart/form-data\" action=\"/uploadota\" method=\"post\">Choose the nuki_hub_updater.bin file to upload: <input name=\"uploadedfile\" type=\"file\" accept=\".bin\" /><br/>");
    }
    else
    {
        response->print("<div id=\"manualupdate\">");
        response->print("<div id=\"rebootform\"><h4>Reboot to Nuki Hub</h4>");
        response->print("Click on the button to reboot to Nuki Hub");
        response->print("<form action=\"/reboottoota\" method=\"get\"><br><input type=\"submit\" value=\"Reboot to Nuki Hub\" /></form><br><br></div>");
        response->print("<div id=\"upform\"><h4>Update Nuki Hub</h4>");
        response->print("Select the latest Nuki Hub binary to update Nuki Hub");
        response->print("<form enctype=\"multipart/form-data\" action=\"/uploadota\" method=\"post\">Choose the nuki_hub.bin file to upload: <input name=\"uploadedfile\" type=\"file\" accept=\".bin\" /><br/>");
    }
    response->print("<br><input id=\"submitbtn\" type=\"submit\" value=\"Upload File\" /></form><br><br></div>");
    response->print("<div id=\"gitdiv\">");
    response->print("<h4>GitHub</h4><br>");
    response->print("<button title=\"Open latest release on GitHub\" onclick=\" window.open('");
    response->print(GITHUB_LATEST_RELEASE_URL);
    response->print("', '_blank'); return false;\">Open latest release on GitHub</button>");
    response->print("<br><br><button title=\"Download latest binary from GitHub\" onclick=\" window.open('");
    response->print(GITHUB_LATEST_RELEASE_BINARY_URL);
    response->print("'); return false;\">Download latest binary from GitHub</button>");
    response->print("<br><br><button title=\"Download latest updater binary from GitHub\" onclick=\" window.open('");
    response->print(GITHUB_LATEST_UPDATER_BINARY_URL);
    response->print("'); return false;\">Download latest updater binary from GitHub</button></div></div>");
    response->print("<script type=\"text/javascript\">");
    response->print("window.addEventListener('load', function () {");
    response->print("	var button = document.getElementById(\"submitbtn\");");
    response->print("	button.addEventListener('click',hideshow,false);");
    response->print("	function hideshow() {");
    response->print("		document.getElementById('autoupdform').style.visibility = 'hidden';");
    response->print("		document.getElementById('rebootform').style.visibility = 'hidden';");
    response->print("		document.getElementById('upform').style.visibility = 'hidden';");
    response->print("		document.getElementById('gitdiv').style.visibility = 'hidden';");
    response->print("		document.getElementById('msgdiv').style.visibility = 'visible';");
    response->print("	}");
    response->print("});");
    response->print("function hideshowmanual() {");
    response->print("	var x = document.getElementById(\"manualupdate\");");
    response->print("	if (x.style.display === \"none\") {");
    response->print("	    x.style.display = \"block\";");
    response->print("	} else {");
    response->print("	    x.style.display = \"none\";");
    response->print("    }");
    response->print("}");
    response->print("</script>");
    response->print("</body></html>");
    request->send(response);
}

void WebCfgServer::buildOtaCompletedHtml(AsyncWebServerRequest *request)
{
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    buildHtmlHeader(response);

    response->print("<div>Over-the-air update completed.<br>You will be forwarded automatically.</div>");
    response->print("<script type=\"text/javascript\">");
    response->print("window.addEventListener('load', function () {");
    response->print("   setTimeout(\"location.href = '/';\",10000);");
    response->print("});");
    response->print("</script>");
    response->print("</body></html>");
    request->send(response);
}

void WebCfgServer::buildHtmlHeader(AsyncResponseStream *response, String additionalHeader)
{
    response->print("<html><head>");
    response->print("<meta name='viewport' content='width=device-width, initial-scale=1'>");
    if(strcmp(additionalHeader.c_str(), "") != 0) response->print(additionalHeader);
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

void WebCfgServer::printProgress(size_t prg, size_t sz) {
  Log->printf("Progress: %d%%\n", (prg*100)/_otaContentLen);
}

void WebCfgServer::handleOtaUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    if(!request->url().endsWith("/uploadota")) return;

    if(filename == "")
    {
        Log->println("Invalid file for OTA upload");
        return;
    }

    if (!index)
    {
        Log->println("Starting manual OTA update");
        _otaContentLen = request->contentLength();

        if(_partitionType == 1 && _otaContentLen > 1600000)
        {
            Log->println("Uploaded OTA file too large, are you trying to upload a Nuki Hub binary instead of a Nuki Hub updater binary?");
            return;
        }
        else if(_partitionType == 2 && _otaContentLen < 1600000)
        {
            Log->println("Uploaded OTA file is too small, are you trying to upload a Nuki Hub updater binary instead of a Nuki Hub binary?");
            return;
        }

        int cmd = U_FLASH;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
            Update.printError(Serial);
        }

        _otaStartTs = esp_timer_get_time() / 1000;
        esp_task_wdt_config_t twdt_config = {
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
    }

    if (_otaContentLen == 0) return;

    if (Update.write(data, len) != len) {
        Update.printError(Serial);
        restartEsp(RestartReason::OTAAborted);
    }

    if (final) {
        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
        response->addHeader("Refresh", "20");
        response->addHeader("Location", "/");
        request->send(response);
        if (!Update.end(true)){
            Update.printError(Serial);
            restartEsp(RestartReason::OTAAborted);
        } else {
            Log->print(F("Progress: 100%"));
            Log->println();
            Log->print("handleFileUpload Total Size: ");
            Log->println(index+len);
            Log->println("Update complete");
            Log->flush();
            restartEsp(RestartReason::OTACompleted);
        }
    }
}

void WebCfgServer::buildConfirmHtml(AsyncWebServerRequest *request, const String &message, uint32_t redirectDelay, bool redirect)
{
    AsyncResponseStream *response = request->beginResponseStream("text/html");
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
    buildHtmlHeader(response, header);
    response->print(message);
    response->print("</body></html>");
    request->send(response);
}

void WebCfgServer::sendCss(AsyncWebServerRequest *request)
{
    // escaped by https://www.cescaper.com/
    AsyncWebServerResponse *asyncResponse = request->beginResponse(200, "text/css", (const uint8_t*)stylecss, sizeof(stylecss));
    asyncResponse ->addHeader("Cache-Control", "public, max-age=3600");
    request->send(asyncResponse );
}

void WebCfgServer::sendFavicon(AsyncWebServerRequest *request)
{
    AsyncWebServerResponse *asyncResponse = request->beginResponse(200, "image/png", (const uint8_t*)favicon_32x32, sizeof(favicon_32x32));
    asyncResponse->addHeader("Cache-Control", "public, max-age=604800");
    request->send(asyncResponse);
}

String WebCfgServer::generateConfirmCode()
{
    int code = random(1000,9999);
    return String(code);
}

#ifndef NUKI_HUB_UPDATER
void WebCfgServer::sendSettings(AsyncWebServerRequest *request)
{
    bool redacted = false;
    bool pairing = false;

    if(request->hasParam("redacted"))
    {
        const AsyncWebParameter* p = request->getParam("redacted");
        if(p->value() == "1") redacted = true;
    }
    if(request->hasParam("pairing"))
    {
        const AsyncWebParameter* p = request->getParam("pairing");
        if(p->value() == "1") pairing = true;
    }

    JsonDocument json;
    String jsonPretty;

    DebugPreferences debugPreferences;

    const std::vector<char*> keysPrefs = debugPreferences.getPreferencesKeys();
    const std::vector<char*> boolPrefs = debugPreferences.getPreferencesBoolKeys();
    const std::vector<char*> redactedPrefs = debugPreferences.getPreferencesRedactedKeys();
    const std::vector<char*> bytePrefs = debugPreferences.getPreferencesByteKeys();
    const std::vector<char*> charPrefs = debugPreferences.getPreferencesCharKeys();

    for(const auto& key : keysPrefs)
    {
        if(strcmp(key, preference_show_secrets) == 0) continue;
        if(strcmp(key, preference_latest_version) == 0) continue;
        if(strcmp(key, preference_has_mac_saved) == 0) continue;
        if(strcmp(key, preference_device_id_lock) == 0) continue;
        if(strcmp(key, preference_device_id_opener) == 0) continue;
        if(!redacted) if(std::find(redactedPrefs.begin(), redactedPrefs.end(), key) != redactedPrefs.end()) continue;
        if(std::find(charPrefs.begin(), charPrefs.end(), key) != charPrefs.end()) continue;
        if(!_preferences->isKey(key)) json[key] = "";
        else if(std::find(boolPrefs.begin(), boolPrefs.end(), key) != boolPrefs.end()) json[key] = _preferences->getBool(key) ? "1" : "0";
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
            for(int i = 0 ; i < 6 ; i++) {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", currentBleAddress[i]);
            }
            json["bleAddressLock"] = text;
            memset(text, 0, sizeof(text));
            text[0] = '\0';
            for(int i = 0 ; i < 32 ; i++) {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", secretKeyK[i]);
            }
            json["secretKeyKLock"] = text;
            memset(text, 0, sizeof(text));
            text[0] = '\0';
            for(int i = 0 ; i < 4 ; i++) {
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
            for(int i = 0 ; i < 6 ; i++) {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", currentBleAddressOpn[i]);
            }
            json["bleAddressOpener"] = text;
            memset(text, 0, sizeof(text));
            text[0] = '\0';
            for(int i = 0 ; i < 32 ; i++) {
                size_t offset = strlen(text);
                sprintf(&(text[offset]), "%02x", secretKeyKOpn[i]);
            }
            json["secretKeyKOpener"] = text;
            memset(text, 0, sizeof(text));
            text[0] = '\0';
            for(int i = 0 ; i < 4 ; i++) {
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
        if(storedLength == 0) continue;
        uint8_t serialized[storedLength];
        memset(serialized, 0, sizeof(serialized));
        size_t size = _preferences->getBytes(key, serialized, sizeof(serialized));
        if(size == 0) continue;
        char text[255];
        text[0] = '\0';
        for(int i = 0 ; i < size ; i++) {
            size_t offset = strlen(text);
            sprintf(&(text[offset]), "%02x", serialized[i]);
        }
        json[key] = text;
        memset(text, 0, sizeof(text));
    }

    serializeJsonPretty(json, jsonPretty);

    AsyncWebServerResponse *asyncResponse = request->beginResponse(200, "application/json", jsonPretty);
    asyncResponse->addHeader("Content-Disposition", "attachment; filename=nuki_hub.json");
    request->send(asyncResponse);

}

bool WebCfgServer::processArgs(AsyncWebServerRequest *request, String& message)
{
    bool configChanged = false;
    bool aclLvlChanged = false;
    bool clearMqttCredentials = false;
    bool clearCredentials = false;
    bool manPairLck = false;
    bool manPairOpn = false;
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
        const AsyncWebParameter* p = request->getParam(index);
        String key = p->name();
        String value = p->value();

        if(key == "MQTTSERVER")
        {
            _preferences->putString(preference_mqtt_broker, value);
            configChanged = true;
        }
        else if(key == "MQTTPORT")
        {
            _preferences->putInt(preference_mqtt_broker_port, value.toInt());
            configChanged = true;
        }
        else if(key == "MQTTUSER")
        {
            if(value == "#")
            {
                clearMqttCredentials = true;
            }
            else
            {
                _preferences->putString(preference_mqtt_user, value);
                configChanged = true;
            }
        }
        else if(key == "MQTTPASS")
        {
            if(value != "*")
            {
                _preferences->putString(preference_mqtt_password, value);
                configChanged = true;
            }
        }
        else if(key == "MQTTPATH")
        {
            _preferences->putString(preference_mqtt_lock_path, value);
            configChanged = true;
        }
        else if(key == "MQTTOPPATH")
        {
            _preferences->putString(preference_mqtt_opener_path, value);
            configChanged = true;
        }
        else if(key == "MQTTCA")
        {
            _preferences->putString(preference_mqtt_ca, value);
            configChanged = true;
        }
        else if(key == "MQTTCRT")
        {
            _preferences->putString(preference_mqtt_crt, value);
            configChanged = true;
        }
        else if(key == "MQTTKEY")
        {
            _preferences->putString(preference_mqtt_key, value);
            configChanged = true;
        }
        else if(key == "NWHW")
        {
            _preferences->putInt(preference_network_hardware, value.toInt());
            configChanged = true;
        }
        else if(key == "NWHWWIFIFB")
        {
            _preferences->putBool(preference_network_wifi_fallback_disabled, (value == "1"));
            configChanged = true;
        }
        else if(key == "RSSI")
        {
            _preferences->putInt(preference_rssi_publish_interval, value.toInt());
            configChanged = true;
        }
        else if(key == "HASSDISCOVERY")
        {
            if(_preferences->getString(preference_mqtt_hass_discovery) != value)
            {
                // Previous HASS config has to be disabled first (remove retained MQTT messages)
                if (_nuki != nullptr)
                {
                    _nuki->disableHASS();
                }
                if (_nukiOpener != nullptr)
                {
                    _nukiOpener->disableHASS();
                }
                _preferences->putString(preference_mqtt_hass_discovery, value);
                configChanged = true;
            }
        }
        else if(key == "OPENERCONT")
        {
            _preferences->putBool(preference_opener_continuous_mode, (value == "1"));
            configChanged = true;
        }
        else if(key == "HASSCUURL")
        {
            _preferences->putString(preference_mqtt_hass_cu_url, value);
            configChanged = true;
        }
        else if(key == "BESTRSSI")
        {
            _preferences->putBool(preference_find_best_rssi, (value == "1"));
            configChanged = true;
        }
        else if(key == "HOSTNAME")
        {
            _preferences->putString(preference_hostname, value);
            configChanged = true;
        }
        else if(key == "NETTIMEOUT")
        {
            _preferences->putInt(preference_network_timeout, value.toInt());
            configChanged = true;
        }
        else if(key == "RSTDISC")
        {
            _preferences->putBool(preference_restart_on_disconnect, (value == "1"));
            configChanged = true;
        }
        else if(key == "RECNWTMQTTDIS")
        {
            _preferences->putBool(preference_recon_netw_on_mqtt_discon, (value == "1"));
            configChanged = true;
        }
        else if(key == "MQTTLOG")
        {
            _preferences->putBool(preference_mqtt_log_enabled, (value == "1"));
            configChanged = true;
        }
        else if(key == "WEBLOG")
        {
            _preferences->putBool(preference_webserial_enabled, (value == "1"));
            configChanged = true;
        }
        else if(key == "CHECKUPDATE")
        {
            _preferences->putBool(preference_check_updates, (value == "1"));
            configChanged = true;
        }
        else if(key == "UPDATEMQTT")
        {
            _preferences->putBool(preference_update_from_mqtt, (value == "1"));
            configChanged = true;
        }
        else if(key == "OFFHYBRID")
        {
            _preferences->putBool(preference_official_hybrid, (value == "1"));
            if((value == "1")) _preferences->putBool(preference_register_as_app, true);
            configChanged = true;
        }
        else if(key == "HYBRIDACT")
        {
            _preferences->putBool(preference_official_hybrid_actions, (value == "1"));
            if(value == "1") _preferences->putBool(preference_register_as_app, true);
            configChanged = true;
        }
        else if(key == "HYBRIDTIMER")
        {
            _preferences->putInt(preference_query_interval_hybrid_lockstate, value.toInt());
            configChanged = true;
        }
        else if(key == "HYBRIDRETRY")
        {
            _preferences->putBool(preference_official_hybrid_retry, (value == "1"));
            configChanged = true;
        }
        else if(key == "DISNONJSON")
        {
            _preferences->putBool(preference_disable_non_json, (value == "1"));
            configChanged = true;
        }
        else if(key == "DHCPENA")
        {
            _preferences->putBool(preference_ip_dhcp_enabled, (value == "1"));
            configChanged = true;
        }
        else if(key == "IPADDR")
        {
            _preferences->putString(preference_ip_address, value);
            configChanged = true;
        }
        else if(key == "IPSUB")
        {
            _preferences->putString(preference_ip_subnet, value);
            configChanged = true;
        }
        else if(key == "IPGTW")
        {
            _preferences->putString(preference_ip_gateway, value);
            configChanged = true;
        }
        else if(key == "DNSSRV")
        {
            _preferences->putString(preference_ip_dns_server, value);
            configChanged = true;
        }
        else if(key == "LSTINT")
        {
            _preferences->putInt(preference_query_interval_lockstate, value.toInt());
            configChanged = true;
        }
        else if(key == "CFGINT")
        {
            _preferences->putInt(preference_query_interval_configuration, value.toInt());
            configChanged = true;
        }
        else if(key == "BATINT")
        {
            _preferences->putInt(preference_query_interval_battery, value.toInt());
            configChanged = true;
        }
        else if(key == "KPINT")
        {
            _preferences->putInt(preference_query_interval_keypad, value.toInt());
            configChanged = true;
        }
        else if(key == "NRTRY")
        {
            _preferences->putInt(preference_command_nr_of_retries, value.toInt());
            configChanged = true;
        }
        else if(key == "TRYDLY")
        {
            _preferences->putInt(preference_command_retry_delay, value.toInt());
            configChanged = true;
        }
        else if(key == "TXPWR")
        {
            if(value.toInt() >= -12 && value.toInt() <= 9)
            {
                _preferences->putInt(preference_ble_tx_power, value.toInt());
                configChanged = true;
            }
        }
        #if PRESENCE_DETECTION_ENABLED
        else if(key == "PRDTMO")
        {
            _preferences->putInt(preference_presence_detection_timeout, value.toInt());
            configChanged = true;
        }
        #endif
        else if(key == "RSBC")
        {
            _preferences->putInt(preference_restart_ble_beacon_lost, value.toInt());
            configChanged = true;
        }
        else if(key == "TSKNTWK")
        {
            if(value.toInt() > 12287 && value.toInt() < 32769)
            {
            _preferences->putInt(preference_task_size_network, value.toInt());
            configChanged = true;
            }
        }
        else if(key == "TSKNUKI")
        {
            if(value.toInt() > 8191 && value.toInt() < 32769)
            {
                _preferences->putInt(preference_task_size_nuki, value.toInt());
                configChanged = true;
            }
        }
        else if(key == "ALMAX")
        {
            if(value.toInt() > 0 && value.toInt() < 51)
            {
                _preferences->putInt(preference_authlog_max_entries, value.toInt());
                configChanged = true;
            }
        }
        else if(key == "KPMAX")
        {
            if(value.toInt() > 0 && value.toInt() < 101)
            {
                _preferences->putInt(preference_keypad_max_entries, value.toInt());
                configChanged = true;
            }
        }
        else if(key == "TCMAX")
        {
            if(value.toInt() > 0 && value.toInt() < 51)
            {
                _preferences->putInt(preference_timecontrol_max_entries, value.toInt());
                configChanged = true;
            }
        }
        else if(key == "BUFFSIZE")
        {
            if(value.toInt() > 4095 && value.toInt() < 32769)
            {
                _preferences->putInt(preference_buffer_size, value.toInt());
                configChanged = true;
            }
        }
        else if(key == "BTLPRST")
        {
            _preferences->putBool(preference_enable_bootloop_reset, (value == "1"));
            configChanged = true;
        }
        else if(key == "OTAUPD")
        {
            _preferences->putString(preference_ota_updater_url, value);
            configChanged = true;
        }
        else if(key == "OTAMAIN")
        {
            _preferences->putString(preference_ota_main_url, value);
            configChanged = true;
        }
        else if(key == "SHOWSECRETS")
        {
            _preferences->putBool(preference_show_secrets, (value == "1"));
            configChanged = true;
        }
        else if(key == "ACLLVLCHANGED")
        {
            aclLvlChanged = true;
        }
        else if(key == "CONFPUB")
        {
            _preferences->putBool(preference_conf_info_enabled, (value == "1"));
            configChanged = true;
        }
        else if(key == "KPPUB")
        {
            _preferences->putBool(preference_keypad_info_enabled, (value == "1"));
            configChanged = true;
        }
        else if(key == "KPCODE")
        {
            _preferences->putBool(preference_keypad_publish_code, (value == "1"));
            configChanged = true;
        }
        else if(key == "KPENA")
        {
            _preferences->putBool(preference_keypad_control_enabled, (value == "1"));
            configChanged = true;
        }
        else if(key == "TCPUB")
        {
            _preferences->putBool(preference_timecontrol_info_enabled, (value == "1"));
            configChanged = true;
        }
        else if(key == "KPPER")
        {
            _preferences->putBool(preference_keypad_topic_per_entry, (value == "1"));
            configChanged = true;
        }
        else if(key == "TCPER")
        {
            _preferences->putBool(preference_timecontrol_topic_per_entry, (value == "1"));
            configChanged = true;
        }
        else if(key == "TCENA")
        {
            _preferences->putBool(preference_timecontrol_control_enabled, (value == "1"));
            configChanged = true;
        }
        else if(key == "PUBAUTH")
        {
            _preferences->putBool(preference_publish_authdata, (value == "1"));
            configChanged = true;
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
            _preferences->putBool(preference_register_as_app, (value == "1"));
            configChanged = true;
        }
        else if(key == "REGAPPOPN")
        {
            _preferences->putBool(preference_register_opener_as_app, (value == "1"));
            configChanged = true;
        }
        else if(key == "LOCKENA")
        {
            _preferences->putBool(preference_lock_enabled, (value == "1"));
            configChanged = true;
        }
        else if(key == "OPENA")
        {
            _preferences->putBool(preference_opener_enabled, (value == "1"));
            configChanged = true;
        }
        else if(key == "CREDUSER")
        {
            if(value == "#")
            {
                clearCredentials = true;
            }
            else
            {
                _preferences->putString(preference_cred_user, value);
                configChanged = true;
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
            }
            else
            {
                message = "Nuki Lock PIN saved";
                _nuki->setPin(value.toInt());
            }
            configChanged = true;
        }
        else if(key == "NUKIOPPIN" && _nukiOpener != nullptr)
        {
            if(value == "#")
            {
                message = "Nuki Opener PIN cleared";
                _nukiOpener->setPin(0xffff);
            }
            else
            {
                message = "Nuki Opener PIN saved";
                _nukiOpener->setPin(value.toInt());
            }
            configChanged = true;
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
            if(value.length() == 12) for(int i=0; i<value.length();i+=2) currentBleAddress[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
        }
        else if(key == "LCKSECRETK")
        {
            if(value.length() == 64) for(int i=0; i<value.length();i+=2) secretKeyK[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
        }
        else if(key == "LCKAUTHID")
        {
            if(value.length() == 8) for(int i=0; i<value.length();i+=2) authorizationId[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
        }
        else if(key == "OPNBLEADDR")
        {
            if(value.length() == 12) for(int i=0; i<value.length();i+=2) currentBleAddressOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
        }
        else if(key == "OPNSECRETK")
        {
            if(value.length() == 64) for(int i=0; i<value.length();i+=2) secretKeyKOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
        }
        else if(key == "OPNAUTHID")
        {
            if(value.length() == 8) for(int i=0; i<value.length();i+=2) authorizationIdOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
        }
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
        configChanged = true;
    }

    if(pass1 != "" && pass1 == pass2)
    {
        _preferences->putString(preference_cred_password, pass1);
        configChanged = true;
    }

    if(clearMqttCredentials)
    {
        _preferences->putString(preference_mqtt_user, "");
        _preferences->putString(preference_mqtt_password, "");
        configChanged = true;
    }

    if(clearCredentials)
    {
        _preferences->putString(preference_cred_user, "");
        _preferences->putString(preference_cred_password, "");
        configChanged = true;
    }

    if(aclLvlChanged)
    {
        _preferences->putBytes(preference_acl, (byte*)(&aclPrefs), sizeof(aclPrefs));
        _preferences->putBytes(preference_conf_lock_basic_acl, (byte*)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
        _preferences->putBytes(preference_conf_opener_basic_acl, (byte*)(&basicOpenerConfigAclPrefs), sizeof(basicOpenerConfigAclPrefs));
        _preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
        _preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));
        configChanged = true;
    }

    if(configChanged)
    {
        message = "Configuration saved ... restarting.";
        _enabled = false;
        _preferences->end();
    }

    return configChanged;
}

bool WebCfgServer::processImport(AsyncWebServerRequest *request, String& message)
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
        const AsyncWebParameter* p = request->getParam(index);
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
            const std::vector<char*> charPrefs = debugPreferences.getPreferencesCharKeys();
            const std::vector<char*> intPrefs = debugPreferences.getPreferencesIntKeys();

            for(const auto& key : keysPrefs)
            {
                if(doc[key].isNull()) continue;
                if(strcmp(key, preference_show_secrets) == 0) continue;
                if(strcmp(key, preference_latest_version) == 0) continue;
                if(strcmp(key, preference_has_mac_saved) == 0) continue;
                if(strcmp(key, preference_device_id_lock) == 0) continue;
                if(strcmp(key, preference_device_id_opener) == 0) continue;
                if(std::find(charPrefs.begin(), charPrefs.end(), key) != charPrefs.end()) continue;
                if(std::find(boolPrefs.begin(), boolPrefs.end(), key) != boolPrefs.end())
                {
                    if (doc[key].as<String>().length() > 0) _preferences->putBool(key, (doc[key].as<String>() == "1" ? true : false));
                    else _preferences->remove(key);
                    continue;
                }
                if(std::find(intPrefs.begin(), intPrefs.end(), key) != intPrefs.end())
                {
                    if (doc[key].as<String>().length() > 0) _preferences->putInt(key, doc[key].as<int>());
                    else _preferences->remove(key);
                    continue;
                }

                if (doc[key].as<String>().length() > 0) _preferences->putString(key, doc[key].as<String>());
                else _preferences->remove(key);
            }

            for(const auto& key : bytePrefs)
            {
                if(!doc[key].isNull() && doc[key].is<String>())
                {
                    String value = doc[key].as<String>();
                    unsigned char tmpchar[32];
                    for(int i=0; i<value.length();i+=2) tmpchar[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
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
                    for(int i=0; i<value.length();i+=2) currentBleAddress[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                    nukiBlePref.putBytes("bleAddress", currentBleAddress, 6);
                }
            }
            if(!doc["secretKeyKLock"].isNull())
            {
                if (doc["secretKeyKLock"].as<String>().length() == 64)
                {
                    String value = doc["secretKeyKLock"].as<String>();
                    for(int i=0; i<value.length();i+=2) secretKeyK[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                    nukiBlePref.putBytes("secretKeyK", secretKeyK, 32);
                }
            }
            if(!doc["authorizationIdLock"].isNull())
            {
                if (doc["authorizationIdLock"].as<String>().length() == 8)
                {
                    String value = doc["authorizationIdLock"].as<String>();
                    for(int i=0; i<value.length();i+=2) authorizationId[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                    nukiBlePref.putBytes("authorizationId", authorizationId, 4);
                }
            }
            nukiBlePref.end();
            if(!doc["securityPinCodeLock"].isNull())
            {
                if(doc["securityPinCodeLock"].as<String>().length() > 0) _nuki->setPin(doc["securityPinCodeLock"].as<int>());
                else _nuki->setPin(0xffff);
            }
            nukiBlePref.begin("NukiHubopener", false);
            if(!doc["bleAddressOpener"].isNull())
            {
                if (doc["bleAddressOpener"].as<String>().length() == 12)
                {
                    String value = doc["bleAddressOpener"].as<String>();
                    for(int i=0; i<value.length();i+=2) currentBleAddressOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                    nukiBlePref.putBytes("bleAddress", currentBleAddressOpn, 6);
                }
            }
            if(!doc["secretKeyKOpener"].isNull())
            {
                if (doc["secretKeyKOpener"].as<String>().length() == 64)
                {
                    String value = doc["secretKeyKOpener"].as<String>();
                    for(int i=0; i<value.length();i+=2) secretKeyKOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                    nukiBlePref.putBytes("secretKeyK", secretKeyKOpn, 32);
                }
            }
            if(!doc["authorizationIdOpener"].isNull())
            {
                if (doc["authorizationIdOpener"].as<String>().length() == 8)
                {
                    String value = doc["authorizationIdOpener"].as<String>();
                    for(int i=0; i<value.length();i+=2) authorizationIdOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
                    nukiBlePref.putBytes("authorizationId", authorizationIdOpn, 4);
                }
            }
            nukiBlePref.end();
            if(!doc["securityPinCodeOpener"].isNull())
            {
                if(doc["securityPinCodeOpener"].as<String>().length() > 0) _nukiOpener->setPin(doc["securityPinCodeOpener"].as<int>());
                else _nukiOpener->setPin(0xffff);
            }

            configChanged = true;
        }
    }

    if(configChanged)
    {
        message = "Configuration saved ... restarting.";
        _enabled = false;
        _preferences->end();
    }

    return configChanged;
}

void WebCfgServer::processGpioArgs(AsyncWebServerRequest *request)
{
    int params = request->params();
    std::vector<PinEntry> pinConfiguration;

    for(int index = 0; index < params; index++)
    {
        const AsyncWebParameter* p = request->getParam(index);
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

void WebCfgServer::buildImportExportHtml(AsyncWebServerRequest *request)
{
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    buildHtmlHeader(response);

    response->print("<div id=\"upform\"><h4>Import configuration</h4>");
    response->print("<form method=\"post\" action=\"import\"><textarea id=\"importjson\" name=\"importjson\" rows=\"10\" cols=\"50\"></textarea><br/>");
    response->print("<br><input type=\"submit\" name=\"submit\" value=\"Import\"></form><br><br></div>");
    response->print("<div id=\"gitdiv\">");
    response->print("<h4>Export configuration</h4><br>");
    response->print("<button title=\"Basic export\" onclick=\" window.open('/export', '_self'); return false;\">Basic export</button>");
    response->print("<br><br><button title=\"Export with redacted settings\" onclick=\" window.open('/export?redacted=1'); return false;\">Export with redacted settings</button>");
    response->print("<br><br><button title=\"Export with redacted settings and pairing data\" onclick=\" window.open('/export?redacted=1&pairing=1'); return false;\">Export with redacted settings and pairing data</button>");
    response->print("</div><div id=\"msgdiv\" style=\"visibility:hidden\">Initiating config update. Please be patient.<br>You will be forwarded automatically when the import is complete.</div>");
    response->print("<script type=\"text/javascript\">");
    response->print("window.addEventListener('load', function () {");
    response->print("	var button = document.getElementById(\"submitbtn\");");
    response->print("	button.addEventListener('click',hideshow,false);");
    response->print("	function hideshow() {");
    response->print("		document.getElementById('upform').style.visibility = 'hidden';");
    response->print("		document.getElementById('gitdiv').style.visibility = 'hidden';");
    response->print("		document.getElementById('msgdiv').style.visibility = 'visible';");
    response->print("	}");
    response->print("});");
    response->print("</script>");
    response->print("</body></html>");
    request->send(response);
}

void WebCfgServer::buildHtml(AsyncWebServerRequest *request)
{
    String header = "<script>let intervalId; window.onload = function() { updateInfo(); intervalId = setInterval(updateInfo, 3000); }; function updateInfo() { var request = new XMLHttpRequest(); request.open('GET', '/status', true); request.onload = () => { const obj = JSON.parse(request.responseText); if (obj.stop == 1) { clearInterval(intervalId); } for (var key of Object.keys(obj)) { if(key=='ota' && document.getElementById(key) !== null) { document.getElementById(key).innerText = \"<a href='/ota'>\" + obj[key] + \"</a>\"; } else if(document.getElementById(key) !== null) { document.getElementById(key).innerText = obj[key]; } } }; request.send(); }</script>";
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    buildHtmlHeader(response, header);

    response->print("<br><h3>Info</h3>\n");
    response->print("<table>");

    printParameter(response, "Hostname", _hostname.c_str(), "", "hostname");
    printParameter(response, "MQTT Connected", _network->mqttConnectionState() > 0 ? "Yes" : "No", "", "mqttState");
    if(_nuki != nullptr)
    {
        char lockStateArr[20];
        NukiLock::lockstateToString(_nuki->keyTurnerState().lockState, lockStateArr);
        printParameter(response, "Nuki Lock paired", _nuki->isPaired() ? ("Yes (BLE Address " + _nuki->getBleAddress().toString() + ")").c_str() : "No", "", "lockPaired");
        printParameter(response, "Nuki Lock state", lockStateArr, "", "lockState");

        if(_nuki->isPaired())
        {
            String lockState = pinStateToString(_preferences->getInt(preference_lock_pin_status, 4));
            printParameter(response, "Nuki Lock PIN status", lockState.c_str(), "", "lockPin");

            if(_preferences->getBool(preference_official_hybrid, false))
            {
                String offConnected = _nuki->offConnected() ? "Yes": "No";
                printParameter(response, "Nuki Lock hybrid mode connected", offConnected.c_str(), "", "lockHybrid");
            }
        }
    }
    if(_nukiOpener != nullptr)
    {
        char openerStateArr[20];
        NukiOpener::lockstateToString(_nukiOpener->keyTurnerState().lockState, openerStateArr);
        printParameter(response, "Nuki Opener paired", _nukiOpener->isPaired() ? ("Yes (BLE Address " + _nukiOpener->getBleAddress().toString() + ")").c_str() : "No", "", "openerPaired");

        if(_nukiOpener->keyTurnerState().nukiState == NukiOpener::State::ContinuousMode) printParameter(response, "Nuki Opener state", "Open (Continuous Mode)", "", "openerState");
        else printParameter(response, "Nuki Opener state", openerStateArr, "", "openerState");

        if(_nukiOpener->isPaired())
        {
            String openerState = pinStateToString(_preferences->getInt(preference_opener_pin_status, 4));
            printParameter(response, "Nuki Opener PIN status", openerState.c_str(), "", "openerPin");
        }
    }
    printParameter(response, "Firmware", NUKI_HUB_VERSION, "/info", "firmware");
    if(_preferences->getBool(preference_check_updates)) printParameter(response, "Latest Firmware", _preferences->getString(preference_latest_version).c_str(), "/ota", "ota");
    response->print("</table><br>");
    response->print("<ul id=\"tblnav\">");
    buildNavigationMenuEntry(response, "MQTT and Network Configuration", "/mqttconfig",  _brokerConfigured ? "" : "Please configure MQTT broker");
    buildNavigationMenuEntry(response, "Nuki Configuration", "/nukicfg");
    buildNavigationMenuEntry(response, "Access Level Configuration", "/acclvl");
    buildNavigationMenuEntry(response, "Credentials", "/cred", _pinsConfigured ? "" : "Please configure PIN");
    buildNavigationMenuEntry(response, "GPIO Configuration", "/gpiocfg");
    buildNavigationMenuEntry(response, "Firmware update", "/ota");
    buildNavigationMenuEntry(response, "Import/Export Configuration", "/impexpcfg");
    if(_preferences->getBool(preference_publish_debug_info, false))
    {
        buildNavigationMenuEntry(response, "Advanced Configuration", "/advanced");
    }
    if(_preferences->getBool(preference_webserial_enabled, false))
    {
        buildNavigationMenuEntry(response, "Open Webserial", "/webserial");
    }
    if(_allowRestartToPortal)
    {
        buildNavigationMenuEntry(response, "Configure Wi-Fi", "/wifi");
    }
    buildNavigationMenuEntry(response, "Reboot Nuki Hub", "/reboot");
    response->print("</ul></body></html>");
    request->send(response);
}


void WebCfgServer::buildCredHtml(AsyncWebServerRequest *request)
{
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    buildHtmlHeader(response);
    response->print("<form class=\"adapt\" method=\"post\" action=\"savecfg\">");
    response->print("<h3>Credentials</h3>");
    response->print("<table>");
    printInputField(response, "CREDUSER", "User (# to clear)", _preferences->getString(preference_cred_user).c_str(), 30, "", false, true);
    printInputField(response, "CREDPASS", "Password", "*", 30, "", true, true);
    printInputField(response, "CREDPASSRE", "Retype password", "*", 30, "", true);
    response->print("</table>");
    response->print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response->print("</form>");

    if(_nuki != nullptr)
    {
        response->print("<br><br><form class=\"adapt\" method=\"post\" action=\"savecfg\">");
        response->print("<h3>Nuki Lock PIN</h3>");
        response->print("<table>");
        printInputField(response, "NUKIPIN", "PIN Code (# to clear)", "*", 20, "", true);
        response->print("</table>");
        response->print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
        response->print("</form>");
    }

    if(_nukiOpener != nullptr)
    {
        response->print("<br><br><form class=\"adapt\" method=\"post\" action=\"savecfg\">");
        response->print("<h3>Nuki Opener PIN</h3>");
        response->print("<table>");
        printInputField(response, "NUKIOPPIN", "PIN Code (# to clear)", "*", 20, "", true);
        response->print("</table>");
        response->print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
        response->print("</form>");
    }

    _confirmCode = generateConfirmCode();
    if(_nuki != nullptr)
    {
        response->print("<br><br><h3>Unpair Nuki Lock</h3>");
        response->print("<form class=\"adapt\" method=\"post\" action=\"/unpairlock\">");
        response->print("<table>");
        String message = "Type ";
        message.concat(_confirmCode);
        message.concat(" to confirm unpair");
        printInputField(response, "CONFIRMTOKEN", message.c_str(), "", 10, "");
        response->print("</table>");
        response->print("<br><button type=\"submit\">OK</button></form>");
    }

    if(_nukiOpener != nullptr)
    {
        response->print("<br><br><h3>Unpair Nuki Opener</h3>");
        response->print("<form class=\"adapt\" method=\"post\" action=\"/unpairopener\">");
        response->print("<table>");
        String message = "Type ";
        message.concat(_confirmCode);
        message.concat(" to confirm unpair");
        printInputField(response, "CONFIRMTOKEN", message.c_str(), "", 10, "");
        response->print("</table>");
        response->print("<br><button type=\"submit\">OK</button></form>");
    }

    response->print("<br><br><h3>Factory reset Nuki Hub</h3>");
    response->print("<h4 class=\"warning\">This will reset all settings to default and unpair Nuki Lock and/or Opener. Optionally will also reset WiFi settings and reopen WiFi manager portal.</h4>");
    response->print("<form class=\"adapt\" method=\"post\" action=\"/factoryreset\">");
    response->print("<table>");
    String message = "Type ";
    message.concat(_confirmCode);
    message.concat(" to confirm factory reset");
    printInputField(response, "CONFIRMTOKEN", message.c_str(), "", 10, "");
    printCheckBox(response, "WIFI", "Also reset WiFi settings", false, "");
    response->print("</table>");
    response->print("<br><button type=\"submit\">OK</button></form>");
    response->print("</body></html>");
    request->send(response);
}

void WebCfgServer::buildMqttConfigHtml(AsyncWebServerRequest *request)
{
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    buildHtmlHeader(response);
    response->print("<form class=\"adapt\" method=\"post\" action=\"savecfg\">");
    response->print("<h3>Basic MQTT and Network Configuration</h3>");
    response->print("<table>");
    printInputField(response, "HOSTNAME", "Host name", _preferences->getString(preference_hostname).c_str(), 100, "");
    printInputField(response, "MQTTSERVER", "MQTT Broker", _preferences->getString(preference_mqtt_broker).c_str(), 100, "");
    printInputField(response, "MQTTPORT", "MQTT Broker port", _preferences->getInt(preference_mqtt_broker_port), 5, "");
    printInputField(response, "MQTTUSER", "MQTT User (# to clear)", _preferences->getString(preference_mqtt_user).c_str(), 30, "", false, true);
    printInputField(response, "MQTTPASS", "MQTT Password", "*", 30, "", true, true);
    response->print("</table><br>");

    response->print("<h3>Advanced MQTT and Network Configuration</h3>");
    response->print("<table>");
    printInputField(response, "HASSDISCOVERY", "Home Assistant discovery topic (empty to disable; usually homeassistant)", _preferences->getString(preference_mqtt_hass_discovery).c_str(), 30, "");
    printInputField(response, "HASSCUURL", "Home Assistant device configuration URL (empty to use http://LOCALIP; fill when using a reverse proxy for example)", _preferences->getString(preference_mqtt_hass_cu_url).c_str(), 261, "");
    if(_nukiOpener != nullptr) printCheckBox(response, "OPENERCONT", "Set Nuki Opener Lock/Unlock action in Home Assistant to Continuous mode", _preferences->getBool(preference_opener_continuous_mode), "");
    printTextarea(response, "MQTTCA", "MQTT SSL CA Certificate (*, optional)", _preferences->getString(preference_mqtt_ca).c_str(), TLS_CA_MAX_SIZE, _network->encryptionSupported(), true);
    printTextarea(response, "MQTTCRT", "MQTT SSL Client Certificate (*, optional)", _preferences->getString(preference_mqtt_crt).c_str(), TLS_CERT_MAX_SIZE, _network->encryptionSupported(), true);
    printTextarea(response, "MQTTKEY", "MQTT SSL Client Key (*, optional)", _preferences->getString(preference_mqtt_key).c_str(), TLS_KEY_MAX_SIZE, _network->encryptionSupported(), true);
    printDropDown(response, "NWHW", "Network hardware", String(_preferences->getInt(preference_network_hardware)), getNetworkDetectionOptions(), "");
    printCheckBox(response, "NWHWWIFIFB", "Disable fallback to Wi-Fi / Wi-Fi config portal", _preferences->getBool(preference_network_wifi_fallback_disabled), "");
    printCheckBox(response, "BESTRSSI", "Connect to AP with the best signal in an environment with multiple APs with the same SSID", _preferences->getBool(preference_find_best_rssi), "");
    printInputField(response, "RSSI", "RSSI Publish interval (seconds; -1 to disable)", _preferences->getInt(preference_rssi_publish_interval), 6, "");
    printInputField(response, "NETTIMEOUT", "MQTT Timeout until restart (seconds; -1 to disable)", _preferences->getInt(preference_network_timeout), 5, "");
    printCheckBox(response, "RSTDISC", "Restart on disconnect", _preferences->getBool(preference_restart_on_disconnect), "");
    printCheckBox(response, "RECNWTMQTTDIS", "Reconnect network on MQTT connection failure", _preferences->getBool(preference_recon_netw_on_mqtt_discon), "");
    printCheckBox(response, "MQTTLOG", "Enable MQTT logging", _preferences->getBool(preference_mqtt_log_enabled), "");
    printCheckBox(response, "WEBLOG", "Enable WebSerial logging", _preferences->getBool(preference_webserial_enabled), "");
    printCheckBox(response, "CHECKUPDATE", "Check for Firmware Updates every 24h", _preferences->getBool(preference_check_updates), "");
    printCheckBox(response, "UPDATEMQTT", "Allow updating using MQTT", _preferences->getBool(preference_update_from_mqtt), "");
    printCheckBox(response, "DISNONJSON", "Disable some extraneous non-JSON topics", _preferences->getBool(preference_disable_non_json), "");
    printCheckBox(response, "OFFHYBRID", "Enable hybrid official MQTT and Nuki Hub setup", _preferences->getBool(preference_official_hybrid), "");
    printCheckBox(response, "HYBRIDACT", "Enable sending actions through official MQTT", _preferences->getBool(preference_official_hybrid_actions), "");
    printInputField(response, "HYBRIDTIMER", "Time between status updates when official MQTT is offline (seconds)", _preferences->getInt(preference_query_interval_hybrid_lockstate), 5, "");
    printCheckBox(response, "HYBRIDRETRY", "Retry command sent using official MQTT over BLE if failed", _preferences->getBool(preference_official_hybrid_retry), "");
    response->print("</table>");
    response->print("* If no encryption is configured for the MQTT broker, leave empty. Only supported for Wi-Fi connections.<br><br>");

    response->print("<h3>IP Address assignment</h3>");
    response->print("<table>");
    printCheckBox(response, "DHCPENA", "Enable DHCP", _preferences->getBool(preference_ip_dhcp_enabled), "");
    printInputField(response, "IPADDR", "Static IP address", _preferences->getString(preference_ip_address).c_str(), 15, "");
    printInputField(response, "IPSUB", "Subnet", _preferences->getString(preference_ip_subnet).c_str(), 15, "");
    printInputField(response, "IPGTW", "Default gateway", _preferences->getString(preference_ip_gateway).c_str(), 15, "");
    printInputField(response, "DNSSRV", "DNS Server", _preferences->getString(preference_ip_dns_server).c_str(), 15, "");
    response->print("</table>");

    response->print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response->print("</form>");
    response->print("</body></html>");
    request->send(response);
}

void WebCfgServer::buildAdvancedConfigHtml(AsyncWebServerRequest *request)
{
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    buildHtmlHeader(response);
    response->print("<form class=\"adapt\" method=\"post\" action=\"savecfg\">");
    response->print("<h3>Advanced Configuration</h3>");
    response->print("<h4 class=\"warning\">Warning: Changing these settings can lead to bootloops that might require you to erase the ESP32 and reflash nukihub using USB/serial</h4>");
    response->print("<table>");
    response->print("<tr><td>Current bootloop prevention state</td><td>");
    response->print(_preferences->getBool(preference_enable_bootloop_reset, false) ? "Enabled" : "Disabled");
    response->print("</td></tr>");
    printCheckBox(response, "BTLPRST", "Enable Bootloop prevention (Try to reset these settings to default on bootloop)", true, "");
    printInputField(response, "BUFFSIZE", "Char buffer size (min 4096, max 32768)", _preferences->getInt(preference_buffer_size, CHAR_BUFFER_SIZE), 6, "");
    response->print("<tr><td>Advised minimum char buffer size based on current settings</td><td id=\"mincharbuffer\"></td>");
    printInputField(response, "TSKNTWK", "Task size Network (min 12288, max 32768)", _preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE), 6, "");
    response->print("<tr><td>Advised minimum network task size based on current settings</td><td id=\"minnetworktask\"></td>");
    printInputField(response, "TSKNUKI", "Task size Nuki (min 8192, max 32768)", _preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE), 6, "");
    printInputField(response, "ALMAX", "Max auth log entries (min 1, max 50)", _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG), 3, "inputmaxauthlog");
    printInputField(response, "KPMAX", "Max keypad entries (min 1, max 100)", _preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD), 3, "inputmaxkeypad");
    printInputField(response, "TCMAX", "Max timecontrol entries (min 1, max 50)", _preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL), 3, "inputmaxtimecontrol");
    printCheckBox(response, "SHOWSECRETS", "Show Pairing secrets on Info page (for 120s after next boot)", _preferences->getBool(preference_show_secrets), "");

    if(_nuki != nullptr)
    {
        printCheckBox(response, "LCKMANPAIR", "Manually set lock pairing data (enable to save values below)", false, "");
        printInputField(response, "LCKBLEADDR", "currentBleAddress", "", 12, "");
        printInputField(response, "LCKSECRETK", "secretKeyK", "", 64, "");
        printInputField(response, "LCKAUTHID", "authorizationId", "", 8, "");
    }
    if(_nukiOpener != nullptr)
    {
        printCheckBox(response, "OPNMANPAIR", "Manually set opener pairing data (enable to save values below)", false, "");
        printInputField(response, "OPNBLEADDR", "currentBleAddress", "", 12, "");
        printInputField(response, "OPNSECRETK", "secretKeyK", "", 64, "");
        printInputField(response, "OPNAUTHID", "authorizationId", "", 8, "");
    }
    printInputField(response, "OTAUPD", "Custom URL to update Nuki Hub updater", "", 255, "");
    printInputField(response, "OTAMAIN", "Custom URL to update Nuki Hub", "", 255, "");
    response->print("</table>");

    response->print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response->print("</form>");
    response->print("</body><script>window.onload=function(){ document.getElementById(\"inputmaxauthlog\").addEventListener(\"keyup\", calculate);document.getElementById(\"inputmaxkeypad\").addEventListener(\"keyup\", calculate);document.getElementById(\"inputmaxtimecontrol\").addEventListener(\"keyup\", calculate); calculate(); }; function calculate() { var authlog = document.getElementById(\"inputmaxauthlog\").value; var keypad = document.getElementById(\"inputmaxkeypad\").value; var timecontrol = document.getElementById(\"inputmaxtimecontrol\").value; var charbuf = 0; var networktask = 0; var sizeauthlog = 0; var sizekeypad = 0; var sizetimecontrol = 0; if(authlog > 0) { sizeauthlog = 280 * authlog; } if(keypad > 0) { sizekeypad = 350 * keypad; } if(timecontrol > 0) { sizetimecontrol = 120 * timecontrol; } charbuf = sizetimecontrol; networktask = 10240 + sizetimecontrol; if(sizeauthlog>sizekeypad && sizeauthlog>sizetimecontrol) { charbuf = sizeauthlog; networktask = 10240 + sizeauthlog;} else if(sizekeypad>sizeauthlog && sizekeypad>sizetimecontrol) { charbuf = sizekeypad; networktask = 10240 + sizekeypad;} if(charbuf<4096) { charbuf = 4096; } else if (charbuf>32768) { charbuf = 32768; } if(networktask<12288) { networktask = 12288; } else if (networktask>32768) { networktask = 32768; } document.getElementById(\"mincharbuffer\").innerHTML = charbuf; document.getElementById(\"minnetworktask\").innerHTML = networktask; }</script></html>");
    request->send(response);
}

void WebCfgServer::buildStatusHtml(AsyncWebServerRequest *request)
{
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    JsonDocument json;
    char _resbuf[2048];
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
    else json["mqttState"] = "No";

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
            if(strcmp(lockStateArr, "undefined") != 0) lockDone = true;
        }
        else json["lockPin"] = "Not Paired";
    }
    else lockDone = true;
    if(_nukiOpener != nullptr)
    {
        char openerStateArr[20];
        NukiOpener::lockstateToString(_nukiOpener->keyTurnerState().lockState, openerStateArr);
        String openerState = openerStateArr;
        String openerPaired = (_nukiOpener->isPaired() ? ("Yes (BLE Address " + _nukiOpener->getBleAddress().toString() + ")").c_str() : "No");
        json["openerPaired"] = openerPaired;

        if(_nukiOpener->keyTurnerState().nukiState == NukiOpener::State::ContinuousMode) json["openerState"] = "Open (Continuous Mode)";
        else json["openerState"] = openerState;

        if(_nukiOpener->isPaired())
        {
            json["openerPin"] = pinStateToString(_preferences->getInt(preference_opener_pin_status, 4));
            if(strcmp(openerStateArr, "undefined") != 0) openerDone = true;
        }
        else json["openerPin"] = "Not Paired";
    }
    else openerDone = true;

    if(_preferences->getBool(preference_check_updates))
    {
        json["latestFirmware"] = _preferences->getString(preference_latest_version);
        latestDone = true;
    }
    else latestDone = true;

    if(mqttDone && lockDone && openerDone && latestDone) json["stop"] = 1;

    serializeJson(json, _resbuf, sizeof(_resbuf));
    response->print(_resbuf);
    request->send(response);
}

String WebCfgServer::pinStateToString(uint8_t value) {
    switch(value)
    {
        case 0:
            return (String)"PIN not set";
        case 1:
            return (String)"PIN valid";
        case 2:
            return (String)"PIN set but invalid";;
        default:
            return (String)"Unknown";
    }
}

void WebCfgServer::buildAccLvlHtml(AsyncWebServerRequest *request, int aclPart)
{
    String partString = "";
    AsyncResponseStream *response;

    if(aclPart == 0)
    {
        response = request->beginResponseStream("text/html");
        buildHtmlHeader(response);
    }
    else response = request->beginResponseStream("text/plain");

    partAccLvlHtml(partString, aclPart);
    response->print(partString);
    request->send(response);
}

void WebCfgServer::partAccLvlHtml(String &partString, int aclPart)
{
    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    switch(aclPart)
    {
        case 0:
            partString.concat("<form method=\"post\" action=\"savecfg\">");
            partString.concat("<input type=\"hidden\" name=\"ACLLVLCHANGED\" value=\"1\">");
            partString.concat("<h3>Nuki General Access Control</h3>");
            partString.concat("<table><tr><th>Setting</th><th>Enabled</th></tr>");
            printCheckBox(partString, "CONFPUB", "Publish Nuki configuration information", _preferences->getBool(preference_conf_info_enabled, true), "");

            if((_nuki != nullptr && _nuki->hasKeypad()) || (_nukiOpener != nullptr && _nukiOpener->hasKeypad()))
            {
                printCheckBox(partString, "KPPUB", "Publish keypad entries information", _preferences->getBool(preference_keypad_info_enabled), "");
                printCheckBox(partString, "KPPER", "Publish a topic per keypad entry and create HA sensor", _preferences->getBool(preference_keypad_topic_per_entry), "");
                printCheckBox(partString, "KPCODE", "Also publish keypad codes (<span class=\"warning\">Disadvised for security reasons</span>)", _preferences->getBool(preference_keypad_publish_code, false), "");
                printCheckBox(partString, "KPENA", "Add, modify and delete keypad codes", _preferences->getBool(preference_keypad_control_enabled), "");
            }
            printCheckBox(partString, "TCPUB", "Publish time control entries information", _preferences->getBool(preference_timecontrol_info_enabled), "");
            printCheckBox(partString, "TCPER", "Publish a topic per time control entry and create HA sensor", _preferences->getBool(preference_timecontrol_topic_per_entry), "");
            printCheckBox(partString, "TCENA", "Add, modify and delete time control entries", _preferences->getBool(preference_timecontrol_control_enabled), "");
            printCheckBox(partString, "PUBAUTH", "Publish authorization log", _preferences->getBool(preference_publish_authdata), "");
            partString.concat("</table><br>");
            partString.concat("<div id=\"acllock\"></div>");
            partString.concat("<div id=\"aclopener\"></div>");
            partString.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
            partString.concat("</form>");
            partString.concat("<script>window.onload = function() { ");
            if(_nuki != nullptr) partString.concat("updateNuki(\"acllock\"); ");
            if(_nukiOpener != nullptr) partString.concat("updateNuki(\"aclopener\"); ");
            partString.concat("}; function updateNuki(type) { var request = new XMLHttpRequest(); request.open('GET', \"/\" + type, true); request.onload = () => { if (document.getElementById(type) !== null) { document.getElementById(type).innerHTML = request.responseText; } }; request.send(); }</script>");
            partString.concat("</body></html>");
            break;
        case 1:
            uint32_t basicLockConfigAclPrefs[16];
            _preferences->getBytes(preference_conf_lock_basic_acl, &basicLockConfigAclPrefs, sizeof(basicLockConfigAclPrefs));
            uint32_t advancedLockConfigAclPrefs[22];
            _preferences->getBytes(preference_conf_lock_advanced_acl, &advancedLockConfigAclPrefs, sizeof(advancedLockConfigAclPrefs));

            partString.concat("<h3>Nuki Lock Access Control</h3>");
            partString.concat("<input type=\"button\" value=\"Allow all\" style=\"margin-right: 10px;\" onclick=\"");
            partString.concat("for(el of document.getElementsByClassName('chk_access_lock')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=true;}\">");
            partString.concat("<input type=\"button\" value=\"Disallow all\" onclick=\"");
            partString.concat("for(el of document.getElementsByClassName('chk_access_lock')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=false;}\">");
            partString.concat("<table><tr><th>Action</th><th>Allowed</th></tr>");

            printCheckBox(partString, "ACLLCKLCK", "Lock", ((int)aclPrefs[0] == 1), "chk_access_lock");
            printCheckBox(partString, "ACLLCKUNLCK", "Unlock", ((int)aclPrefs[1] == 1), "chk_access_lock");
            printCheckBox(partString, "ACLLCKUNLTCH", "Unlatch", ((int)aclPrefs[2] == 1), "chk_access_lock");
            printCheckBox(partString, "ACLLCKLNG", "Lock N Go", ((int)aclPrefs[3] == 1), "chk_access_lock");
            printCheckBox(partString, "ACLLCKLNGU", "Lock N Go Unlatch", ((int)aclPrefs[4] == 1), "chk_access_lock");
            printCheckBox(partString, "ACLLCKFLLCK", "Full Lock", ((int)aclPrefs[5] == 1), "chk_access_lock");
            printCheckBox(partString, "ACLLCKFOB1", "Fob Action 1", ((int)aclPrefs[6] == 1), "chk_access_lock");
            printCheckBox(partString, "ACLLCKFOB2", "Fob Action 2", ((int)aclPrefs[7] == 1), "chk_access_lock");
            printCheckBox(partString, "ACLLCKFOB3", "Fob Action 3", ((int)aclPrefs[8] == 1), "chk_access_lock");
            partString.concat("</table><br>");

            partString.concat("<h3>Nuki Lock Config Control (Requires PIN to be set)</h3>");
            partString.concat("<input type=\"button\" value=\"Allow all\" style=\"margin-right: 10px;\" onclick=\"");
            partString.concat("for(el of document.getElementsByClassName('chk_config_lock')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=true;}\">");
            partString.concat("<input type=\"button\" value=\"Disallow all\" onclick=\"");
            partString.concat("for(el of document.getElementsByClassName('chk_config_lock')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=false;}\">");
            partString.concat("<table><tr><th>Change</th><th>Allowed</th></tr>");

            printCheckBox(partString, "CONFLCKNAME", "Name", ((int)basicLockConfigAclPrefs[0] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKLAT", "Latitude", ((int)basicLockConfigAclPrefs[1] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKLONG", "Longitude", ((int)basicLockConfigAclPrefs[2] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKAUNL", "Auto unlatch", ((int)basicLockConfigAclPrefs[3] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKPRENA", "Pairing enabled", ((int)basicLockConfigAclPrefs[4] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKBTENA", "Button enabled", ((int)basicLockConfigAclPrefs[5] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKLEDENA", "LED flash enabled", ((int)basicLockConfigAclPrefs[6] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKLEDBR", "LED brightness", ((int)basicLockConfigAclPrefs[7] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKTZOFF", "Timezone offset", ((int)basicLockConfigAclPrefs[8] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKDSTM", "DST mode", ((int)basicLockConfigAclPrefs[9] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKFOB1", "Fob Action 1", ((int)basicLockConfigAclPrefs[10] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKFOB2", "Fob Action 2", ((int)basicLockConfigAclPrefs[11] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKFOB3", "Fob Action 3", ((int)basicLockConfigAclPrefs[12] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKSGLLCK", "Single Lock", ((int)basicLockConfigAclPrefs[13] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKADVM", "Advertising Mode", ((int)basicLockConfigAclPrefs[14] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKTZID", "Timezone ID", ((int)basicLockConfigAclPrefs[15] == 1), "chk_config_lock");

            printCheckBox(partString, "CONFLCKUPOD", "Unlocked Position Offset Degrees", ((int)advancedLockConfigAclPrefs[0] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKLPOD", "Locked Position Offset Degrees", ((int)advancedLockConfigAclPrefs[1] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKSLPOD", "Single Locked Position Offset Degrees", ((int)advancedLockConfigAclPrefs[2] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKUTLTOD", "Unlocked To Locked Transition Offset Degrees", ((int)advancedLockConfigAclPrefs[3] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKLNGT", "Lock n Go timeout", ((int)advancedLockConfigAclPrefs[4] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKSBPA", "Single button press action", ((int)advancedLockConfigAclPrefs[5] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKDBPA", "Double button press action", ((int)advancedLockConfigAclPrefs[6] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKDC", "Detached cylinder", ((int)advancedLockConfigAclPrefs[7] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKBATT", "Battery type", ((int)advancedLockConfigAclPrefs[8] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKABTD", "Automatic battery type detection", ((int)advancedLockConfigAclPrefs[9] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKUNLD", "Unlatch duration", ((int)advancedLockConfigAclPrefs[10] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKALT", "Auto lock timeout", ((int)advancedLockConfigAclPrefs[11] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKAUNLD", "Auto unlock disabled", ((int)advancedLockConfigAclPrefs[12] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKNMENA", "Nightmode enabled", ((int)advancedLockConfigAclPrefs[13] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKNMST", "Nightmode start time", ((int)advancedLockConfigAclPrefs[14] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKNMET", "Nightmode end time", ((int)advancedLockConfigAclPrefs[15] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKNMALENA", "Nightmode auto lock enabled", ((int)advancedLockConfigAclPrefs[16] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKNMAULD", "Nightmode auto unlock disabled", ((int)advancedLockConfigAclPrefs[17] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKNMLOS", "Nightmode immediate lock on start", ((int)advancedLockConfigAclPrefs[18] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKALENA", "Auto lock enabled", ((int)advancedLockConfigAclPrefs[19] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKIALENA", "Immediate auto lock enabled", ((int)advancedLockConfigAclPrefs[20] == 1), "chk_config_lock");
            printCheckBox(partString, "CONFLCKAUENA", "Auto update enabled", ((int)advancedLockConfigAclPrefs[21] == 1), "chk_config_lock");
            partString.concat("</table><br>");
            break;
        case 2:
            uint32_t basicOpenerConfigAclPrefs[14];
            _preferences->getBytes(preference_conf_opener_basic_acl, &basicOpenerConfigAclPrefs, sizeof(basicOpenerConfigAclPrefs));
            uint32_t advancedOpenerConfigAclPrefs[20];
            _preferences->getBytes(preference_conf_opener_advanced_acl, &advancedOpenerConfigAclPrefs, sizeof(advancedOpenerConfigAclPrefs));

            partString.concat("<h3>Nuki Opener Access Control</h3>");
            partString.concat("<input type=\"button\" value=\"Allow all\" style=\"margin-right: 10px;\" onclick=\"");
            partString.concat("for(el of document.getElementsByClassName('chk_access_opener')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=true;}\">");
            partString.concat("<input type=\"button\" value=\"Disallow all\" onclick=\"");
            partString.concat("for(el of document.getElementsByClassName('chk_access_opener')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=false;}\">");
            partString.concat("<table><tr><th>Action</th><th>Allowed</th></tr>");

            printCheckBox(partString, "ACLOPNUNLCK", "Activate Ring-to-Open", ((int)aclPrefs[9] == 1), "chk_access_opener");
            printCheckBox(partString, "ACLOPNLCK", "Deactivate Ring-to-Open", ((int)aclPrefs[10] == 1), "chk_access_opener");
            printCheckBox(partString, "ACLOPNUNLTCH", "Electric Strike Actuation", ((int)aclPrefs[11] == 1), "chk_access_opener");
            printCheckBox(partString, "ACLOPNUNLCKCM", "Activate Continuous Mode", ((int)aclPrefs[12] == 1), "chk_access_opener");
            printCheckBox(partString, "ACLOPNLCKCM", "Deactivate Continuous Mode", ((int)aclPrefs[13] == 1), "chk_access_opener");
            printCheckBox(partString, "ACLOPNFOB1", "Fob Action 1", ((int)aclPrefs[14] == 1), "chk_access_opener");
            printCheckBox(partString, "ACLOPNFOB2", "Fob Action 2", ((int)aclPrefs[15] == 1), "chk_access_opener");
            printCheckBox(partString, "ACLOPNFOB3", "Fob Action 3", ((int)aclPrefs[16] == 1), "chk_access_opener");
            partString.concat("</table><br>");

            partString.concat("<h3>Nuki Opener Config Control (Requires PIN to be set)</h3>");
            partString.concat("<input type=\"button\" value=\"Allow all\" style=\"margin-right: 10px;\" onclick=\"");
            partString.concat("for(el of document.getElementsByClassName('chk_config_opener')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=true;}\">");
            partString.concat("<input type=\"button\" value=\"Disallow all\" onclick=\"");
            partString.concat("for(el of document.getElementsByClassName('chk_config_opener')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=false;}\">");
            partString.concat("<table><tr><th>Change</th><th>Allowed</th></tr>");

            printCheckBox(partString, "CONFOPNNAME", "Name", ((int)basicOpenerConfigAclPrefs[0] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNLAT", "Latitude", ((int)basicOpenerConfigAclPrefs[1] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNLONG", "Longitude", ((int)basicOpenerConfigAclPrefs[2] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNPRENA", "Pairing enabled", ((int)basicOpenerConfigAclPrefs[3] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNBTENA", "Button enabled", ((int)basicOpenerConfigAclPrefs[4] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNLEDENA", "LED flash enabled", ((int)basicOpenerConfigAclPrefs[5] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNTZOFF", "Timezone offset", ((int)basicOpenerConfigAclPrefs[6] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNDSTM", "DST mode", ((int)basicOpenerConfigAclPrefs[7] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNFOB1", "Fob Action 1", ((int)basicOpenerConfigAclPrefs[8] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNFOB2", "Fob Action 2", ((int)basicOpenerConfigAclPrefs[9] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNFOB3", "Fob Action 3", ((int)basicOpenerConfigAclPrefs[10] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNOPM", "Operating Mode", ((int)basicOpenerConfigAclPrefs[11] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNADVM", "Advertising Mode", ((int)basicOpenerConfigAclPrefs[12] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNTZID", "Timezone ID", ((int)basicOpenerConfigAclPrefs[13] == 1), "chk_config_opener");

            printCheckBox(partString, "CONFOPNICID", "Intercom ID", ((int)advancedOpenerConfigAclPrefs[0] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNBUSMS", "BUS mode Switch", ((int)advancedOpenerConfigAclPrefs[1] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNSCDUR", "Short Circuit Duration", ((int)advancedOpenerConfigAclPrefs[2] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNESD", "Eletric Strike Delay", ((int)advancedOpenerConfigAclPrefs[3] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNRESD", "Random Electric Strike Delay", ((int)advancedOpenerConfigAclPrefs[4] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNESDUR", "Electric Strike Duration", ((int)advancedOpenerConfigAclPrefs[5] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNDRTOAR", "Disable RTO after ring", ((int)advancedOpenerConfigAclPrefs[6] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNRTOT", "RTO timeout", ((int)advancedOpenerConfigAclPrefs[7] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNDRBSUP", "Doorbell suppression", ((int)advancedOpenerConfigAclPrefs[8] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNDRBSUPDUR", "Doorbell suppression duration", ((int)advancedOpenerConfigAclPrefs[9] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNSRING", "Sound Ring", ((int)advancedOpenerConfigAclPrefs[10] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNSOPN", "Sound Open", ((int)advancedOpenerConfigAclPrefs[11] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNSRTO", "Sound RTO", ((int)advancedOpenerConfigAclPrefs[12] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNSCM", "Sound CM", ((int)advancedOpenerConfigAclPrefs[13] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNSCFRM", "Sound confirmation", ((int)advancedOpenerConfigAclPrefs[14] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNSLVL", "Sound level", ((int)advancedOpenerConfigAclPrefs[15] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNSBPA", "Single button press action", ((int)advancedOpenerConfigAclPrefs[16] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNDBPA", "Double button press action", ((int)advancedOpenerConfigAclPrefs[17] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNBATT", "Battery type", ((int)advancedOpenerConfigAclPrefs[18] == 1), "chk_config_opener");
            printCheckBox(partString, "CONFOPNABTD", "Automatic battery type detection", ((int)advancedOpenerConfigAclPrefs[19] == 1), "chk_config_opener");
            partString.concat("</table><br>");
            break;
        default:
            break;
    }
}

void WebCfgServer::buildNukiConfigHtml(AsyncWebServerRequest *request)
{
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    buildHtmlHeader(response);
    response->print("<form class=\"adapt\" method=\"post\" action=\"savecfg\">");
    response->print("<h3>Basic Nuki Configuration</h3>");
    response->print("<table>");
    printCheckBox(response, "LOCKENA", "Nuki Smartlock enabled", _preferences->getBool(preference_lock_enabled), "");

    if(_preferences->getBool(preference_lock_enabled))
    {
        printInputField(response, "MQTTPATH", "MQTT Nuki Smartlock Path", _preferences->getString(preference_mqtt_lock_path).c_str(), 180, "");
    }

    printCheckBox(response, "OPENA", "Nuki Opener enabled", _preferences->getBool(preference_opener_enabled), "");

    if(_preferences->getBool(preference_opener_enabled))
    {
        printInputField(response, "MQTTOPPATH", "MQTT Nuki Opener Path", _preferences->getString(preference_mqtt_opener_path).c_str(), 180, "");
    }
    response->print("</table><br>");

    response->print("<h3>Advanced Nuki Configuration</h3>");
    response->print("<table>");

    printInputField(response, "LSTINT", "Query interval lock state (seconds)", _preferences->getInt(preference_query_interval_lockstate), 10, "");
    printInputField(response, "CFGINT", "Query interval configuration (seconds)", _preferences->getInt(preference_query_interval_configuration), 10, "");
    printInputField(response, "BATINT", "Query interval battery (seconds)", _preferences->getInt(preference_query_interval_battery), 10, "");
    if((_nuki != nullptr && _nuki->hasKeypad()) || (_nukiOpener != nullptr && _nukiOpener->hasKeypad()))
    {
        printInputField(response, "KPINT", "Query interval keypad (seconds)", _preferences->getInt(preference_query_interval_keypad), 10, "");
    }
    printInputField(response, "NRTRY", "Number of retries if command failed", _preferences->getInt(preference_command_nr_of_retries), 10, "");
    printInputField(response, "TRYDLY", "Delay between retries (milliseconds)", _preferences->getInt(preference_command_retry_delay), 10, "");
    if(_nuki != nullptr) printCheckBox(response, "REGAPP", "Lock: Nuki Bridge is running alongside Nuki Hub (needs re-pairing if changed)", _preferences->getBool(preference_register_as_app), "");
    if(_nukiOpener != nullptr) printCheckBox(response, "REGAPPOPN", "Opener: Nuki Bridge is running alongside Nuki Hub (needs re-pairing if changed)", _preferences->getBool(preference_register_opener_as_app), "");
#if PRESENCE_DETECTION_ENABLED
    printInputField(response, "PRDTMO", "Presence detection timeout (seconds; -1 to disable)", _preferences->getInt(preference_presence_detection_timeout), 10, "");
#endif
    printInputField(response, "RSBC", "Restart if bluetooth beacons not received (seconds; -1 to disable)", _preferences->getInt(preference_restart_ble_beacon_lost), 10, "");
    printInputField(response, "TXPWR", "BLE transmit power in dB (minimum -12, maximum 9)", _preferences->getInt(preference_ble_tx_power, 9), 10, "");

    response->print("</table>");
    response->print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response->print("</form>");
    response->print("</body></html>");
    request->send(response);
}

void WebCfgServer::buildGpioConfigHtml(AsyncWebServerRequest *request)
{
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    buildHtmlHeader(response);
    response->print("<form method=\"post\" action=\"savegpiocfg\">");
    response->print("<h3>GPIO Configuration</h3>");
    response->print("<table>");
    std::vector<std::pair<String, String>> options;
    String gpiopreselects = "var gpio = []; ";

    const auto& availablePins = _gpio->availablePins();
    for(const auto& pin : availablePins)
    {
        String pinStr = String(pin);
        String pinDesc = "Gpio " + pinStr;
        printDropDown(response, pinStr.c_str(), pinDesc.c_str(), "", options, "gpioselect");
        gpiopreselects.concat("gpio[" + pinStr + "] = '" + getPreselectionForGpio(pin) + "';");
    }

    response->print("</table>");
    response->print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response->print("</form>");

    options = getGpioOptions();

    response->print("<script type=\"text/javascript\">" + gpiopreselects + "var gpiooptions = '");

    for(const auto& option : options)
    {
        response->print("<option value=\"");
        response->print(option.first);
        response->print("\">");
        response->print(option.second);
        response->print("</option>");
    }

    response->print("'; var gpioselects = document.getElementsByClassName('gpioselect'); for (let i = 0; i < gpioselects.length; i++) { gpioselects[i].options.length = 0; gpioselects[i].innerHTML = gpiooptions; gpioselects[i].value = gpio[gpioselects[i].name]; }</script>");
    response->print("</body></html>");
    request->send(response);
}

void WebCfgServer::buildConfigureWifiHtml(AsyncWebServerRequest *request)
{
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    buildHtmlHeader(response);
    response->print("<h3>Wi-Fi</h3>");
    response->print("Click confirm to restart ESP into Wi-Fi configuration mode. After restart, connect to ESP access point to reconfigure Wi-Fi.<br><br>");
    buildNavigationButton(response, "Confirm", "/wifimanager");
    response->print("</body></html>");
    request->send(response);
}

void WebCfgServer::buildInfoHtml(AsyncWebServerRequest *request)
{
    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    buildHtmlHeader(response);
    response->print("<h3>System Information</h3><pre>");
    response->print("------------ NUKI HUB ------------");
    response->print("\nVersion: ");
    response->print(NUKI_HUB_VERSION);
    response->print("\nBuild: ");
    response->print(NUKI_HUB_BUILD);
    #ifndef DEBUG_NUKIHUB
    response->print("\nBuild type: Release");
    #else
    response->print("\nBuild type: Debug");
    #endif
    response->print("\nBuild date: ");
    response->print(NUKI_HUB_DATE);
    response->print("\nUptime (min): ");
    response->print(esp_timer_get_time() / 1000 / 1000 / 60);
    response->print("\nConfig version: ");
    response->print(_preferences->getInt(preference_config_version));
    response->print("\nLast restart reason FW: ");
    response->print(getRestartReason());
    response->print("\nLast restart reason ESP: ");
    response->print(getEspRestartReason());
    response->print("\nFree heap: ");
    response->print(esp_get_free_heap_size());
    response->print("\nNetwork task stack high watermark: ");
    response->print(uxTaskGetStackHighWaterMark(networkTaskHandle));
    response->print("\nNuki task stack high watermark: ");
    response->print(uxTaskGetStackHighWaterMark(nukiTaskHandle));
    response->print("\n\n------------ GENERAL SETTINGS ------------");
    response->print("\nNetwork task stack size: ");
    response->print(_preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE));
    response->print("\nNuki task stack size: ");
    response->print(_preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE));
    response->print("\nCheck for updates: ");
    response->print(_preferences->getBool(preference_check_updates, false) ? "Yes" : "No");
    response->print("\nLatest version: ");
    response->print(_preferences->getString(preference_latest_version, ""));
    response->print("\nAllow update from MQTT: ");
    response->print(_preferences->getBool(preference_update_from_mqtt, false) ? "Yes" : "No");
    response->print("\nWeb configurator username: ");
    response->print(_preferences->getString(preference_cred_user, "").length() > 0 ? "***" : "Not set");
    response->print("\nWeb configurator password: ");
    response->print(_preferences->getString(preference_cred_password, "").length() > 0 ? "***" : "Not set");
    response->print("\nWeb configurator enabled: ");
    response->print(_preferences->getBool(preference_webserver_enabled, true) ? "Yes" : "No");
    response->print("\nPublish debug information enabled: ");
    response->print(_preferences->getBool(preference_publish_debug_info, false) ? "Yes" : "No");
    response->print("\nMQTT log enabled: ");
    response->print(_preferences->getBool(preference_mqtt_log_enabled, false) ? "Yes" : "No");
    response->print("\nWebserial enabled: ");
    response->print(_preferences->getBool(preference_webserial_enabled, false) ? "Yes" : "No");
    response->print("\nBootloop protection enabled: ");
    response->print(_preferences->getBool(preference_enable_bootloop_reset, false) ? "Yes" : "No");
    response->print("\n\n------------ NETWORK ------------");
    response->print("\nNetwork device: ");
    response->print(_network->networkDeviceName());
    response->print("\nNetwork connected: ");
    response->print(_network->isConnected() ? "Yes" : "No");
    if(_network->isConnected())
    {
        response->print("\nIP Address: ");
        response->print(_network->localIP());
        if(_network->networkDeviceName() == "Built-in Wi-Fi")
        {
            response->print("\nSSID: ");
            response->print(WiFi.SSID());
            response->print("\nBSSID of AP: ");
            response->print(_network->networkBSSID());
            response->print("\nESP32 MAC address: ");
            response->print(WiFi.macAddress());
        }
        else
        {
            /*
            preference_has_mac_saved
            preference_has_mac_byte_0
            preference_has_mac_byte_1
            preference_has_mac_byte_2
            */
        }
    }
    response->print("\n\n------------ NETWORK SETTINGS ------------");
    response->print("\nNuki Hub hostname: ");
    response->print(_preferences->getString(preference_hostname, ""));
    if(_preferences->getBool(preference_ip_dhcp_enabled, true)) response->print("\nDHCP enabled: Yes");
    else
    {
        response->print("\nDHCP enabled: No");
        response->print("\nStatic IP address: ");
        response->print(_preferences->getString(preference_ip_address, ""));
        response->print("\nStatic IP subnet: ");
        response->print(_preferences->getString(preference_ip_subnet, ""));
        response->print("\nStatic IP gateway: ");
        response->print(_preferences->getString(preference_ip_gateway, ""));
        response->print("\nStatic IP DNS server: ");
        response->print(_preferences->getString(preference_ip_dns_server, ""));
    }

    response->print("\nFallback to Wi-Fi / Wi-Fi config portal disabled: ");
    response->print(_preferences->getBool(preference_network_wifi_fallback_disabled, false) ? "Yes" : "No");
    if(_network->networkDeviceName() == "Built-in Wi-Fi")
    {
        response->print("\nConnect to AP with the best signal enabled: ");
        response->print(_preferences->getBool(preference_find_best_rssi, false) ? "Yes" : "No");
        response->print("\nRSSI Publish interval (s): ");

        if(_preferences->getInt(preference_rssi_publish_interval, 60) < 0) response->print("Disabled");
        else response->print(_preferences->getInt(preference_rssi_publish_interval, 60));
    }
    response->print("\nRestart ESP32 on network disconnect enabled: ");
    response->print(_preferences->getBool(preference_restart_on_disconnect, false) ? "Yes" : "No");
    response->print("\nReconnect network on MQTT connection failure enabled: ");
    response->print(_preferences->getBool(preference_recon_netw_on_mqtt_discon, false) ? "Yes" : "No");
    response->print("\nMQTT Timeout until restart (s): ");
    if(_preferences->getInt(preference_network_timeout, 60) < 0) response->print("Disabled");
    else response->print(_preferences->getInt(preference_network_timeout, 60));
    response->print("\n\n------------ MQTT ------------");
    response->print("\nMQTT connected: ");
    response->print(_network->mqttConnectionState() > 0 ? "Yes" : "No");
    response->print("\nMQTT broker address: ");
    response->print(_preferences->getString(preference_mqtt_broker, ""));
    response->print("\nMQTT broker port: ");
    response->print(_preferences->getInt(preference_mqtt_broker_port, 1883));
    response->print("\nMQTT username: ");
    response->print(_preferences->getString(preference_mqtt_user, "").length() > 0 ? "***" : "Not set");
    response->print("\nMQTT password: ");
    response->print(_preferences->getString(preference_mqtt_password, "").length() > 0 ? "***" : "Not set");
    if(_nuki != nullptr)
    {
        response->print("\nMQTT lock base topic: ");
        response->print(_preferences->getString(preference_mqtt_lock_path, ""));
    }
    if(_nukiOpener != nullptr)
    {
        response->print("\nMQTT opener base topic: ");
        response->print(_preferences->getString(preference_mqtt_lock_path, ""));
    }
    response->print("\nMQTT SSL CA: ");
    response->print(_preferences->getString(preference_mqtt_ca, "").length() > 0 ? "***" : "Not set");
    response->print("\nMQTT SSL CRT: ");
    response->print(_preferences->getString(preference_mqtt_crt, "").length() > 0 ? "***" : "Not set");
    response->print("\nMQTT SSL Key: ");
    response->print(_preferences->getString(preference_mqtt_key, "").length() > 0 ? "***" : "Not set");
    response->print("\n\n------------ BLUETOOTH ------------");
    response->print("\nBluetooth TX power (dB): ");
    response->print(_preferences->getInt(preference_ble_tx_power, 9));
    response->print("\nBluetooth command nr of retries: ");
    response->print(_preferences->getInt(preference_command_nr_of_retries, 3));
    response->print("\nBluetooth command retry delay (ms): ");
    response->print(_preferences->getInt(preference_command_retry_delay, 100));
    response->print("\nSeconds until reboot when no BLE beacons recieved: ");
    response->print(_preferences->getInt(preference_restart_ble_beacon_lost, 60));
    response->print("\n\n------------ QUERY / PUBLISH SETTINGS ------------");
    response->print("\nLock/Opener state query interval (s): ");
    response->print(_preferences->getInt(preference_query_interval_lockstate, 1800));
    response->print("\nPublish Nuki device authorization log: ");
    response->print(_preferences->getBool(preference_publish_authdata, false) ? "Yes" : "No");
    response->print("\nMax authorization log entries to retrieve: ");
    response->print(_preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG));
    response->print("\nBattery state query interval (s): ");
    response->print(_preferences->getInt(preference_query_interval_battery, 1800));
    response->print("\nMost non-JSON MQTT topics disabled: ");
    response->print(_preferences->getBool(preference_disable_non_json, false) ? "Yes" : "No");
    response->print("\nPublish Nuki device config: ");
    response->print(_preferences->getBool(preference_conf_info_enabled, false) ? "Yes" : "No");
    response->print("\nConfig query interval (s): ");
    response->print(_preferences->getInt(preference_query_interval_configuration, 3600));
    response->print("\nPublish Keypad info: ");
    response->print(_preferences->getBool(preference_keypad_info_enabled, false) ? "Yes" : "No");
    response->print("\nKeypad query interval (s): ");
    response->print(_preferences->getInt(preference_query_interval_keypad, 1800));
    response->print("\nEnable Keypad control: ");
    response->print(_preferences->getBool(preference_keypad_control_enabled, false) ? "Yes" : "No");
    response->print("\nPublish Keypad topic per entry: ");
    response->print(_preferences->getBool(preference_keypad_topic_per_entry, false) ? "Yes" : "No");
    response->print("\nPublish Keypad codes: ");
    response->print(_preferences->getBool(preference_keypad_publish_code, false) ? "Yes" : "No");
    response->print("\nMax keypad entries to retrieve: ");
    response->print(_preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD));
    response->print("\nPublish timecontrol info: ");
    response->print(_preferences->getBool(preference_timecontrol_info_enabled, false) ? "Yes" : "No");
    response->print("\nKeypad query interval (s): ");
    response->print(_preferences->getInt(preference_query_interval_keypad, 1800));
    response->print("\nEnable timecontrol control: ");
    response->print(_preferences->getBool(preference_timecontrol_control_enabled, false) ? "Yes" : "No");
    response->print("\nPublish timecontrol topic per entry: ");
    response->print(_preferences->getBool(preference_timecontrol_topic_per_entry, false) ? "Yes" : "No");
    response->print("\nMax timecontrol entries to retrieve: ");
    response->print(_preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL));
    response->print("\n\n------------ HOME ASSISTANT ------------");
    response->print("\nHome Assistant auto discovery enabled: ");
    if(_preferences->getString(preference_mqtt_hass_discovery, "").length() > 0)
    {
        response->print("Yes");
        response->print("\nHome Assistant auto discovery topic: ");
        response->print(_preferences->getString(preference_mqtt_hass_discovery, "") + "/");
        response->print("\nNuki Hub configuration URL for HA: ");
        response->print(_preferences->getString(preference_mqtt_hass_cu_url, "").length() > 0 ? _preferences->getString(preference_mqtt_hass_cu_url, "") : "http://" + _network->localIP());
    }
    else response->print("No");
    response->print("\n\n------------ NUKI LOCK ------------");
    if(_nuki == nullptr || !_preferences->getBool(preference_lock_enabled, true)) response->print("\nLock enabled: No");
    else
    {
        response->print("\nLock enabled: Yes");
        response->print("\nPaired: ");
        response->print(_nuki->isPaired() ? "Yes" : "No");
        response->print("\nNuki Hub device ID: ");
        response->print(_preferences->getUInt(preference_device_id_lock, 0));
        response->print("\nNuki device ID: ");
        response->print(_preferences->getUInt(preference_nuki_id_lock, 0) > 0 ? "***" : "Not set");
        response->print("\nFirmware version: ");
        response->print(_nuki->firmwareVersion().c_str());
        response->print("\nHardware version: ");
        response->print(_nuki->hardwareVersion().c_str());
        response->print("\nValid PIN set: ");
        response->print(_nuki->isPaired() ? _nuki->isPinValid() ? "Yes" : "No" : "-");
        response->print("\nHas door sensor: ");
        response->print(_nuki->hasDoorSensor() ? "Yes" : "No");
        response->print("\nHas keypad: ");
        response->print(_nuki->hasKeypad() ? "Yes" : "No");
        if(_nuki->hasKeypad())
        {
            response->print("\nKeypad highest entries count: ");
            response->print(_preferences->getInt(preference_lock_max_keypad_code_count, 0));
        }
        response->print("\nTimecontrol highest entries count: ");
        response->print(_preferences->getInt(preference_lock_max_timecontrol_entry_count, 0));
        response->print("\nRegister as: ");
        response->print(_preferences->getBool(preference_register_as_app, false) ? "App" : "Bridge");
        response->print("\n\n------------ HYBRID MODE ------------");
        if(!_preferences->getBool(preference_official_hybrid, false)) response->print("\nHybrid mode enabled: No");
        else
        {
            response->print("\nHybrid mode enabled: Yes");
            response->print("\nHybrid mode connected: ");
            response->print(_nuki->offConnected() ? "Yes": "No");
            response->print("\nSending actions through official MQTT enabled: ");
            response->print(_preferences->getBool(preference_official_hybrid_actions, false) ? "Yes" : "No");
            if(_preferences->getBool(preference_official_hybrid_actions, false))
            {
                response->print("\nRetry actions through BLE enabled: ");
                response->print(_preferences->getBool(preference_official_hybrid_retry, false) ? "Yes" : "No");
            }
            response->print("\nTime between status updates when official MQTT is offline (s): ");
            response->print(_preferences->getInt(preference_query_interval_hybrid_lockstate, 600));
        }
        uint32_t basicLockConfigAclPrefs[16];
        _preferences->getBytes(preference_conf_lock_basic_acl, &basicLockConfigAclPrefs, sizeof(basicLockConfigAclPrefs));
        uint32_t advancedLockConfigAclPrefs[22];
        _preferences->getBytes(preference_conf_lock_advanced_acl, &advancedLockConfigAclPrefs, sizeof(advancedLockConfigAclPrefs));
        response->print("\n\n------------ NUKI LOCK ACL ------------");
        response->print("\nLock: ");
        response->print((int)aclPrefs[0] ? "Allowed" : "Disallowed");
        response->print("\nUnlock: ");
        response->print((int)aclPrefs[1] ? "Allowed" : "Disallowed");
        response->print("\nUnlatch: ");
        response->print((int)aclPrefs[2] ? "Allowed" : "Disallowed");
        response->print("\nLock N Go: ");
        response->print((int)aclPrefs[3] ? "Allowed" : "Disallowed");
        response->print("\nLock N Go Unlatch: ");
        response->print((int)aclPrefs[4] ? "Allowed" : "Disallowed");
        response->print("\nFull Lock: ");
        response->print((int)aclPrefs[5] ? "Allowed" : "Disallowed");
        response->print("\nFob Action 1: ");
        response->print((int)aclPrefs[6] ? "Allowed" : "Disallowed");
        response->print("\nFob Action 2: ");
        response->print((int)aclPrefs[7] ? "Allowed" : "Disallowed");
        response->print("\nFob Action 3: ");
        response->print((int)aclPrefs[8] ? "Allowed" : "Disallowed");
        response->print("\n\n------------ NUKI LOCK CONFIG ACL ------------");
        response->print("\nName: ");
        response->print((int)basicLockConfigAclPrefs[0] ? "Allowed" : "Disallowed");
        response->print("\nLatitude: ");
        response->print((int)basicLockConfigAclPrefs[1] ? "Allowed" : "Disallowed");
        response->print("\nLongitude: ");
        response->print((int)basicLockConfigAclPrefs[2] ? "Allowed" : "Disallowed");
        response->print("\nAuto Unlatch: ");
        response->print((int)basicLockConfigAclPrefs[3] ? "Allowed" : "Disallowed");
        response->print("\nPairing enabled: ");
        response->print((int)basicLockConfigAclPrefs[4] ? "Allowed" : "Disallowed");
        response->print("\nButton enabled: ");
        response->print((int)basicLockConfigAclPrefs[5] ? "Allowed" : "Disallowed");
        response->print("\nLED flash enabled: ");
        response->print((int)basicLockConfigAclPrefs[6] ? "Allowed" : "Disallowed");
        response->print("\nLED brightness: ");
        response->print((int)basicLockConfigAclPrefs[7] ? "Allowed" : "Disallowed");
        response->print("\nTimezone offset: ");
        response->print((int)basicLockConfigAclPrefs[8] ? "Allowed" : "Disallowed");
        response->print("\nDST mode: ");
        response->print((int)basicLockConfigAclPrefs[9] ? "Allowed" : "Disallowed");
        response->print("\nFob Action 1: ");
        response->print((int)basicLockConfigAclPrefs[10] ? "Allowed" : "Disallowed");
        response->print("\nFob Action 2: ");
        response->print((int)basicLockConfigAclPrefs[11] ? "Allowed" : "Disallowed");
        response->print("\nFob Action 3: ");
        response->print((int)basicLockConfigAclPrefs[12] ? "Allowed" : "Disallowed");
        response->print("\nSingle Lock: ");
        response->print((int)basicLockConfigAclPrefs[13] ? "Allowed" : "Disallowed");
        response->print("\nAdvertising Mode: ");
        response->print((int)basicLockConfigAclPrefs[14] ? "Allowed" : "Disallowed");
        response->print("\nTimezone ID: ");
        response->print((int)basicLockConfigAclPrefs[15] ? "Allowed" : "Disallowed");
        response->print("\nUnlocked Position Offset Degrees: ");
        response->print((int)advancedLockConfigAclPrefs[0] ? "Allowed" : "Disallowed");
        response->print("\nLocked Position Offset Degrees: ");
        response->print((int)advancedLockConfigAclPrefs[1] ? "Allowed" : "Disallowed");
        response->print("\nSingle Locked Position Offset Degrees: ");
        response->print((int)advancedLockConfigAclPrefs[2] ? "Allowed" : "Disallowed");
        response->print("\nUnlocked To Locked Transition Offset Degrees: ");
        response->print((int)advancedLockConfigAclPrefs[3] ? "Allowed" : "Disallowed");
        response->print("\nLock n Go timeout: ");
        response->print((int)advancedLockConfigAclPrefs[4] ? "Allowed" : "Disallowed");
        response->print("\nSingle button press action: ");
        response->print((int)advancedLockConfigAclPrefs[5] ? "Allowed" : "Disallowed");
        response->print("\nDouble button press action: ");
        response->print((int)advancedLockConfigAclPrefs[6] ? "Allowed" : "Disallowed");
        response->print("\nDetached cylinder: ");
        response->print((int)advancedLockConfigAclPrefs[7] ? "Allowed" : "Disallowed");
        response->print("\nBattery type: ");
        response->print((int)advancedLockConfigAclPrefs[8] ? "Allowed" : "Disallowed");
        response->print("\nAutomatic battery type detection: ");
        response->print((int)advancedLockConfigAclPrefs[9] ? "Allowed" : "Disallowed");
        response->print("\nUnlatch duration: ");
        response->print((int)advancedLockConfigAclPrefs[10] ? "Allowed" : "Disallowed");
        response->print("\nAuto lock timeout: ");
        response->print((int)advancedLockConfigAclPrefs[11] ? "Allowed" : "Disallowed");
        response->print("\nAuto unlock disabled: ");
        response->print((int)advancedLockConfigAclPrefs[12] ? "Allowed" : "Disallowed");
        response->print("\nNightmode enabled: ");
        response->print((int)advancedLockConfigAclPrefs[13] ? "Allowed" : "Disallowed");
        response->print("\nNightmode start time: ");
        response->print((int)advancedLockConfigAclPrefs[14] ? "Allowed" : "Disallowed");
        response->print("\nNightmode end time: ");
        response->print((int)advancedLockConfigAclPrefs[15] ? "Allowed" : "Disallowed");
        response->print("\nNightmode auto lock enabled: ");
        response->print((int)advancedLockConfigAclPrefs[16] ? "Allowed" : "Disallowed");
        response->print("\nNightmode auto unlock disabled: ");
        response->print((int)advancedLockConfigAclPrefs[17] ? "Allowed" : "Disallowed");
        response->print("\nNightmode immediate lock on start: ");
        response->print((int)advancedLockConfigAclPrefs[18] ? "Allowed" : "Disallowed");
        response->print("\nAuto lock enabled: ");
        response->print((int)advancedLockConfigAclPrefs[19] ? "Allowed" : "Disallowed");
        response->print("\nImmediate auto lock enabled: ");
        response->print((int)advancedLockConfigAclPrefs[20] ? "Allowed" : "Disallowed");
        response->print("\nAuto update enabled: ");
        response->print((int)advancedLockConfigAclPrefs[21] ? "Allowed" : "Disallowed");

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
            response->print("\n\n------------ NUKI LOCK PAIRING ------------");
            response->print("\nBLE Address: ");
            for (int i = 0; i < 6; i++)
            {
                sprintf(tmp, "%02x", currentBleAddress[i]);
                response->print(tmp);
            }
            response->print("\nSecretKeyK: ");
            for (int i = 0; i < 32; i++)
            {
                sprintf(tmp, "%02x", secretKeyK[i]);
                response->print(tmp);
            }
            response->print("\nAuthorizationId: ");
            for (int i = 0; i < 4; i++)
            {
                sprintf(tmp, "%02x", authorizationId[i]);
                response->print(tmp);
            }
        }
    }

    response->print("\n\n------------ NUKI OPENER ------------");
    if(_nukiOpener == nullptr || !_preferences->getBool(preference_opener_enabled, true)) response->print("\nOpener enabled: No");
    else
    {
        response->print("\nOpener enabled: Yes");
        response->print("\nPaired: ");
        response->print(_nukiOpener->isPaired() ? "Yes" : "No");
        response->print("\nNuki Hub device ID: ");
        response->print(_preferences->getUInt(preference_device_id_opener, 0));
        response->print("\nNuki device ID: ");
        response->print(_preferences->getUInt(preference_nuki_id_opener, 0) > 0 ? "***" : "Not set");
        response->print("\nFirmware version: ");
        response->print(_nukiOpener->firmwareVersion().c_str());
        response->print("\nHardware version: ");
        response->print(_nukiOpener->hardwareVersion().c_str());
        response->print("\nOpener valid PIN set: ");
        response->print(_nukiOpener->isPaired() ? _nukiOpener->isPinValid() ? "Yes" : "No" : "-");
        response->print("\nOpener has keypad: ");
        response->print(_nukiOpener->hasKeypad() ? "Yes" : "No");
        if(_nuki->hasKeypad())
        {
            response->print("\nKeypad highest entries count: ");
            response->print(_preferences->getInt(preference_opener_max_keypad_code_count, 0));
        }
        response->print("\nTimecontrol highest entries count: ");
        response->print(_preferences->getInt(preference_opener_max_timecontrol_entry_count, 0));
        response->print("\nRegister as: ");
        response->print(_preferences->getBool(preference_register_opener_as_app, false) ? "App" : "Bridge");
        response->print("\nNuki Opener Lock/Unlock action set to Continuous mode in Home Assistant: ");
        response->print(_preferences->getBool(preference_opener_continuous_mode, false) ? "Yes" : "No");
        uint32_t basicOpenerConfigAclPrefs[14];
        _preferences->getBytes(preference_conf_opener_basic_acl, &basicOpenerConfigAclPrefs, sizeof(basicOpenerConfigAclPrefs));
        uint32_t advancedOpenerConfigAclPrefs[20];
        _preferences->getBytes(preference_conf_opener_advanced_acl, &advancedOpenerConfigAclPrefs, sizeof(advancedOpenerConfigAclPrefs));
        response->print("\n\n------------ NUKI OPENER ACL ------------");
        response->print("\nActivate Ring-to-Open: ");
        response->print((int)aclPrefs[9] ? "Allowed" : "Disallowed");
        response->print("\nDeactivate Ring-to-Open: ");
        response->print((int)aclPrefs[10] ? "Allowed" : "Disallowed");
        response->print("\nElectric Strike Actuation: ");
        response->print((int)aclPrefs[11] ? "Allowed" : "Disallowed");
        response->print("\nActivate Continuous Mode: ");
        response->print((int)aclPrefs[12] ? "Allowed" : "Disallowed");
        response->print("\nDeactivate Continuous Mode: ");
        response->print((int)aclPrefs[13] ? "Allowed" : "Disallowed");
        response->print("\nFob Action 1: ");
        response->print((int)aclPrefs[14] ? "Allowed" : "Disallowed");
        response->print("\nFob Action 2: ");
        response->print((int)aclPrefs[15] ? "Allowed" : "Disallowed");
        response->print("\nFob Action 3: ");
        response->print((int)aclPrefs[16] ? "Allowed" : "Disallowed");
        response->print("\n\n------------ NUKI OPENER CONFIG ACL ------------");
        response->print("\nName: ");
        response->print((int)basicOpenerConfigAclPrefs[0] ? "Allowed" : "Disallowed");
        response->print("\nLatitude: ");
        response->print((int)basicOpenerConfigAclPrefs[1] ? "Allowed" : "Disallowed");
        response->print("\nLongitude: ");
        response->print((int)basicOpenerConfigAclPrefs[2] ? "Allowed" : "Disallowed");
        response->print("\nPairing enabled: ");
        response->print((int)basicOpenerConfigAclPrefs[3] ? "Allowed" : "Disallowed");
        response->print("\nButton enabled: ");
        response->print((int)basicOpenerConfigAclPrefs[4] ? "Allowed" : "Disallowed");
        response->print("\nLED flash enabled: ");
        response->print((int)basicOpenerConfigAclPrefs[5] ? "Allowed" : "Disallowed");
        response->print("\nTimezone offset: ");
        response->print((int)basicOpenerConfigAclPrefs[6] ? "Allowed" : "Disallowed");
        response->print("\nDST mode: ");
        response->print((int)basicOpenerConfigAclPrefs[7] ? "Allowed" : "Disallowed");
        response->print("\nFob Action 1: ");
        response->print((int)basicOpenerConfigAclPrefs[8] ? "Allowed" : "Disallowed");
        response->print("\nFob Action 2: ");
        response->print((int)basicOpenerConfigAclPrefs[9] ? "Allowed" : "Disallowed");
        response->print("\nFob Action 3: ");
        response->print((int)basicOpenerConfigAclPrefs[10] ? "Allowed" : "Disallowed");
        response->print("\nOperating Mode: ");
        response->print((int)basicOpenerConfigAclPrefs[11] ? "Allowed" : "Disallowed");
        response->print("\nAdvertising Mode: ");
        response->print((int)basicOpenerConfigAclPrefs[12] ? "Allowed" : "Disallowed");
        response->print("\nTimezone ID: ");
        response->print((int)basicOpenerConfigAclPrefs[13] ? "Allowed" : "Disallowed");
        response->print("\nIntercom ID: ");
        response->print((int)advancedOpenerConfigAclPrefs[0] ? "Allowed" : "Disallowed");
        response->print("\nBUS mode Switch: ");
        response->print((int)advancedOpenerConfigAclPrefs[1] ? "Allowed" : "Disallowed");
        response->print("\nShort Circuit Duration: ");
        response->print((int)advancedOpenerConfigAclPrefs[2] ? "Allowed" : "Disallowed");
        response->print("\nEletric Strike Delay: ");
        response->print((int)advancedOpenerConfigAclPrefs[3] ? "Allowed" : "Disallowed");
        response->print("\nRandom Electric Strike Delay: ");
        response->print((int)advancedOpenerConfigAclPrefs[4] ? "Allowed" : "Disallowed");
        response->print("\nElectric Strike Duration: ");
        response->print((int)advancedOpenerConfigAclPrefs[5] ? "Allowed" : "Disallowed");
        response->print("\nDisable RTO after ring: ");
        response->print((int)advancedOpenerConfigAclPrefs[6] ? "Allowed" : "Disallowed");
        response->print("\nRTO timeout: ");
        response->print((int)advancedOpenerConfigAclPrefs[7] ? "Allowed" : "Disallowed");
        response->print("\nDoorbell suppression: ");
        response->print((int)advancedOpenerConfigAclPrefs[8] ? "Allowed" : "Disallowed");
        response->print("\nDoorbell suppression duration: ");
        response->print((int)advancedOpenerConfigAclPrefs[9] ? "Allowed" : "Disallowed");
        response->print("\nSound Ring: ");
        response->print((int)advancedOpenerConfigAclPrefs[10] ? "Allowed" : "Disallowed");
        response->print("\nSound Open: ");
        response->print((int)advancedOpenerConfigAclPrefs[11] ? "Allowed" : "Disallowed");
        response->print("\nSound RTO: ");
        response->print((int)advancedOpenerConfigAclPrefs[12] ? "Allowed" : "Disallowed");
        response->print("\nSound CM: ");
        response->print((int)advancedOpenerConfigAclPrefs[13] ? "Allowed" : "Disallowed");
        response->print("\nSound confirmation: ");
        response->print((int)advancedOpenerConfigAclPrefs[14] ? "Allowed" : "Disallowed");
        response->print("\nSound level: ");
        response->print((int)advancedOpenerConfigAclPrefs[15] ? "Allowed" : "Disallowed");
        response->print("\nSingle button press action: ");
        response->print((int)advancedOpenerConfigAclPrefs[16] ? "Allowed" : "Disallowed");
        response->print("\nDouble button press action: ");
        response->print((int)advancedOpenerConfigAclPrefs[17] ? "Allowed" : "Disallowed");
        response->print("\nBattery type: ");
        response->print((int)advancedOpenerConfigAclPrefs[18] ? "Allowed" : "Disallowed");
        response->print("\nAutomatic battery type detection: ");
        response->print((int)advancedOpenerConfigAclPrefs[19] ? "Allowed" : "Disallowed");
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
            response->print("\n\n------------ NUKI OPENER PAIRING ------------");
            response->print("\nBLE Address: ");
            for (int i = 0; i < 6; i++)
            {
                sprintf(tmp, "%02x", currentBleAddressOpn[i]);
                response->print(tmp);
            }
            response->print("\nSecretKeyK: ");
            for (int i = 0; i < 32; i++)
            {
                sprintf(tmp, "%02x", secretKeyKOpn[i]);
                response->print(tmp);
            }
            response->print("\nAuthorizationId: ");
            for (int i = 0; i < 4; i++)
            {
                sprintf(tmp, "%02x", authorizationIdOpn[i]);
                response->print(tmp);
            }
        }
    }

    response->print("\n\n------------ GPIO ------------");
    String gpioStr = "";
    _gpio->getConfigurationText(gpioStr, _gpio->pinConfiguration());
    response->print(gpioStr);

    response->print("</pre> </body></html>");
    request->send(response);
}

void WebCfgServer::processUnpair(AsyncWebServerRequest *request, bool opener)
{
    String value = "";
    if(request->hasParam("CONFIRMTOKEN"))
    {
        const AsyncWebParameter* p = request->getParam("CONFIRMTOKEN");
        if(p->value() != "") value = p->value();
    }

    if(value != _confirmCode)
    {
        buildConfirmHtml(request, "Confirm code is invalid.", 3);
        return;
    }

    buildConfirmHtml(request, opener ? "Unpairing Nuki Opener and restarting." : "Unpairing Nuki Lock and restarting.", 3);
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
}

void WebCfgServer::processUpdate(AsyncWebServerRequest *request)
{
    String value = "";
    if(request->hasParam("token"))
    {
        const AsyncWebParameter* p = request->getParam("token");
        if(p->value() != "") value = p->value();
    }

    if(value != _confirmCode)
    {
        buildConfirmHtml(request, "Confirm code is invalid.", 3, true);
        return;
    }

    if(request->hasParam("beta"))
    {
        if(request->hasParam("debug"))
        {
            buildConfirmHtml(request, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest DEBUG BETA version", 2, true);
            _preferences->putString(preference_ota_updater_url, GITHUB_BETA_UPDATER_BINARY_URL_DBG);
            _preferences->putString(preference_ota_main_url, GITHUB_BETA_RELEASE_BINARY_URL_DBG);
        }
        else
        {
            buildConfirmHtml(request, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest BETA version", 2, true);
            _preferences->putString(preference_ota_updater_url, GITHUB_BETA_UPDATER_BINARY_URL);
            _preferences->putString(preference_ota_main_url, GITHUB_BETA_RELEASE_BINARY_URL);
        }
    }
    else if(request->hasParam("master"))
    {
        if(request->hasParam("debug"))
        {
            buildConfirmHtml(request, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest DEBUG DEVELOPMENT version", 2, true);
            _preferences->putString(preference_ota_updater_url, GITHUB_MASTER_UPDATER_BINARY_URL_DBG);
            _preferences->putString(preference_ota_main_url, GITHUB_MASTER_RELEASE_BINARY_URL_DBG);
        }
        else
        {
            buildConfirmHtml(request, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest DEVELOPMENT version", 2, true);
            _preferences->putString(preference_ota_updater_url, GITHUB_MASTER_UPDATER_BINARY_URL);
            _preferences->putString(preference_ota_main_url, GITHUB_MASTER_RELEASE_BINARY_URL);
        }
    }
    else
    {
        if(request->hasParam("debug"))
        {
            buildConfirmHtml(request, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest DEBUG RELEASE version", 2, true);
            _preferences->putString(preference_ota_updater_url, GITHUB_LATEST_UPDATER_BINARY_URL_DBG);
            _preferences->putString(preference_ota_main_url, GITHUB_LATEST_UPDATER_BINARY_URL_DBG);
        }
        else
        {
            buildConfirmHtml(request, "Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest RELEASE version", 2, true);
            _preferences->putString(preference_ota_updater_url, GITHUB_LATEST_UPDATER_BINARY_URL);
            _preferences->putString(preference_ota_main_url, GITHUB_LATEST_RELEASE_BINARY_URL);
        }
    }
    waitAndProcess(true, 1000);
    restartEsp(RestartReason::OTAReboot);
}

void WebCfgServer::processFactoryReset(AsyncWebServerRequest *request)
{
    String value = "";
    if(request->hasParam("CONFIRMTOKEN"))
    {
        const AsyncWebParameter* p = request->getParam("CONFIRMTOKEN");
        if(p->value() != "") value = p->value();
    }

    bool resetWifi = false;
    if(value.length() == 0 || value != _confirmCode)
    {
        buildConfirmHtml(request, "Confirm code is invalid.", 3);
        return;
    }
    else
    {
        String value2 = "";
        if(request->hasParam("WIFI"))
        {
            const AsyncWebParameter* p = request->getParam("WIFI");
            if(p->value() != "") value = p->value();
        }

        if(value2 == "1")
        {
            resetWifi = true;
            buildConfirmHtml(request, "Factory resetting Nuki Hub, unpairing Nuki Lock and Nuki Opener and resetting WiFi.", 3);
        }
        else buildConfirmHtml(request, "Factory resetting Nuki Hub, unpairing Nuki Lock and Nuki Opener.", 3);
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

    if(resetWifi)
    {
        wifi_config_t current_conf;
        esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &current_conf);
        memset(current_conf.sta.ssid, 0, sizeof(current_conf.sta.ssid));
        memset(current_conf.sta.password, 0, sizeof(current_conf.sta.password));
        esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &current_conf);
        _network->reconfigureDevice();
    }

    waitAndProcess(false, 3000);
    restartEsp(RestartReason::NukiHubReset);
}

void WebCfgServer::printInputField(AsyncResponseStream *response,
                                   const char *token,
                                   const char *description,
                                   const char *value,
                                   const size_t& maxLength,
                                   const char *id,
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
    if(strcmp(id, "") != 0)
    {
        response->print(" id=\"");
        response->print(id);
        response->print("\"");
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

void WebCfgServer::printInputField(AsyncResponseStream *response,
                                   const char *token,
                                   const char *description,
                                   const int value,
                                   size_t maxLength,
                                   const char *id)
{
    char valueStr[20];
    itoa(value, valueStr, 10);
    printInputField(response, token, description, valueStr, maxLength, id);
}

void WebCfgServer::printCheckBox(AsyncResponseStream *response, const char *token, const char *description, const bool value, const char *htmlClass)
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

void WebCfgServer::printCheckBox(String &partString, const char *token, const char *description, const bool value, const char *htmlClass)
{
    partString.concat("<tr><td>");
    partString.concat(description);
    partString.concat("</td><td>");

    partString.concat("<input type=hidden name=\"");
    partString.concat(token);
    partString.concat("\" value=\"0\"");
    partString.concat("/>");

    partString.concat("<input type=checkbox name=\"");
    partString.concat(token);

    partString.concat("\" class=\"");
    partString.concat(htmlClass);

    partString.concat("\" value=\"1\"");
    partString.concat(value ? " checked=\"checked\"" : "");
    partString.concat("/></td></tr>");
}

void WebCfgServer::printTextarea(AsyncResponseStream *response,
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

void WebCfgServer::printDropDown(AsyncResponseStream *response, const char *token, const char *description, const String preselectedValue, const std::vector<std::pair<String, String>> options, const String className)
{
    response->print("<tr><td>");
    response->print(description);
    response->print("</td><td>");

    if(className.length() > 0) response->print("<select class=\"" + className + "\" name=\"");
    else response->print("<select name=\"");
    response->print(token);
    response->print("\">");

    for(const auto& option : options)
    {
        if(option.first == preselectedValue) response->print("<option selected=\"selected\" value=\"");
        else response->print("<option value=\"");
        response->print(option.first);
        response->print("\">");
        response->print(option.second);
        response->print("</option>");
    }

    response->print("</select>");
    response->print("</td></tr>");
}

void WebCfgServer::buildNavigationButton(AsyncResponseStream *response, const char *caption, const char *targetPath, const char* labelText)
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

void WebCfgServer::buildNavigationMenuEntry(AsyncResponseStream *response, const char *title, const char *targetPath, const char* warningMessage)
{
    response->print("<a href=\"");
    response->print(targetPath);
    response->print("\">");
    response->print("<li>");
    response->print(title);
    if(strcmp(warningMessage, "") != 0){
        response->print("<span>");
        response->print(warningMessage);
        response->print("</span>");
    }
    response->print("</li></a>");
}

void WebCfgServer::printParameter(AsyncResponseStream *response, const char *description, const char *value, const char *link, const char *id)
{
    response->print("<tr>");
    response->print("<td>");
    response->print(description);
    response->print("</td>");
    if(strcmp(id, "") == 0) response->print("<td>");
    else
    {
        response->print("<td id=\"");
        response->print(id);
        response->print("\">");
    }
    if(strcmp(link, "") == 0) response->print(value);
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

    options.push_back(std::make_pair("1", "Wi-Fi only"));
    options.push_back(std::make_pair("2", "Generic W5500"));
    options.push_back(std::make_pair("3", "M5Stack Atom POE (W5500)"));
    options.push_back(std::make_pair("4", "Olimex ESP32-POE / ESP-POE-ISO"));
    options.push_back(std::make_pair("5", "WT32-ETH01"));
    options.push_back(std::make_pair("6", "M5STACK PoESP32 Unit"));
    options.push_back(std::make_pair("7", "LilyGO T-ETH-POE"));
    options.push_back(std::make_pair("8", "GL-S10"));
    options.push_back(std::make_pair("9", "ETH01-Evo"));

    return options;
}

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
