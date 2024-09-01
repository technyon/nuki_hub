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
#endif
#include <Update.h>

extern const uint8_t x509_crt_imported_bundle_bin_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_imported_bundle_bin_end[]   asm("_binary_x509_crt_bundle_end");

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
    _response.reserve(8192);

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
    _asyncServer->on("/import", HTTP_POST, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        String message = "";
        bool restart = processImport(request, message);
        buildConfirmHtml(request, message, 3, true);
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
        buildAccLvlHtml(request);
    });
    _asyncServer->on("/custntw", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildCustomNetworkConfigHtml(request);
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
    #ifndef CONFIG_IDF_TARGET_ESP32H2
    _asyncServer->on("/wifi", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildConfigureWifiHtml(request);
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
    #endif
    _asyncServer->on("/unpairlock", HTTP_POST, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        processUnpair(request, false);
    });
    _asyncServer->on("/unpairopener", HTTP_POST, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        processUnpair(request, true);
    });
    _asyncServer->on("/factoryreset", HTTP_POST, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        processFactoryReset(request);
    });
    _asyncServer->on("/info", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildInfoHtml(request);
    });
    _asyncServer->on("/debugon", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        _preferences->putBool(preference_publish_debug_info, true);
        buildConfirmHtml(request, "Debug On", 3, true);
    });
    _asyncServer->on("/debugoff", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        _preferences->putBool(preference_publish_debug_info, false);
        buildConfirmHtml(request, "Debug Off", 3, true);
    });
    _asyncServer->on("/savecfg", HTTP_POST, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        String message = "";
        bool restart = processArgs(request, message);
        buildConfirmHtml(request, message, 3, true);
    });
    _asyncServer->on("/savegpiocfg", HTTP_POST, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        processGpioArgs(request);
        buildConfirmHtml(request, "Saving GPIO configuration. Restarting.", 3, true);
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
        buildConfirmHtml(request, "Rebooting to other partition", 2, true);
        waitAndProcess(true, 1000);
        esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL));
        restartEsp(RestartReason::OTAReboot);
    });
    _asyncServer->on("/reboot", HTTP_GET, [&](AsyncWebServerRequest *request){
        if(strlen(_credUser) > 0 && strlen(_credPassword) > 0) if(!request->authenticate(_credUser, _credPassword)) return request->requestAuthentication();
        buildConfirmHtml(request, "Rebooting", 2, true);
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

void WebCfgServer::sendResponse(AsyncWebServerRequest *request)
{
    AsyncWebServerResponse *response = request->beginChunkedResponse("text/html",
    [&](uint8_t *buffer, size_t maxlen, size_t index) -> size_t {
      size_t len = min(maxlen, _response.length() - index);
      memcpy(buffer, _response.c_str() + index, len);
      return len;
    });

    request->send(response);
}

void WebCfgServer::buildOtaHtml(AsyncWebServerRequest *request, bool debug)
{
    _response = "";
    buildHtmlHeader();

    bool errored = false;
    if(request->hasParam("errored"))
    {
        const AsyncWebParameter* p = request->getParam("errored");
        if(p->value() != "") errored = true;
    }

    if(errored) _response.concat("<div>Over-the-air update errored. Please check the logs for more info</div><br/>");

    if(_partitionType == 0)
    {
        _response.concat("<h4 class=\"warning\">You are currently running Nuki Hub with an outdated partition scheme. Because of this you cannot use OTA to update to 9.00 or higher. Please check GitHub for instructions on how to update to 9.00 and the new partition scheme</h4>");
        _response.concat("<button title=\"Open latest release on GitHub\" onclick=\" window.open('");
        _response.concat(GITHUB_LATEST_RELEASE_URL);
        _response.concat("', '_blank'); return false;\">Open latest release on GitHub</button>");
        return;
    }

    _response.concat("<div id=\"msgdiv\" style=\"visibility:hidden\">Initiating Over-the-air update. This will take about two minutes, please be patient.<br>You will be forwarded automatically when the update is complete.</div>");
    _response.concat("<div id=\"autoupdform\"><h4>Update Nuki Hub</h4>");
    _response.concat("Click on the button to reboot and automatically update Nuki Hub and the Nuki Hub updater to the latest versions from GitHub");
    _response.concat("<div style=\"clear: both\"></div>");

    String release_type;

    if(debug) release_type = "debug";
    else release_type = "release";

    #ifndef DEBUG_NUKIHUB
    String build_type = "release";
    #else
    String build_type = "debug";
    #endif
    _response.concat("<form onsubmit=\"if(document.getElementById('currentver').innerHTML == document.getElementById('latestver').innerHTML && '" + release_type + "' == '" + build_type + "') { alert('You are already on this version, build and build type'); return false; } else { return confirm('Do you really want to update to the latest release?'); } \" action=\"/autoupdate\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"release\" value=\"1\" /><input type=\"hidden\" name=\"" + release_type + "\" value=\"1\" /><input type=\"hidden\" name=\"token\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"background: green\" value=\"Update to latest release\"></form>");
    _response.concat("<form onsubmit=\"if(document.getElementById('currentver').innerHTML == document.getElementById('betaver').innerHTML && '" + release_type + "' == '" + build_type + "') { alert('You are already on this version, build and build type'); return false; } else { return confirm('Do you really want to update to the latest beta? This version could contain breaking bugs and necessitate downgrading to the latest release version using USB/Serial'); }\" action=\"/autoupdate\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"beta\" value=\"1\" /><input type=\"hidden\" name=\"" + release_type + "\" value=\"1\" /><input type=\"hidden\" name=\"token\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"color: black; background: yellow\"  value=\"Update to latest beta\"></form>");
    _response.concat("<form onsubmit=\"if(document.getElementById('currentver').innerHTML == document.getElementById('devver').innerHTML && '" + release_type + "' == '" + build_type + "') { alert('You are already on this version, build and build type'); return false; } else { return confirm('Do you really want to update to the latest development version? This version could contain breaking bugs and necessitate downgrading to the latest release version using USB/Serial'); }\" action=\"/autoupdate\" method=\"get\" style=\"float: left; margin-right: 10px\"><input type=\"hidden\" name=\"master\" value=\"1\" /><input type=\"hidden\" name=\"" + release_type + "\" value=\"1\" /><input type=\"hidden\" name=\"token\" value=\"" + _confirmCode + "\" /><br><input type=\"submit\" style=\"background: red\"  value=\"Update to latest development version\"></form>");
    _response.concat("<div style=\"clear: both\"></div><br>");

    _response.concat("<b>Current version: </b><span id=\"currentver\">");
    _response.concat(NUKI_HUB_VERSION);
    _response.concat(" (");
    _response.concat(NUKI_HUB_BUILD);
    _response.concat(")</span>, ");
    _response.concat(NUKI_HUB_DATE);
    _response.concat("<br>");

    #ifndef NUKI_HUB_UPDATER
    bool manifestSuccess = false;
    JsonDocument doc;

    NetworkClientSecure *client = new NetworkClientSecure;
    if (client) {
        client->setCACertBundle(x509_crt_imported_bundle_bin_start, x509_crt_imported_bundle_bin_end - x509_crt_imported_bundle_bin_start);
        {
            HTTPClient https;
            https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            https.setTimeout(2500);
            https.useHTTP10(true);

            if (https.begin(*client, GITHUB_OTA_MANIFEST_URL)) {
                int http_responseCode = https.GET();

                if (http_responseCode == HTTP_CODE_OK || http_responseCode == HTTP_CODE_MOVED_PERMANENTLY)
                {
                    DeserializationError jsonError = deserializeJson(doc, https.getStream());
                    if (!jsonError) { manifestSuccess = true; }
                }
                https.end();
            }
        }
        delete client;
    }

    if(!manifestSuccess)
    {
        _response.concat("<span id=\"currentver\" style=\"display: none;\">currentver</span><span id=\"latestver\" style=\"display: none;\">latestver</span><span id=\"devver\" style=\"display: none;\">devver</span><span id=\"betaver\" style=\"display: none;\">betaver</span>");
    }
    else
    {
        _response.concat("<b>Latest release version: </b><span id=\"latestver\">");
        _response.concat(doc["release"]["fullversion"].as<const char*>());
        _response.concat(" (");
        _response.concat(doc["release"]["build"].as<const char*>());
        _response.concat(")</span>, ");
        _response.concat(doc["release"]["time"].as<const char*>());
        _response.concat("<br>");
        _response.concat("<b>Latest beta version: </b><span id=\"betaver\">");
        if(doc["beta"]["fullversion"] != "No beta available")
        {
            _response.concat(doc["beta"]["fullversion"].as<const char*>());
            _response.concat(" (");
            _response.concat(doc["beta"]["build"].as<const char*>());
            _response.concat(")</span>, ");
            _response.concat(doc["beta"]["time"].as<const char*>());
        }
        else
        {
            _response.concat(doc["beta"]["fullversion"].as<const char*>());
            _response.concat("</span>");
        }
        _response.concat("<br>");
        _response.concat("<b>Latest development version: </b><span id=\"devver\">");
        _response.concat(doc["master"]["fullversion"].as<const char*>());
        _response.concat(" (");
        _response.concat(doc["master"]["build"].as<const char*>());
        _response.concat(")</span>, ");
        _response.concat(doc["master"]["time"].as<const char*>());
        _response.concat("<br>");

        String currentVersion = NUKI_HUB_VERSION;
        const char* latestVersion;

        if(atof(doc["release"]["version"]) >= atof(currentVersion.c_str())) latestVersion = doc["release"]["fullversion"];
        else if(currentVersion.indexOf("beta") > 0) latestVersion = doc["beta"]["fullversion"];
        else if(currentVersion.indexOf("master") > 0) latestVersion = doc["master"]["fullversion"];
        else latestVersion = doc["release"]["fullversion"];

        if(strcmp(latestVersion, _preferences->getString(preference_latest_version).c_str()) != 0) _preferences->putString(preference_latest_version, latestVersion);
    }
    #endif
    _response.concat("<br></div>");

    if(_partitionType == 1)
    {
        _response.concat("<h4><a onclick=\"hideshowmanual();\">Manually update Nuki Hub</a></h4><div id=\"manualupdate\" style=\"display: none\">");
        _response.concat("<div id=\"rebootform\"><h4>Reboot to Nuki Hub Updater</h4>");
        _response.concat("Click on the button to reboot to the Nuki Hub updater, where you can select the latest Nuki Hub binary to update");
        _response.concat("<form action=\"/reboottoota\" method=\"get\"><br><input type=\"submit\" value=\"Reboot to Nuki Hub Updater\" /></form><br><br></div>");
        _response.concat("<div id=\"upform\"><h4>Update Nuki Hub Updater</h4>");
        _response.concat("Select the latest Nuki Hub updater binary to update the Nuki Hub updater");
        _response.concat("<form enctype=\"multipart/form-data\" action=\"/uploadota\" method=\"post\">Choose the nuki_hub_updater.bin file to upload: <input name=\"uploadedfile\" type=\"file\" accept=\".bin\" /><br/>");
    }
    else
    {
        _response.concat("<div id=\"manualupdate\">");
        _response.concat("<div id=\"rebootform\"><h4>Reboot to Nuki Hub</h4>");
        _response.concat("Click on the button to reboot to Nuki Hub");
        _response.concat("<form action=\"/reboottoota\" method=\"get\"><br><input type=\"submit\" value=\"Reboot to Nuki Hub\" /></form><br><br></div>");
        _response.concat("<div id=\"upform\"><h4>Update Nuki Hub</h4>");
        _response.concat("Select the latest Nuki Hub binary to update Nuki Hub");
        _response.concat("<form enctype=\"multipart/form-data\" action=\"/uploadota\" method=\"post\">Choose the nuki_hub.bin file to upload: <input name=\"uploadedfile\" type=\"file\" accept=\".bin\" /><br/>");
    }
    _response.concat("<br><input id=\"submitbtn\" type=\"submit\" value=\"Upload File\" /></form><br><br></div>");
    _response.concat("<div id=\"gitdiv\">");
    _response.concat("<h4>GitHub</h4><br>");
    _response.concat("<button title=\"Open latest release on GitHub\" onclick=\" window.open('");
    _response.concat(GITHUB_LATEST_RELEASE_URL);
    _response.concat("', '_blank'); return false;\">Open latest release on GitHub</button>");
    _response.concat("<br><br><button title=\"Download latest binary from GitHub\" onclick=\" window.open('");
    _response.concat(GITHUB_LATEST_RELEASE_BINARY_URL);
    _response.concat("'); return false;\">Download latest binary from GitHub</button>");
    _response.concat("<br><br><button title=\"Download latest updater binary from GitHub\" onclick=\" window.open('");
    _response.concat(GITHUB_LATEST_UPDATER_BINARY_URL);
    _response.concat("'); return false;\">Download latest updater binary from GitHub</button></div></div>");
    _response.concat("<script type=\"text/javascript\">");
    _response.concat("window.addEventListener('load', function () {");
    _response.concat("	var button = document.getElementById(\"submitbtn\");");
    _response.concat("	button.addEventListener('click',hideshow,false);");
    _response.concat("	function hideshow() {");
    _response.concat("		document.getElementById('autoupdform').style.visibility = 'hidden';");
    _response.concat("		document.getElementById('rebootform').style.visibility = 'hidden';");
    _response.concat("		document.getElementById('upform').style.visibility = 'hidden';");
    _response.concat("		document.getElementById('gitdiv').style.visibility = 'hidden';");
    _response.concat("		document.getElementById('msgdiv').style.visibility = 'visible';");
    _response.concat("	}");
    _response.concat("});");
    _response.concat("function hideshowmanual() {");
    _response.concat("	var x = document.getElementById(\"manualupdate\");");
    _response.concat("	if (x.style.display === \"none\") {");
    _response.concat("	    x.style.display = \"block\";");
    _response.concat("	} else {");
    _response.concat("	    x.style.display = \"none\";");
    _response.concat("    }");
    _response.concat("}");
    _response.concat("</script>");
    _response.concat("</body></html>");
    sendResponse(request);
}

void WebCfgServer::buildOtaCompletedHtml(AsyncWebServerRequest *request)
{
    _response = "";
    buildHtmlHeader();

    _response.concat("<div>Over-the-air update completed.<br>You will be forwarded automatically.</div>");
    _response.concat("<script type=\"text/javascript\">");
    _response.concat("window.addEventListener('load', function () {");
    _response.concat("   setTimeout(\"location.href = '/';\",10000);");
    _response.concat("});");
    _response.concat("</script>");
    _response.concat("</body></html>");
    sendResponse(request);
}

void WebCfgServer::buildHtmlHeader(String additionalHeader)
{
    _response.concat("<html><head>");
    _response.concat("<meta name='viewport' content='width=device-width, initial-scale=1'>");
    if(strcmp(additionalHeader.c_str(), "") != 0) _response.concat(additionalHeader);
    _response.concat("<link rel='stylesheet' href='/style.css'>");
    _response.concat("<title>Nuki Hub</title></head><body>");
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
    _response = "";
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
    buildHtmlHeader(header);
    _response.concat(message);
    _response.concat("</body></html>");
    sendResponse(request);
}

void WebCfgServer::sendCss(AsyncWebServerRequest *request)
{
    // escaped by https://www.cescaper.com/
    AsyncWebServerResponse *asyncResponse = request->beginResponse(200, "text/css", (const uint8_t*)stylecss, sizeof(stylecss));
    asyncResponse ->addHeader("Cache-Control", "public, max-age=3600");
    request->send(asyncResponse);
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

    for(const auto& key : keysPrefs)
    {
        if(strcmp(key, preference_show_secrets) == 0) continue;
        if(strcmp(key, preference_latest_version) == 0) continue;
        if(strcmp(key, preference_device_id_lock) == 0) continue;
        if(strcmp(key, preference_device_id_opener) == 0) continue;
        if(!redacted) if(std::find(redactedPrefs.begin(), redactedPrefs.end(), key) != redactedPrefs.end()) continue;
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

    AsyncWebServerResponse *response = request->beginChunkedResponse("application/json",
    [&](uint8_t *buffer, size_t maxlen, size_t index) -> size_t {
      size_t len = min(maxlen, jsonPretty.length() - index);
      memcpy(buffer, jsonPretty.c_str() + index, len);
      return len;
    });

    request->send(response);
}

bool WebCfgServer::processArgs(AsyncWebServerRequest *request, String& message)
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
        const AsyncWebParameter* p = request->getParam(index);
        String key = p->name();
        String value = p->value();

        if(index < params -1)
        {
            const AsyncWebParameter* next = request->getParam(index+1);
            if(key == next->name()) continue;
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
        else if(key == "MQTTOPPATH")
        {
            if(_preferences->getString(preference_mqtt_opener_path, "") != value)
            {
                _preferences->putString(preference_mqtt_opener_path, value);
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
                    if(value.toInt() != 11) _preferences->putInt(preference_network_custom_phy, 0);
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
                if (_nuki != nullptr) _nuki->disableHASS();
                if (_nukiOpener != nullptr) _nukiOpener->disableHASS();
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
        else if(key == "BESTRSSI")
        {
            if(_preferences->getBool(preference_find_best_rssi, false) != (value == "1"))
            {
                _preferences->putBool(preference_find_best_rssi, (value == "1"));
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
        else if(key == "RECNWTMQTTDIS")
        {
            if(_preferences->getBool(preference_recon_netw_on_mqtt_discon, false) != (value == "1"))
            {
                _preferences->putBool(preference_recon_netw_on_mqtt_discon, (value == "1"));
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
                if((value == "1")) _preferences->putBool(preference_register_as_app, true);
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
                if(value == "1") _preferences->putBool(preference_register_as_app, true);
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
                Log->println("ACLCONFBADVANCEDOPENER");
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
            const std::vector<char*> intPrefs = debugPreferences.getPreferencesIntKeys();

            for(const auto& key : keysPrefs)
            {
                if(doc[key].isNull()) continue;
                if(strcmp(key, preference_show_secrets) == 0) continue;
                if(strcmp(key, preference_latest_version) == 0) continue;
                if(strcmp(key, preference_device_id_lock) == 0) continue;
                if(strcmp(key, preference_device_id_opener) == 0) continue;
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
            if(!doc["securityPinCodeLock"].isNull() && _nuki != nullptr)
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
            if(!doc["securityPinCodeOpener"].isNull() && _nukiOpener != nullptr)
            {
                if(doc["securityPinCodeOpener"].as<String>().length() > 0) _nukiOpener->setPin(doc["securityPinCodeOpener"].as<int>());
                else _nukiOpener->setPin(0xffff);
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
    _response = "";
    buildHtmlHeader();

    _response.concat("<div id=\"upform\"><h4>Import configuration</h4>");
    _response.concat("<form method=\"post\" action=\"import\"><textarea id=\"importjson\" name=\"importjson\" rows=\"10\" cols=\"50\"></textarea><br/>");
    _response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Import\"></form><br><br></div>");
    _response.concat("<div id=\"gitdiv\">");
    _response.concat("<h4>Export configuration</h4><br>");
    _response.concat("<button title=\"Basic export\" onclick=\" window.open('/export', '_self'); return false;\">Basic export</button>");
    _response.concat("<br><br><button title=\"Export with redacted settings\" onclick=\" window.open('/export?redacted=1'); return false;\">Export with redacted settings</button>");
    _response.concat("<br><br><button title=\"Export with redacted settings and pairing data\" onclick=\" window.open('/export?redacted=1&pairing=1'); return false;\">Export with redacted settings and pairing data</button>");
    _response.concat("</div></body></html>");
    sendResponse(request);
}

void WebCfgServer::buildCustomNetworkConfigHtml(AsyncWebServerRequest *request)
{
    String header = "<script>window.onload=function(){var physelect=document.getElementsByName('NWCUSTPHY')[0];hideshowopt(physelect.value);physelect.addEventListener('change', function(event){var select=event.target;var selectedOption=select.options[select.selectedIndex];hideshowopt(selectedOption.getAttribute('value'));});};function hideshowopt(value){if(value>=1&&value<=3){hideopt('internalopt',true);hideopt('externalopt',false);}else if(value>=4&&value<=9){hideopt('internalopt', false);hideopt('externalopt', true);}else {hideopt('internalopt', true);hideopt('externalopt', true);}}function hideopt(opts,hide){var hideopts = document.getElementsByClassName(opts);for(var i=0;i<hideopts.length;i++){if(hide==true){hideopts[i].style.display='none';}else{hideopts[i].style.display='block';}}}</script>";
    _response = "";
    buildHtmlHeader(header);
    _response.concat("<form class=\"adapt\" method=\"post\" action=\"savecfg\">");
    _response.concat("<h3>Custom Ethernet Configuration</h3>");
    _response.concat("<table>");
    printDropDown("NWCUSTPHY", "PHY", String(_preferences->getInt(preference_network_custom_phy)), getNetworkCustomPHYOptions(), "");
    printInputField("NWCUSTADDR", "ADDR", _preferences->getInt(preference_network_custom_addr, 1), 6, "");
    #if defined(CONFIG_IDF_TARGET_ESP32)
    printDropDown("NWCUSTCLK", "CLK", String(_preferences->getInt(preference_network_custom_clk, 0)), getNetworkCustomCLKOptions(), "internalopt");
    printInputField("NWCUSTPWR", "PWR", _preferences->getInt(preference_network_custom_pwr, 12), 6, "class=\"internalopt\"");
    printInputField("NWCUSTMDIO", "MDIO", _preferences->getInt(preference_network_custom_mdio), 6, "class=\"internalopt\"");
    printInputField("NWCUSTMDC", "MDC", _preferences->getInt(preference_network_custom_mdc), 6, "class=\"internalopt\"");
    #endif
    printInputField("NWCUSTIRQ", "IRQ", _preferences->getInt(preference_network_custom_irq, -1), 6, "class=\"externalopt\"");
    printInputField("NWCUSTRST", "RST", _preferences->getInt(preference_network_custom_rst, -1), 6, "class=\"externalopt\"");
    printInputField("NWCUSTCS", "CS", _preferences->getInt(preference_network_custom_cs, -1), 6, "class=\"externalopt\"");
    printInputField("NWCUSTSCK", "SCK", _preferences->getInt(preference_network_custom_sck, -1), 6, "class=\"externalopt\"");
    printInputField("NWCUSTMISO", "MISO", _preferences->getInt(preference_network_custom_miso, -1), 6, "class=\"externalopt\"");
    printInputField("NWCUSTMOSI", "MOSI", _preferences->getInt(preference_network_custom_mosi, -1), 6, "class=\"externalopt\"");

    _response.concat("</table>");

    _response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    _response.concat("</form>");
    _response.concat("</body></html>");
    sendResponse(request);
}

void WebCfgServer::buildHtml(AsyncWebServerRequest *request)
{
    String header = "<script>let intervalId; window.onload = function() { updateInfo(); intervalId = setInterval(updateInfo, 3000); }; function updateInfo() { var request = new XMLHttpRequest(); request.open('GET', '/status', true); request.onload = () => { const obj = JSON.parse(request.responseText); if (obj.stop == 1) { clearInterval(intervalId); } for (var key of Object.keys(obj)) { if(key=='ota' && document.getElementById(key) !== null) { document.getElementById(key).innerText = \"<a href='/ota'>\" + obj[key] + \"</a>\"; } else if(document.getElementById(key) !== null) { document.getElementById(key).innerText = obj[key]; } } }; request.send(); }</script>";
    _response = "";
    buildHtmlHeader(header);

    if(_rebootRequired) _response.concat("<table><tbody><tr><td colspan=\"2\" style=\"border: 0; color: red; font-size: 32px; font-weight: bold; text-align: center;\">REBOOT REQUIRED TO APPLY SETTINGS</td></tr></tbody></table>");
    if(_preferences->getBool(preference_webserial_enabled, false)) _response.concat("<table><tbody><tr><td colspan=\"2\" style=\"border: 0; color: red; font-size: 32px; font-weight: bold; text-align: center;\">WEBSERIAL IS ENABLED, ONLY ENABLE WHEN DEBUGGING AND DISABLE ASAP</td></tr></tbody></table>");
    #ifdef DEBUG_NUKIHUB
    _response.concat("<table><tbody><tr><td colspan=\"2\" style=\"border: 0; color: red; font-size: 32px; font-weight: bold; text-align: center;\">RUNNING DEBUG BUILD, SWITCH TO RELEASE BUILD ASAP</td></tr></tbody></table>");
    #endif

    _response.concat("<h3>Info</h3><br>");
    _response.concat("<table>");

    printParameter("Hostname", _hostname.c_str(), "", "hostname");
    printParameter("MQTT Connected", _network->mqttConnectionState() > 0 ? "Yes" : "No", "", "mqttState");
    if(_nuki != nullptr)
    {
        char lockStateArr[20];
        NukiLock::lockstateToString(_nuki->keyTurnerState().lockState, lockStateArr);
        printParameter("Nuki Lock paired", _nuki->isPaired() ? ("Yes (BLE Address " + _nuki->getBleAddress().toString() + ")").c_str() : "No", "", "lockPaired");
        printParameter("Nuki Lock state", lockStateArr, "", "lockState");

        if(_nuki->isPaired())
        {
            String lockState = pinStateToString(_preferences->getInt(preference_lock_pin_status, 4));
            printParameter("Nuki Lock PIN status", lockState.c_str(), "", "lockPin");

            if(_preferences->getBool(preference_official_hybrid_enabled, false))
            {
                String offConnected = _nuki->offConnected() ? "Yes": "No";
                printParameter("Nuki Lock hybrid mode connected", offConnected.c_str(), "", "lockHybrid");
            }
        }
    }
    if(_nukiOpener != nullptr)
    {
        char openerStateArr[20];
        NukiOpener::lockstateToString(_nukiOpener->keyTurnerState().lockState, openerStateArr);
        printParameter("Nuki Opener paired", _nukiOpener->isPaired() ? ("Yes (BLE Address " + _nukiOpener->getBleAddress().toString() + ")").c_str() : "No", "", "openerPaired");

        if(_nukiOpener->keyTurnerState().nukiState == NukiOpener::State::ContinuousMode) printParameter("Nuki Opener state", "Open (Continuous Mode)", "", "openerState");
        else printParameter("Nuki Opener state", openerStateArr, "", "openerState");

        if(_nukiOpener->isPaired())
        {
            String openerState = pinStateToString(_preferences->getInt(preference_opener_pin_status, 4));
            printParameter("Nuki Opener PIN status", openerState.c_str(), "", "openerPin");
        }
    }
    printParameter("Firmware", NUKI_HUB_VERSION, "/info", "firmware");
    if(_preferences->getBool(preference_check_updates)) printParameter("Latest Firmware", _preferences->getString(preference_latest_version).c_str(), "/ota", "ota");
    _response.concat("</table><br>");
    _response.concat("<ul id=\"tblnav\">");
    buildNavigationMenuEntry("MQTT and Network Configuration", "/mqttconfig",  _brokerConfigured ? "" : "Please configure MQTT broker");
    buildNavigationMenuEntry("Nuki Configuration", "/nukicfg");
    buildNavigationMenuEntry("Access Level Configuration", "/acclvl");
    buildNavigationMenuEntry("Credentials", "/cred", _pinsConfigured ? "" : "Please configure PIN");
    buildNavigationMenuEntry("GPIO Configuration", "/gpiocfg");
    buildNavigationMenuEntry("Firmware update", "/ota");
    buildNavigationMenuEntry("Import/Export Configuration", "/impexpcfg");
    if(_preferences->getInt(preference_network_hardware, 0) == 11)
    {
        buildNavigationMenuEntry("Custom Ethernet Configuration", "/custntw");
    }
    if (_preferences->getBool(preference_publish_debug_info, false))
    {
        buildNavigationMenuEntry("Advanced Configuration", "/advanced");
    }
    if(_preferences->getBool(preference_webserial_enabled, false))
    {
        buildNavigationMenuEntry("Open Webserial", "/webserial");
    }
    #ifndef CONFIG_IDF_TARGET_ESP32H2
    if(_allowRestartToPortal) buildNavigationMenuEntry("Configure Wi-Fi", "/wifi");
    #endif
    buildNavigationMenuEntry("Reboot Nuki Hub", "/reboot");
    _response.concat("</ul></body></html>");
    sendResponse(request);
}

void WebCfgServer::buildCredHtml(AsyncWebServerRequest *request)
{
    _response = "";
    buildHtmlHeader();
    _response.concat("<form class=\"adapt\" method=\"post\" action=\"savecfg\">");
    _response.concat("<h3>Credentials</h3>");
    _response.concat("<table>");
    printInputField("CREDUSER", "User (# to clear)", _preferences->getString(preference_cred_user).c_str(), 30, "", false, true);
    printInputField("CREDPASS", "Password", "*", 30, "", true, true);
    printInputField("CREDPASSRE", "Retype password", "*", 30, "", true);
    _response.concat("</table>");
    _response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    _response.concat("</form>");
    if(_nuki != nullptr)
    {
        _response.concat("<br><br><form class=\"adapt\" method=\"post\" action=\"savecfg\">");
        _response.concat("<h3>Nuki Lock PIN</h3>");
        _response.concat("<table>");
        printInputField("NUKIPIN", "PIN Code (# to clear)", "*", 20, "", true);
        _response.concat("</table>");
        _response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
        _response.concat("</form>");
    }
    if(_nukiOpener != nullptr)
    {
        _response.concat("<br><br><form class=\"adapt\" method=\"post\" action=\"savecfg\">");
        _response.concat("<h3>Nuki Opener PIN</h3>");
        _response.concat("<table>");
        printInputField("NUKIOPPIN", "PIN Code (# to clear)", "*", 20, "", true);
        _response.concat("</table>");
        _response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
        _response.concat("</form>");
    }
    if(_nuki != nullptr)
    {
        _response.concat("<br><br><h3>Unpair Nuki Lock</h3>");
        _response.concat("<form class=\"adapt\" method=\"post\" action=\"/unpairlock\">");
        _response.concat("<table>");
        String message = "Type ";
        message.concat(_confirmCode);
        message.concat(" to confirm unpair");
        printInputField("CONFIRMTOKEN", message.c_str(), "", 10, "");
        _response.concat("</table>");
        _response.concat("<br><button type=\"submit\">OK</button></form>");
    }
    if(_nukiOpener != nullptr)
    {
        _response.concat("<br><br><h3>Unpair Nuki Opener</h3>");
        _response.concat("<form class=\"adapt\" method=\"post\" action=\"/unpairopener\">");
        _response.concat("<table>");
        String message = "Type ";
        message.concat(_confirmCode);
        message.concat(" to confirm unpair");
        printInputField("CONFIRMTOKEN", message.c_str(), "", 10, "");
        _response.concat("</table>");
        _response.concat("<br><button type=\"submit\">OK</button></form>");
    }
    _response.concat("<br><br><h3>Factory reset Nuki Hub</h3>");
    _response.concat("<h4 class=\"warning\">This will reset all settings to default and unpair Nuki Lock and/or Opener.");
    #ifndef CONFIG_IDF_TARGET_ESP32H2
    _response.concat("Optionally will also reset WiFi settings and reopen WiFi manager portal.");
    #endif
    _response.concat("</h4>");
    _response.concat("<form class=\"adapt\" method=\"post\" action=\"/factoryreset\">");
    _response.concat("<table>");
    String message = "Type ";
    message.concat(_confirmCode);
    message.concat(" to confirm factory reset");
    printInputField("CONFIRMTOKEN", message.c_str(), "", 10, "");
    #ifndef CONFIG_IDF_TARGET_ESP32H2
    printCheckBox("WIFI", "Also reset WiFi settings", false, "");
    #endif
    _response.concat("</table>");
    _response.concat("<br><button type=\"submit\">OK</button></form>");
    _response.concat("</body></html>");
    sendResponse(request);
}

void WebCfgServer::buildMqttConfigHtml(AsyncWebServerRequest *request)
{
    _response = "";
    buildHtmlHeader();
    _response.concat("<form class=\"adapt\" method=\"post\" action=\"savecfg\">");
    _response.concat("<h3>Basic MQTT and Network Configuration</h3>");
    _response.concat("<table>");
    printInputField("HOSTNAME", "Host name", _preferences->getString(preference_hostname).c_str(), 100, "");
    printInputField("MQTTSERVER", "MQTT Broker", _preferences->getString(preference_mqtt_broker).c_str(), 100, "");
    printInputField("MQTTPORT", "MQTT Broker port", _preferences->getInt(preference_mqtt_broker_port), 5, "");
    printInputField("MQTTUSER", "MQTT User (# to clear)", _preferences->getString(preference_mqtt_user).c_str(), 30, "", false, true);
    printInputField("MQTTPASS", "MQTT Password", "*", 30, "", true, true);
    _response.concat("</table><br>");

    _response.concat("<h3>Advanced MQTT and Network Configuration</h3>");
    _response.concat("<table>");
    printInputField("HASSDISCOVERY", "Home Assistant discovery topic (empty to disable; usually homeassistant)", _preferences->getString(preference_mqtt_hass_discovery).c_str(), 30, "");
    printInputField("HASSCUURL", "Home Assistant device configuration URL (empty to use http://LOCALIP; fill when using a reverse proxy for example)", _preferences->getString(preference_mqtt_hass_cu_url).c_str(), 261, "");
    if(_preferences->getBool(preference_opener_enabled, false)) printCheckBox("OPENERCONT", "Set Nuki Opener Lock/Unlock action in Home Assistant to Continuous mode", _preferences->getBool(preference_opener_continuous_mode), "");
    printTextarea("MQTTCA", "MQTT SSL CA Certificate (*, optional)", _preferences->getString(preference_mqtt_ca).c_str(), TLS_CA_MAX_SIZE, _network->encryptionSupported(), true);
    printTextarea("MQTTCRT", "MQTT SSL Client Certificate (*, optional)", _preferences->getString(preference_mqtt_crt).c_str(), TLS_CERT_MAX_SIZE, _network->encryptionSupported(), true);
    printTextarea("MQTTKEY", "MQTT SSL Client Key (*, optional)", _preferences->getString(preference_mqtt_key).c_str(), TLS_KEY_MAX_SIZE, _network->encryptionSupported(), true);
    printDropDown("NWHW", "Network hardware", String(_preferences->getInt(preference_network_hardware)), getNetworkDetectionOptions(), "");
    #ifndef CONFIG_IDF_TARGET_ESP32H2
    printCheckBox("NWHWWIFIFB", "Disable fallback to Wi-Fi / Wi-Fi config portal", _preferences->getBool(preference_network_wifi_fallback_disabled), "");
    printCheckBox("BESTRSSI", "Connect to AP with the best signal in an environment with multiple APs with the same SSID", _preferences->getBool(preference_find_best_rssi), "");
    printInputField("RSSI", "RSSI Publish interval (seconds; -1 to disable)", _preferences->getInt(preference_rssi_publish_interval), 6, "");
    #endif
    printInputField("NETTIMEOUT", "MQTT Timeout until restart (seconds; -1 to disable)", _preferences->getInt(preference_network_timeout), 5, "");
    printCheckBox("RSTDISC", "Restart on disconnect", _preferences->getBool(preference_restart_on_disconnect), "");
    printCheckBox("RECNWTMQTTDIS", "Reconnect network on MQTT connection failure", _preferences->getBool(preference_recon_netw_on_mqtt_discon), "");
    printCheckBox("MQTTLOG", "Enable MQTT logging", _preferences->getBool(preference_mqtt_log_enabled), "");
    printCheckBox("CHECKUPDATE", "Check for Firmware Updates every 24h", _preferences->getBool(preference_check_updates), "");
    printCheckBox("UPDATEMQTT", "Allow updating using MQTT", _preferences->getBool(preference_update_from_mqtt), "");
    printCheckBox("DISNONJSON", "Disable some extraneous non-JSON topics", _preferences->getBool(preference_disable_non_json), "");
    printCheckBox("OFFHYBRID", "Enable hybrid official MQTT and Nuki Hub setup", _preferences->getBool(preference_official_hybrid_enabled), "");
    printCheckBox("HYBRIDACT", "Enable sending actions through official MQTT", _preferences->getBool(preference_official_hybrid_actions), "");
    printInputField("HYBRIDTIMER", "Time between status updates when official MQTT is offline (seconds)", _preferences->getInt(preference_query_interval_hybrid_lockstate), 5, "");
    // printCheckBox("HYBRIDRETRY", "Retry command sent using official MQTT over BLE if failed", _preferences->getBool(preference_official_hybrid_retry), ""); // NOT IMPLEMENTED (YET?)
    _response.concat("</table>");
    _response.concat("* If no encryption is configured for the MQTT broker, leave empty.<br><br>");

    _response.concat("<h3>IP Address assignment</h3>");
    _response.concat("<table>");
    printCheckBox("DHCPENA", "Enable DHCP", _preferences->getBool(preference_ip_dhcp_enabled), "");
    printInputField("IPADDR", "Static IP address", _preferences->getString(preference_ip_address).c_str(), 15, "");
    printInputField("IPSUB", "Subnet", _preferences->getString(preference_ip_subnet).c_str(), 15, "");
    printInputField("IPGTW", "Default gateway", _preferences->getString(preference_ip_gateway).c_str(), 15, "");
    printInputField("DNSSRV", "DNS Server", _preferences->getString(preference_ip_dns_server).c_str(), 15, "");
    _response.concat("</table>");
    _response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    _response.concat("</form>");
    _response.concat("</body></html>");
    sendResponse(request);
}

void WebCfgServer::buildAdvancedConfigHtml(AsyncWebServerRequest *request)
{
    _response = "";
    buildHtmlHeader();
    _response.concat("<form class=\"adapt\" method=\"post\" action=\"savecfg\">");
    _response.concat("<h3>Advanced Configuration</h3>");
    _response.concat("<h4 class=\"warning\">Warning: Changing these settings can lead to bootloops that might require you to erase the ESP32 and reflash nukihub using USB/serial</h4>");
    _response.concat("<table>");
    _response.concat("<tr><td>Current bootloop prevention state</td><td>");
    _response.concat(_preferences->getBool(preference_enable_bootloop_reset, false) ? "Enabled" : "Disabled");
    _response.concat("</td></tr>");
    printCheckBox("WEBLOG", "Enable WebSerial logging", _preferences->getBool(preference_webserial_enabled), "");
    printCheckBox("BTLPRST", "Enable Bootloop prevention (Try to reset these settings to default on bootloop)", true, "");
    printInputField("BUFFSIZE", "Char buffer size (min 4096, max 32768)", _preferences->getInt(preference_buffer_size, CHAR_BUFFER_SIZE), 6, "");
    _response.concat("<tr><td>Advised minimum char buffer size based on current settings</td><td id=\"mincharbuffer\"></td>");
    printInputField("TSKNTWK", "Task size Network (min 12288, max 32768)", _preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE), 6, "");
    _response.concat("<tr><td>Advised minimum network task size based on current settings</td><td id=\"minnetworktask\"></td>");
    printInputField("TSKNUKI", "Task size Nuki (min 8192, max 32768)", _preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE), 6, "");
    printInputField("ALMAX", "Max auth log entries (min 1, max 50)", _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG), 3, "id=\"inputmaxauthlog\"");
    printInputField("KPMAX", "Max keypad entries (min 1, max 100)", _preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD), 3, "id=\"inputmaxkeypad\"");
    printInputField("TCMAX", "Max timecontrol entries (min 1, max 50)", _preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL), 3, "id=\"inputmaxtimecontrol\"");
    printInputField("AUTHMAX", "Max authorization entries (min 1, max 50)", _preferences->getInt(preference_auth_max_entries, MAX_AUTH), 3, "id=\"inputmaxauth\"");
    printCheckBox("SHOWSECRETS", "Show Pairing secrets on Info page", _preferences->getBool(preference_show_secrets), "");
    if(_preferences->getBool(preference_lock_enabled, true))
    {
        printCheckBox("LCKMANPAIR", "Manually set lock pairing data (enable to save values below)", false, "");
        printInputField("LCKBLEADDR", "currentBleAddress", "", 12, "");
        printInputField("LCKSECRETK", "secretKeyK", "", 64, "");
        printInputField("LCKAUTHID", "authorizationId", "", 8, "");
    }
    if(_preferences->getBool(preference_opener_enabled, false))
    {
        printCheckBox("OPNMANPAIR", "Manually set opener pairing data (enable to save values below)", false, "");
        printInputField("OPNBLEADDR", "currentBleAddress", "", 12, "");
        printInputField("OPNSECRETK", "secretKeyK", "", 64, "");
        printInputField("OPNAUTHID", "authorizationId", "", 8, "");
    }
    printInputField("OTAUPD", "Custom URL to update Nuki Hub updater", "", 255, "");
    printInputField("OTAMAIN", "Custom URL to update Nuki Hub", "", 255, "");
    _response.concat("</table>");

    _response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    _response.concat("</form>");
    _response.concat("</body><script>window.onload = function() { document.getElementById(\"inputmaxauthlog\").addEventListener(\"keyup\", calculate);document.getElementById(\"inputmaxkeypad\").addEventListener(\"keyup\", calculate);document.getElementById(\"inputmaxtimecontrol\").addEventListener(\"keyup\", calculate);document.getElementById(\"inputmaxauth\").addEventListener(\"keyup\", calculate); calculate(); }; function calculate() { var auth = document.getElementById(\"inputmaxauth\").value; var authlog = document.getElementById(\"inputmaxauthlog\").value; var keypad = document.getElementById(\"inputmaxkeypad\").value; var timecontrol = document.getElementById(\"inputmaxtimecontrol\").value; var charbuf = 0; var networktask = 0; var sizeauth = 0; var sizeauthlog = 0; var sizekeypad = 0; var sizetimecontrol = 0; if(auth > 0) { sizeauth = 300 * auth; } if(authlog > 0) { sizeauthlog = 280 * authlog; } if(keypad > 0) { sizekeypad = 350 * keypad; } if(timecontrol > 0) { sizetimecontrol = 120 * timecontrol; } charbuf = sizetimecontrol; networktask = 10240 + sizetimecontrol; if(sizeauthlog>sizekeypad && sizeauthlog>sizetimecontrol && sizeauthlog>sizeauth) { charbuf = sizeauthlog; networktask = 10240 + sizeauthlog;} else if(sizekeypad>sizeauthlog && sizekeypad>sizetimecontrol && sizekeypad>sizeauth) { charbuf = sizekeypad; networktask = 10240 + sizekeypad;} else if(sizeauth>sizeauthlog && sizeauth>sizetimecontrol && sizeauth>sizekeypad) { charbuf = sizeauth; networktask = 10240 + sizeauth;} if(charbuf<4096) { charbuf = 4096; } else if (charbuf>32768) { charbuf = 32768; } if(networktask<12288) { networktask = 12288; } else if (networktask>32768) { networktask = 32768; } document.getElementById(\"mincharbuffer\").innerHTML = charbuf; document.getElementById(\"minnetworktask\").innerHTML = networktask; }</script></html>");
    sendResponse(request);
}

void WebCfgServer::buildStatusHtml(AsyncWebServerRequest *request)
{
    _response = "";
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
    _response.concat(_resbuf);
    sendResponse(request);
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

void WebCfgServer::buildAccLvlHtml(AsyncWebServerRequest *request)
{
    _response = "";
    buildHtmlHeader();

    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    _response.concat("<form method=\"post\" action=\"savecfg\">");
    _response.concat("<input type=\"hidden\" name=\"ACLLVLCHANGED\" value=\"1\">");
    _response.concat("<h3>Nuki General Access Control</h3>");
    _response.concat("<table><tr><th>Setting</th><th>Enabled</th></tr>");
    printCheckBox("CONFPUB", "Publish Nuki configuration information", _preferences->getBool(preference_conf_info_enabled, true), "");

    if((_nuki != nullptr && _nuki->hasKeypad()) || (_nukiOpener != nullptr && _nukiOpener->hasKeypad()))
    {
        printCheckBox("KPPUB", "Publish keypad entries information", _preferences->getBool(preference_keypad_info_enabled), "");
        printCheckBox("KPPER", "Publish a topic per keypad entry and create HA sensor", _preferences->getBool(preference_keypad_topic_per_entry), "");
        printCheckBox("KPCODE", "Also publish keypad codes (<span class=\"warning\">Disadvised for security reasons</span>)", _preferences->getBool(preference_keypad_publish_code, false), "");
        printCheckBox("KPENA", "Add, modify and delete keypad codes", _preferences->getBool(preference_keypad_control_enabled), "");
    }
    printCheckBox("TCPUB", "Publish time control entries information", _preferences->getBool(preference_timecontrol_info_enabled), "");
    printCheckBox("TCPER", "Publish a topic per time control entry and create HA sensor", _preferences->getBool(preference_timecontrol_topic_per_entry), "");
    printCheckBox("TCENA", "Add, modify and delete time control entries", _preferences->getBool(preference_timecontrol_control_enabled), "");
    printCheckBox("AUTHPUB", "Publish authorization entries information", _preferences->getBool(preference_auth_info_enabled), "");
    printCheckBox("AUTHPER", "Publish a topic per authorization entry and create HA sensor", _preferences->getBool(preference_auth_topic_per_entry), "");
    printCheckBox("AUTHENA", "Modify and delete authorization entries", _preferences->getBool(preference_auth_control_enabled), "");
    printCheckBox("PUBAUTH", "Publish authorization log", _preferences->getBool(preference_publish_authdata), "");
    _response.concat("</table><br>");
    _response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");

    if(_nuki != nullptr)
    {
        uint32_t basicLockConfigAclPrefs[16];
        _preferences->getBytes(preference_conf_lock_basic_acl, &basicLockConfigAclPrefs, sizeof(basicLockConfigAclPrefs));
        uint32_t advancedLockConfigAclPrefs[22];
        _preferences->getBytes(preference_conf_lock_advanced_acl, &advancedLockConfigAclPrefs, sizeof(advancedLockConfigAclPrefs));

        _response.concat("<h3>Nuki Lock Access Control</h3>");
        _response.concat("<input type=\"button\" value=\"Allow all\" style=\"margin-right: 10px;\" onclick=\"");
        _response.concat("for(el of document.getElementsByClassName('chk_access_lock')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=true;}\">");
        _response.concat("<input type=\"button\" value=\"Disallow all\" onclick=\"");
        _response.concat("for(el of document.getElementsByClassName('chk_access_lock')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=false;}\">");
        _response.concat("<table><tr><th>Action</th><th>Allowed</th></tr>");

        printCheckBox("ACLLCKLCK", "Lock", ((int)aclPrefs[0] == 1), "chk_access_lock");
        printCheckBox("ACLLCKUNLCK", "Unlock", ((int)aclPrefs[1] == 1), "chk_access_lock");
        printCheckBox("ACLLCKUNLTCH", "Unlatch", ((int)aclPrefs[2] == 1), "chk_access_lock");
        printCheckBox("ACLLCKLNG", "Lock N Go", ((int)aclPrefs[3] == 1), "chk_access_lock");
        printCheckBox("ACLLCKLNGU", "Lock N Go Unlatch", ((int)aclPrefs[4] == 1), "chk_access_lock");
        printCheckBox("ACLLCKFLLCK", "Full Lock", ((int)aclPrefs[5] == 1), "chk_access_lock");
        printCheckBox("ACLLCKFOB1", "Fob Action 1", ((int)aclPrefs[6] == 1), "chk_access_lock");
        printCheckBox("ACLLCKFOB2", "Fob Action 2", ((int)aclPrefs[7] == 1), "chk_access_lock");
        printCheckBox("ACLLCKFOB3", "Fob Action 3", ((int)aclPrefs[8] == 1), "chk_access_lock");
        _response.concat("</table><br>");

        _response.concat("<h3>Nuki Lock Config Control (Requires PIN to be set)</h3>");
        _response.concat("<input type=\"button\" value=\"Allow all\" style=\"margin-right: 10px;\" onclick=\"");
        _response.concat("for(el of document.getElementsByClassName('chk_config_lock')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=true;}\">");
        _response.concat("<input type=\"button\" value=\"Disallow all\" onclick=\"");
        _response.concat("for(el of document.getElementsByClassName('chk_config_lock')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=false;}\">");
        _response.concat("<table><tr><th>Change</th><th>Allowed</th></tr>");

        printCheckBox("CONFLCKNAME", "Name", ((int)basicLockConfigAclPrefs[0] == 1), "chk_config_lock");
        printCheckBox("CONFLCKLAT", "Latitude", ((int)basicLockConfigAclPrefs[1] == 1), "chk_config_lock");
        printCheckBox("CONFLCKLONG", "Longitude", ((int)basicLockConfigAclPrefs[2] == 1), "chk_config_lock");
        printCheckBox("CONFLCKAUNL", "Auto unlatch", ((int)basicLockConfigAclPrefs[3] == 1), "chk_config_lock");
        printCheckBox("CONFLCKPRENA", "Pairing enabled", ((int)basicLockConfigAclPrefs[4] == 1), "chk_config_lock");
        printCheckBox("CONFLCKBTENA", "Button enabled", ((int)basicLockConfigAclPrefs[5] == 1), "chk_config_lock");
        printCheckBox("CONFLCKLEDENA", "LED flash enabled", ((int)basicLockConfigAclPrefs[6] == 1), "chk_config_lock");
        printCheckBox("CONFLCKLEDBR", "LED brightness", ((int)basicLockConfigAclPrefs[7] == 1), "chk_config_lock");
        printCheckBox("CONFLCKTZOFF", "Timezone offset", ((int)basicLockConfigAclPrefs[8] == 1), "chk_config_lock");
        printCheckBox("CONFLCKDSTM", "DST mode", ((int)basicLockConfigAclPrefs[9] == 1), "chk_config_lock");
        printCheckBox("CONFLCKFOB1", "Fob Action 1", ((int)basicLockConfigAclPrefs[10] == 1), "chk_config_lock");
        printCheckBox("CONFLCKFOB2", "Fob Action 2", ((int)basicLockConfigAclPrefs[11] == 1), "chk_config_lock");
        printCheckBox("CONFLCKFOB3", "Fob Action 3", ((int)basicLockConfigAclPrefs[12] == 1), "chk_config_lock");
        printCheckBox("CONFLCKSGLLCK", "Single Lock", ((int)basicLockConfigAclPrefs[13] == 1), "chk_config_lock");
        printCheckBox("CONFLCKADVM", "Advertising Mode", ((int)basicLockConfigAclPrefs[14] == 1), "chk_config_lock");
        printCheckBox("CONFLCKTZID", "Timezone ID", ((int)basicLockConfigAclPrefs[15] == 1), "chk_config_lock");

        printCheckBox("CONFLCKUPOD", "Unlocked Position Offset Degrees", ((int)advancedLockConfigAclPrefs[0] == 1), "chk_config_lock");
        printCheckBox("CONFLCKLPOD", "Locked Position Offset Degrees", ((int)advancedLockConfigAclPrefs[1] == 1), "chk_config_lock");
        printCheckBox("CONFLCKSLPOD", "Single Locked Position Offset Degrees", ((int)advancedLockConfigAclPrefs[2] == 1), "chk_config_lock");
        printCheckBox("CONFLCKUTLTOD", "Unlocked To Locked Transition Offset Degrees", ((int)advancedLockConfigAclPrefs[3] == 1), "chk_config_lock");
        printCheckBox("CONFLCKLNGT", "Lock n Go timeout", ((int)advancedLockConfigAclPrefs[4] == 1), "chk_config_lock");
        printCheckBox("CONFLCKSBPA", "Single button press action", ((int)advancedLockConfigAclPrefs[5] == 1), "chk_config_lock");
        printCheckBox("CONFLCKDBPA", "Double button press action", ((int)advancedLockConfigAclPrefs[6] == 1), "chk_config_lock");
        printCheckBox("CONFLCKDC", "Detached cylinder", ((int)advancedLockConfigAclPrefs[7] == 1), "chk_config_lock");
        printCheckBox("CONFLCKBATT", "Battery type", ((int)advancedLockConfigAclPrefs[8] == 1), "chk_config_lock");
        printCheckBox("CONFLCKABTD", "Automatic battery type detection", ((int)advancedLockConfigAclPrefs[9] == 1), "chk_config_lock");
        printCheckBox("CONFLCKUNLD", "Unlatch duration", ((int)advancedLockConfigAclPrefs[10] == 1), "chk_config_lock");
        printCheckBox("CONFLCKALT", "Auto lock timeout", ((int)advancedLockConfigAclPrefs[11] == 1), "chk_config_lock");
        printCheckBox("CONFLCKAUNLD", "Auto unlock disabled", ((int)advancedLockConfigAclPrefs[12] == 1), "chk_config_lock");
        printCheckBox("CONFLCKNMENA", "Nightmode enabled", ((int)advancedLockConfigAclPrefs[13] == 1), "chk_config_lock");
        printCheckBox("CONFLCKNMST", "Nightmode start time", ((int)advancedLockConfigAclPrefs[14] == 1), "chk_config_lock");
        printCheckBox("CONFLCKNMET", "Nightmode end time", ((int)advancedLockConfigAclPrefs[15] == 1), "chk_config_lock");
        printCheckBox("CONFLCKNMALENA", "Nightmode auto lock enabled", ((int)advancedLockConfigAclPrefs[16] == 1), "chk_config_lock");
        printCheckBox("CONFLCKNMAULD", "Nightmode auto unlock disabled", ((int)advancedLockConfigAclPrefs[17] == 1), "chk_config_lock");
        printCheckBox("CONFLCKNMLOS", "Nightmode immediate lock on start", ((int)advancedLockConfigAclPrefs[18] == 1), "chk_config_lock");
        printCheckBox("CONFLCKALENA", "Auto lock enabled", ((int)advancedLockConfigAclPrefs[19] == 1), "chk_config_lock");
        printCheckBox("CONFLCKIALENA", "Immediate auto lock enabled", ((int)advancedLockConfigAclPrefs[20] == 1), "chk_config_lock");
        printCheckBox("CONFLCKAUENA", "Auto update enabled", ((int)advancedLockConfigAclPrefs[21] == 1), "chk_config_lock");
        _response.concat("</table><br>");
        _response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    }
    if(_nukiOpener != nullptr)
    {
        uint32_t basicOpenerConfigAclPrefs[14];
        _preferences->getBytes(preference_conf_opener_basic_acl, &basicOpenerConfigAclPrefs, sizeof(basicOpenerConfigAclPrefs));
        uint32_t advancedOpenerConfigAclPrefs[20];
        _preferences->getBytes(preference_conf_opener_advanced_acl, &advancedOpenerConfigAclPrefs, sizeof(advancedOpenerConfigAclPrefs));

        _response.concat("<h3>Nuki Opener Access Control</h3>");
        _response.concat("<input type=\"button\" value=\"Allow all\" style=\"margin-right: 10px;\" onclick=\"");
        _response.concat("for(el of document.getElementsByClassName('chk_access_opener')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=true;}\">");
        _response.concat("<input type=\"button\" value=\"Disallow all\" onclick=\"");
        _response.concat("for(el of document.getElementsByClassName('chk_access_opener')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=false;}\">");
        _response.concat("<table><tr><th>Action</th><th>Allowed</th></tr>");

        printCheckBox("ACLOPNUNLCK", "Activate Ring-to-Open", ((int)aclPrefs[9] == 1), "chk_access_opener");
        printCheckBox("ACLOPNLCK", "Deactivate Ring-to-Open", ((int)aclPrefs[10] == 1), "chk_access_opener");
        printCheckBox("ACLOPNUNLTCH", "Electric Strike Actuation", ((int)aclPrefs[11] == 1), "chk_access_opener");
        printCheckBox("ACLOPNUNLCKCM", "Activate Continuous Mode", ((int)aclPrefs[12] == 1), "chk_access_opener");
        printCheckBox("ACLOPNLCKCM", "Deactivate Continuous Mode", ((int)aclPrefs[13] == 1), "chk_access_opener");
        printCheckBox("ACLOPNFOB1", "Fob Action 1", ((int)aclPrefs[14] == 1), "chk_access_opener");
        printCheckBox("ACLOPNFOB2", "Fob Action 2", ((int)aclPrefs[15] == 1), "chk_access_opener");
        printCheckBox("ACLOPNFOB3", "Fob Action 3", ((int)aclPrefs[16] == 1), "chk_access_opener");
        _response.concat("</table><br>");

        _response.concat("<h3>Nuki Opener Config Control (Requires PIN to be set)</h3>");
        _response.concat("<input type=\"button\" value=\"Allow all\" style=\"margin-right: 10px;\" onclick=\"");
        _response.concat("for(el of document.getElementsByClassName('chk_config_opener')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=true;}\">");
        _response.concat("<input type=\"button\" value=\"Disallow all\" onclick=\"");
        _response.concat("for(el of document.getElementsByClassName('chk_config_opener')){if(el.constructor.name==='HTMLInputElement'&amp;&amp;el.type==='checkbox')el.checked=false;}\">");
        _response.concat("<table><tr><th>Change</th><th>Allowed</th></tr>");

        printCheckBox("CONFOPNNAME", "Name", ((int)basicOpenerConfigAclPrefs[0] == 1), "chk_config_opener");
        printCheckBox("CONFOPNLAT", "Latitude", ((int)basicOpenerConfigAclPrefs[1] == 1), "chk_config_opener");
        printCheckBox("CONFOPNLONG", "Longitude", ((int)basicOpenerConfigAclPrefs[2] == 1), "chk_config_opener");
        printCheckBox("CONFOPNPRENA", "Pairing enabled", ((int)basicOpenerConfigAclPrefs[3] == 1), "chk_config_opener");
        printCheckBox("CONFOPNBTENA", "Button enabled", ((int)basicOpenerConfigAclPrefs[4] == 1), "chk_config_opener");
        printCheckBox("CONFOPNLEDENA", "LED flash enabled", ((int)basicOpenerConfigAclPrefs[5] == 1), "chk_config_opener");
        printCheckBox("CONFOPNTZOFF", "Timezone offset", ((int)basicOpenerConfigAclPrefs[6] == 1), "chk_config_opener");
        printCheckBox("CONFOPNDSTM", "DST mode", ((int)basicOpenerConfigAclPrefs[7] == 1), "chk_config_opener");
        printCheckBox("CONFOPNFOB1", "Fob Action 1", ((int)basicOpenerConfigAclPrefs[8] == 1), "chk_config_opener");
        printCheckBox("CONFOPNFOB2", "Fob Action 2", ((int)basicOpenerConfigAclPrefs[9] == 1), "chk_config_opener");
        printCheckBox("CONFOPNFOB3", "Fob Action 3", ((int)basicOpenerConfigAclPrefs[10] == 1), "chk_config_opener");
        printCheckBox("CONFOPNOPM", "Operating Mode", ((int)basicOpenerConfigAclPrefs[11] == 1), "chk_config_opener");
        printCheckBox("CONFOPNADVM", "Advertising Mode", ((int)basicOpenerConfigAclPrefs[12] == 1), "chk_config_opener");
        printCheckBox("CONFOPNTZID", "Timezone ID", ((int)basicOpenerConfigAclPrefs[13] == 1), "chk_config_opener");

        printCheckBox("CONFOPNICID", "Intercom ID", ((int)advancedOpenerConfigAclPrefs[0] == 1), "chk_config_opener");
        printCheckBox("CONFOPNBUSMS", "BUS mode Switch", ((int)advancedOpenerConfigAclPrefs[1] == 1), "chk_config_opener");
        printCheckBox("CONFOPNSCDUR", "Short Circuit Duration", ((int)advancedOpenerConfigAclPrefs[2] == 1), "chk_config_opener");
        printCheckBox("CONFOPNESD", "Eletric Strike Delay", ((int)advancedOpenerConfigAclPrefs[3] == 1), "chk_config_opener");
        printCheckBox("CONFOPNRESD", "Random Electric Strike Delay", ((int)advancedOpenerConfigAclPrefs[4] == 1), "chk_config_opener");
        printCheckBox("CONFOPNESDUR", "Electric Strike Duration", ((int)advancedOpenerConfigAclPrefs[5] == 1), "chk_config_opener");
        printCheckBox("CONFOPNDRTOAR", "Disable RTO after ring", ((int)advancedOpenerConfigAclPrefs[6] == 1), "chk_config_opener");
        printCheckBox("CONFOPNRTOT", "RTO timeout", ((int)advancedOpenerConfigAclPrefs[7] == 1), "chk_config_opener");
        printCheckBox("CONFOPNDRBSUP", "Doorbell suppression", ((int)advancedOpenerConfigAclPrefs[8] == 1), "chk_config_opener");
        printCheckBox("CONFOPNDRBSUPDUR", "Doorbell suppression duration", ((int)advancedOpenerConfigAclPrefs[9] == 1), "chk_config_opener");
        printCheckBox("CONFOPNSRING", "Sound Ring", ((int)advancedOpenerConfigAclPrefs[10] == 1), "chk_config_opener");
        printCheckBox("CONFOPNSOPN", "Sound Open", ((int)advancedOpenerConfigAclPrefs[11] == 1), "chk_config_opener");
        printCheckBox("CONFOPNSRTO", "Sound RTO", ((int)advancedOpenerConfigAclPrefs[12] == 1), "chk_config_opener");
        printCheckBox("CONFOPNSCM", "Sound CM", ((int)advancedOpenerConfigAclPrefs[13] == 1), "chk_config_opener");
        printCheckBox("CONFOPNSCFRM", "Sound confirmation", ((int)advancedOpenerConfigAclPrefs[14] == 1), "chk_config_opener");
        printCheckBox("CONFOPNSLVL", "Sound level", ((int)advancedOpenerConfigAclPrefs[15] == 1), "chk_config_opener");
        printCheckBox("CONFOPNSBPA", "Single button press action", ((int)advancedOpenerConfigAclPrefs[16] == 1), "chk_config_opener");
        printCheckBox("CONFOPNDBPA", "Double button press action", ((int)advancedOpenerConfigAclPrefs[17] == 1), "chk_config_opener");
        printCheckBox("CONFOPNBATT", "Battery type", ((int)advancedOpenerConfigAclPrefs[18] == 1), "chk_config_opener");
        printCheckBox("CONFOPNABTD", "Automatic battery type detection", ((int)advancedOpenerConfigAclPrefs[19] == 1), "chk_config_opener");
        _response.concat("</table><br>");
        _response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    }
    _response.concat("</form>");
    _response.concat("</body></html>");
    sendResponse(request);
}

void WebCfgServer::buildNukiConfigHtml(AsyncWebServerRequest *request)
{
    _response = "";
    buildHtmlHeader();
    _response.concat("<form class=\"adapt\" method=\"post\" action=\"savecfg\">");
    _response.concat("<h3>Basic Nuki Configuration</h3>");
    _response.concat("<table>");
    printCheckBox("LOCKENA", "Nuki Lock enabled", _preferences->getBool(preference_lock_enabled), "");
    if(_preferences->getBool(preference_lock_enabled)) printInputField("MQTTPATH", "MQTT Nuki Lock Path", _preferences->getString(preference_mqtt_lock_path).c_str(), 180, "");
    printCheckBox("OPENA", "Nuki Opener enabled", _preferences->getBool(preference_opener_enabled), "");
    if(_preferences->getBool(preference_opener_enabled)) printInputField("MQTTOPPATH", "MQTT Nuki Opener Path", _preferences->getString(preference_mqtt_opener_path).c_str(), 180, "");
    _response.concat("</table><br>");
    _response.concat("<h3>Advanced Nuki Configuration</h3>");
    _response.concat("<table>");

    printInputField("LSTINT", "Query interval lock state (seconds)", _preferences->getInt(preference_query_interval_lockstate), 10, "");
    printInputField("CFGINT", "Query interval configuration (seconds)", _preferences->getInt(preference_query_interval_configuration), 10, "");
    printInputField("BATINT", "Query interval battery (seconds)", _preferences->getInt(preference_query_interval_battery), 10, "");
    if((_nuki != nullptr && _nuki->hasKeypad()) || (_nukiOpener != nullptr && _nukiOpener->hasKeypad()))
    {
        printInputField("KPINT", "Query interval keypad (seconds)", _preferences->getInt(preference_query_interval_keypad), 10, "");
    }
    printInputField("NRTRY", "Number of retries if command failed", _preferences->getInt(preference_command_nr_of_retries), 10, "");
    printInputField("TRYDLY", "Delay between retries (milliseconds)", _preferences->getInt(preference_command_retry_delay), 10, "");
    if(_preferences->getBool(preference_lock_enabled, true)) printCheckBox("REGAPP", "Lock: Nuki Bridge is running alongside Nuki Hub (needs re-pairing if changed)", _preferences->getBool(preference_register_as_app), "");
    if(_preferences->getBool(preference_opener_enabled, false)) printCheckBox("REGAPPOPN", "Opener: Nuki Bridge is running alongside Nuki Hub (needs re-pairing if changed)", _preferences->getBool(preference_register_opener_as_app), "");
    printInputField("RSBC", "Restart if bluetooth beacons not received (seconds; -1 to disable)", _preferences->getInt(preference_restart_ble_beacon_lost), 10, "");
    printInputField("TXPWR", "BLE transmit power in dB (minimum -12, maximum 9)", _preferences->getInt(preference_ble_tx_power, 9), 10, "");

    _response.concat("</table>");
    _response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    _response.concat("</form>");
    _response.concat("</body></html>");
    sendResponse(request);
}

void WebCfgServer::buildGpioConfigHtml(AsyncWebServerRequest *request)
{
    _response = "";
    buildHtmlHeader();
    _response.concat("<form method=\"post\" action=\"savegpiocfg\">");
    _response.concat("<h3>GPIO Configuration</h3>");
    _response.concat("<table>");
    std::vector<std::pair<String, String>> options;
    String gpiopreselects = "var gpio = []; ";

    const auto& availablePins = _gpio->availablePins();
    const auto& disabledPins = _gpio->getDisabledPins();

    for(const auto& pin : availablePins)
    {
        String pinStr = String(pin);
        String pinDesc = "Gpio " + pinStr;
        printDropDown(pinStr.c_str(), pinDesc.c_str(), "", options, "gpioselect");
        if(std::find(disabledPins.begin(), disabledPins.end(), pin) != disabledPins.end()) gpiopreselects.concat("gpio[" + pinStr + "] = '21';");
        else gpiopreselects.concat("gpio[" + pinStr + "] = '" + getPreselectionForGpio(pin) + "';");
    }

    _response.concat("</table>");
    _response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    _response.concat("</form>");

    options = getGpioOptions();

    _response.concat("<script type=\"text/javascript\">" + gpiopreselects + "var gpiooptions = '");

    for(const auto& option : options)
    {
        _response.concat("<option value=\"");
        _response.concat(option.first);
        _response.concat("\">");
        _response.concat(option.second);
        _response.concat("</option>");
    }

    _response.concat("'; var gpioselects = document.getElementsByClassName('gpioselect'); for (let i = 0; i < gpioselects.length; i++) { gpioselects[i].options.length = 0; gpioselects[i].innerHTML = gpiooptions; gpioselects[i].value = gpio[gpioselects[i].name]; if(gpioselects[i].value == 21) { gpioselects[i].disabled = true; } }</script>");
    _response.concat("</body></html>");
    sendResponse(request);
}

#ifndef CONFIG_IDF_TARGET_ESP32H2
void WebCfgServer::buildConfigureWifiHtml(AsyncWebServerRequest *request)
{
    _response = "";
    buildHtmlHeader();
    _response.concat("<h3>Wi-Fi</h3>");
    _response.concat("Click confirm to restart ESP into Wi-Fi configuration mode. After restart, connect to ESP access point to reconfigure Wi-Fi.<br><br>");
    buildNavigationButton("Confirm", "/wifimanager");
    _response.concat("</body></html>");
    sendResponse(request);
}
#endif

void WebCfgServer::buildInfoHtml(AsyncWebServerRequest *request)
{
     _response = "";
    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));
    buildHtmlHeader();
    _response.concat("<h3>System Information</h3><pre>");
    _response.concat("------------ NUKI HUB ------------");
    _response.concat("\nVersion: ");
    _response.concat(NUKI_HUB_VERSION);
    _response.concat("\nBuild: ");
    _response.concat(NUKI_HUB_BUILD);
    #ifndef DEBUG_NUKIHUB
    _response.concat("\nBuild type: Release");
    #else
    _response.concat("\nBuild type: Debug");
    #endif
    _response.concat("\nBuild date: ");
    _response.concat(NUKI_HUB_DATE);
    _response.concat("\nUpdater version: ");
    _response.concat(_preferences->getString(preference_updater_version, ""));
    _response.concat("\nUpdater build: ");
    _response.concat(_preferences->getString(preference_updater_build, ""));
    _response.concat("\nUpdater build date: ");
    _response.concat(_preferences->getString(preference_updater_date, ""));
    _response.concat("\nUptime (min): ");
    _response.concat(esp_timer_get_time() / 1000 / 1000 / 60);
    _response.concat("\nConfig version: ");
    _response.concat(_preferences->getInt(preference_config_version));
    _response.concat("\nLast restart reason FW: ");
    _response.concat(getRestartReason());
    _response.concat("\nLast restart reason ESP: ");
    _response.concat(getEspRestartReason());
    _response.concat("\nFree internal heap: ");
    _response.concat(ESP.getFreeHeap());
    _response.concat("\nTotal internal heap: ");
    _response.concat(ESP.getHeapSize());
    #ifdef CONFIG_SOC_SPIRAM_SUPPORTED
    if(esp_psram_get_size() > 0)
    {
        _response.concat("\nPSRAM Available: Yes");
        _response.concat("\nTotal PSRAM: ");
        _response.concat(esp_psram_get_size());
        _response.concat("\nFree PSRAM: ");
        _response.concat((esp_get_free_heap_size() - ESP.getFreeHeap()));
        _response.concat("\nTotal free heap: ");
        _response.concat(esp_get_free_heap_size());
    }
    else
    {
        _response.concat("\nPSRAM Available: No");
    }
    #else
    _response.concat("\nPSRAM Available: No");
    #endif
    _response.concat("\nNetwork task stack high watermark: ");
    _response.concat(uxTaskGetStackHighWaterMark(networkTaskHandle));
    _response.concat("\nNuki task stack high watermark: ");
    _response.concat(uxTaskGetStackHighWaterMark(nukiTaskHandle));
    _response.concat("\n\n------------ GENERAL SETTINGS ------------");
    _response.concat("\nNetwork task stack size: ");
    _response.concat(_preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE));
    _response.concat("\nNuki task stack size: ");
    _response.concat(_preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE));
    _response.concat("\nCheck for updates: ");
    _response.concat(_preferences->getBool(preference_check_updates, false) ? "Yes" : "No");
    _response.concat("\nLatest version: ");
    _response.concat(_preferences->getString(preference_latest_version, ""));
    _response.concat("\nAllow update from MQTT: ");
    _response.concat(_preferences->getBool(preference_update_from_mqtt, false) ? "Yes" : "No");
    _response.concat("\nWeb configurator username: ");
    _response.concat(_preferences->getString(preference_cred_user, "").length() > 0 ? "***" : "Not set");
    _response.concat("\nWeb configurator password: ");
    _response.concat(_preferences->getString(preference_cred_password, "").length() > 0 ? "***" : "Not set");
    _response.concat("\nWeb configurator enabled: ");
    _response.concat(_preferences->getBool(preference_webserver_enabled, true) ? "Yes" : "No");
    _response.concat("\nPublish debug information enabled: ");
    _response.concat(_preferences->getBool(preference_publish_debug_info, false) ? "Yes" : "No");
    _response.concat("\nMQTT log enabled: ");
    _response.concat(_preferences->getBool(preference_mqtt_log_enabled, false) ? "Yes" : "No");
    _response.concat("\nWebserial enabled: ");
    _response.concat(_preferences->getBool(preference_webserial_enabled, false) ? "Yes" : "No");
    _response.concat("\nBootloop protection enabled: ");
    _response.concat(_preferences->getBool(preference_enable_bootloop_reset, false) ? "Yes" : "No");
    _response.concat("\n\n------------ NETWORK ------------");
    _response.concat("\nNetwork device: ");
    _response.concat(_network->networkDeviceName());
    _response.concat("\nNetwork connected: ");
    _response.concat(_network->isConnected() ? "Yes" : "No");
    if(_network->isConnected())
    {
        _response.concat("\nIP Address: ");
        _response.concat(_network->localIP());

        if(_network->networkDeviceName() == "Built-in Wi-Fi")
        {
            #ifndef CONFIG_IDF_TARGET_ESP32H2
            _response.concat("\nSSID: ");
            _response.concat(WiFi.SSID());
            _response.concat("\nBSSID of AP: ");
            _response.concat(_network->networkBSSID());
            _response.concat("\nESP32 MAC address: ");
            _response.concat(WiFi.macAddress());
            #endif
        }
        else
        {
            //Ethernet info
        }
    }
    _response.concat("\n\n------------ NETWORK SETTINGS ------------");
    _response.concat("\nNuki Hub hostname: ");
    _response.concat(_preferences->getString(preference_hostname, ""));
    if(_preferences->getBool(preference_ip_dhcp_enabled, true)) _response.concat("\nDHCP enabled: Yes");
    else
    {
        _response.concat("\nDHCP enabled: No");
        _response.concat("\nStatic IP address: ");
        _response.concat(_preferences->getString(preference_ip_address, ""));
        _response.concat("\nStatic IP subnet: ");
        _response.concat(_preferences->getString(preference_ip_subnet, ""));
        _response.concat("\nStatic IP gateway: ");
        _response.concat(_preferences->getString(preference_ip_gateway, ""));
        _response.concat("\nStatic IP DNS server: ");
        _response.concat(_preferences->getString(preference_ip_dns_server, ""));
    }

    #ifndef CONFIG_IDF_TARGET_ESP32H2
    _response.concat("\nFallback to Wi-Fi / Wi-Fi config portal disabled: ");
    _response.concat(_preferences->getBool(preference_network_wifi_fallback_disabled, false) ? "Yes" : "No");
    if(_network->networkDeviceName() == "Built-in Wi-Fi")
    {
        _response.concat("\nConnect to AP with the best signal enabled: ");
        _response.concat(_preferences->getBool(preference_find_best_rssi, false) ? "Yes" : "No");
        _response.concat("\nRSSI Publish interval (s): ");

        if(_preferences->getInt(preference_rssi_publish_interval, 60) < 0) _response.concat("Disabled");
        else _response.concat(_preferences->getInt(preference_rssi_publish_interval, 60));
    }
    #endif
    _response.concat("\nRestart ESP32 on network disconnect enabled: ");
    _response.concat(_preferences->getBool(preference_restart_on_disconnect, false) ? "Yes" : "No");
    _response.concat("\nReconnect network on MQTT connection failure enabled: ");
    _response.concat(_preferences->getBool(preference_recon_netw_on_mqtt_discon, false) ? "Yes" : "No");
    _response.concat("\nMQTT Timeout until restart (s): ");
    if(_preferences->getInt(preference_network_timeout, 60) < 0) _response.concat("Disabled");
    else _response.concat(_preferences->getInt(preference_network_timeout, 60));
    _response.concat("\n\n------------ MQTT ------------");
    _response.concat("\nMQTT connected: ");
    _response.concat(_network->mqttConnectionState() > 0 ? "Yes" : "No");
    _response.concat("\nMQTT broker address: ");
    _response.concat(_preferences->getString(preference_mqtt_broker, ""));
    _response.concat("\nMQTT broker port: ");
    _response.concat(_preferences->getInt(preference_mqtt_broker_port, 1883));
    _response.concat("\nMQTT username: ");
    _response.concat(_preferences->getString(preference_mqtt_user, "").length() > 0 ? "***" : "Not set");
    _response.concat("\nMQTT password: ");
    _response.concat(_preferences->getString(preference_mqtt_password, "").length() > 0 ? "***" : "Not set");
    if(_preferences->getBool(preference_lock_enabled, true))
    {
        _response.concat("\nMQTT lock base topic: ");
        _response.concat(_preferences->getString(preference_mqtt_lock_path, ""));
    }
    if(_preferences->getBool(preference_opener_enabled, false))
    {
        _response.concat("\nMQTT opener base topic: ");
        _response.concat(_preferences->getString(preference_mqtt_lock_path, ""));
    }
    _response.concat("\nMQTT SSL CA: ");
    _response.concat(_preferences->getString(preference_mqtt_ca, "").length() > 0 ? "***" : "Not set");
    _response.concat("\nMQTT SSL CRT: ");
    _response.concat(_preferences->getString(preference_mqtt_crt, "").length() > 0 ? "***" : "Not set");
    _response.concat("\nMQTT SSL Key: ");
    _response.concat(_preferences->getString(preference_mqtt_key, "").length() > 0 ? "***" : "Not set");
    _response.concat("\n\n------------ BLUETOOTH ------------");
    _response.concat("\nBluetooth TX power (dB): ");
    _response.concat(_preferences->getInt(preference_ble_tx_power, 9));
    _response.concat("\nBluetooth command nr of retries: ");
    _response.concat(_preferences->getInt(preference_command_nr_of_retries, 3));
    _response.concat("\nBluetooth command retry delay (ms): ");
    _response.concat(_preferences->getInt(preference_command_retry_delay, 100));
    _response.concat("\nSeconds until reboot when no BLE beacons recieved: ");
    _response.concat(_preferences->getInt(preference_restart_ble_beacon_lost, 60));
    _response.concat("\n\n------------ QUERY / PUBLISH SETTINGS ------------");
    _response.concat("\nLock/Opener state query interval (s): ");
    _response.concat(_preferences->getInt(preference_query_interval_lockstate, 1800));
    _response.concat("\nPublish Nuki device authorization log: ");
    _response.concat(_preferences->getBool(preference_publish_authdata, false) ? "Yes" : "No");
    _response.concat("\nMax authorization log entries to retrieve: ");
    _response.concat(_preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG));
    _response.concat("\nBattery state query interval (s): ");
    _response.concat(_preferences->getInt(preference_query_interval_battery, 1800));
    _response.concat("\nMost non-JSON MQTT topics disabled: ");
    _response.concat(_preferences->getBool(preference_disable_non_json, false) ? "Yes" : "No");
    _response.concat("\nPublish Nuki device config: ");
    _response.concat(_preferences->getBool(preference_conf_info_enabled, false) ? "Yes" : "No");
    _response.concat("\nConfig query interval (s): ");
    _response.concat(_preferences->getInt(preference_query_interval_configuration, 3600));
    _response.concat("\nPublish Keypad info: ");
    _response.concat(_preferences->getBool(preference_keypad_info_enabled, false) ? "Yes" : "No");
    _response.concat("\nKeypad query interval (s): ");
    _response.concat(_preferences->getInt(preference_query_interval_keypad, 1800));
    _response.concat("\nEnable Keypad control: ");
    _response.concat(_preferences->getBool(preference_keypad_control_enabled, false) ? "Yes" : "No");
    _response.concat("\nPublish Keypad topic per entry: ");
    _response.concat(_preferences->getBool(preference_keypad_topic_per_entry, false) ? "Yes" : "No");
    _response.concat("\nPublish Keypad codes: ");
    _response.concat(_preferences->getBool(preference_keypad_publish_code, false) ? "Yes" : "No");
    _response.concat("\nMax keypad entries to retrieve: ");
    _response.concat(_preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD));
    _response.concat("\nPublish timecontrol info: ");
    _response.concat(_preferences->getBool(preference_timecontrol_info_enabled, false) ? "Yes" : "No");
    _response.concat("\nKeypad query interval (s): ");
    _response.concat(_preferences->getInt(preference_query_interval_keypad, 1800));
    _response.concat("\nEnable timecontrol control: ");
    _response.concat(_preferences->getBool(preference_timecontrol_control_enabled, false) ? "Yes" : "No");
    _response.concat("\nPublish timecontrol topic per entry: ");
    _response.concat(_preferences->getBool(preference_timecontrol_topic_per_entry, false) ? "Yes" : "No");
    _response.concat("\nMax timecontrol entries to retrieve: ");
    _response.concat(_preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL));
    _response.concat("\n\n------------ HOME ASSISTANT ------------");
    _response.concat("\nHome Assistant auto discovery enabled: ");
    if(_preferences->getString(preference_mqtt_hass_discovery, "").length() > 0)
    {
        _response.concat("Yes");
        _response.concat("\nHome Assistant auto discovery topic: ");
        _response.concat(_preferences->getString(preference_mqtt_hass_discovery, "") + "/");
        _response.concat("\nNuki Hub configuration URL for HA: ");
        _response.concat(_preferences->getString(preference_mqtt_hass_cu_url, "").length() > 0 ? _preferences->getString(preference_mqtt_hass_cu_url, "") : "http://" + _network->localIP());
    }
    else _response.concat("No");
    _response.concat("\n\n------------ NUKI LOCK ------------");
    if(_nuki == nullptr || !_preferences->getBool(preference_lock_enabled, true)) _response.concat("\nLock enabled: No");
    else
    {
        _response.concat("\nLock enabled: Yes");
        _response.concat("\nPaired: ");
        _response.concat(_nuki->isPaired() ? "Yes" : "No");
        _response.concat("\nNuki Hub device ID: ");
        _response.concat(_preferences->getUInt(preference_device_id_lock, 0));
        _response.concat("\nNuki device ID: ");
        _response.concat(_preferences->getUInt(preference_nuki_id_lock, 0) > 0 ? "***" : "Not set");
        _response.concat("\nFirmware version: ");
        _response.concat(_nuki->firmwareVersion().c_str());
        _response.concat("\nHardware version: ");
        _response.concat(_nuki->hardwareVersion().c_str());
        _response.concat("\nValid PIN set: ");
        _response.concat(_nuki->isPaired() ? _nuki->isPinValid() ? "Yes" : "No" : "-");
        _response.concat("\nHas door sensor: ");
        _response.concat(_nuki->hasDoorSensor() ? "Yes" : "No");
        _response.concat("\nHas keypad: ");
        _response.concat(_nuki->hasKeypad() ? "Yes" : "No");
        if(_nuki->hasKeypad())
        {
            _response.concat("\nKeypad highest entries count: ");
            _response.concat(_preferences->getInt(preference_lock_max_keypad_code_count, 0));
        }
        _response.concat("\nTimecontrol highest entries count: ");
        _response.concat(_preferences->getInt(preference_lock_max_timecontrol_entry_count, 0));
        _response.concat("\nRegister as: ");
        _response.concat(_preferences->getBool(preference_register_as_app, false) ? "App" : "Bridge");
        _response.concat("\n\n------------ HYBRID MODE ------------");
        if(!_preferences->getBool(preference_official_hybrid_enabled, false)) _response.concat("\nHybrid mode enabled: No");
        else
        {
            _response.concat("\nHybrid mode enabled: Yes");
            _response.concat("\nHybrid mode connected: ");
            _response.concat(_nuki->offConnected() ? "Yes": "No");
            _response.concat("\nSending actions through official MQTT enabled: ");
            _response.concat(_preferences->getBool(preference_official_hybrid_actions, false) ? "Yes" : "No");
            /* NOT IMPLEMENTED (YET?)
            if(_preferences->getBool(preference_official_hybrid_actions, false))
            {
                _response.concat("\nRetry actions through BLE enabled: ");
                _response.concat(_preferences->getBool(preference_official_hybrid_retry, false) ? "Yes" : "No");
            }
            */
            _response.concat("\nTime between status updates when official MQTT is offline (s): ");
            _response.concat(_preferences->getInt(preference_query_interval_hybrid_lockstate, 600));
        }
        uint32_t basicLockConfigAclPrefs[16];
        _preferences->getBytes(preference_conf_lock_basic_acl, &basicLockConfigAclPrefs, sizeof(basicLockConfigAclPrefs));
        uint32_t advancedLockConfigAclPrefs[22];
        _preferences->getBytes(preference_conf_lock_advanced_acl, &advancedLockConfigAclPrefs, sizeof(advancedLockConfigAclPrefs));
        _response.concat("\n\n------------ NUKI LOCK ACL ------------");
        _response.concat("\nLock: ");
        _response.concat((int)aclPrefs[0] ? "Allowed" : "Disallowed");
        _response.concat("\nUnlock: ");
        _response.concat((int)aclPrefs[1] ? "Allowed" : "Disallowed");
        _response.concat("\nUnlatch: ");
        _response.concat((int)aclPrefs[2] ? "Allowed" : "Disallowed");
        _response.concat("\nLock N Go: ");
        _response.concat((int)aclPrefs[3] ? "Allowed" : "Disallowed");
        _response.concat("\nLock N Go Unlatch: ");
        _response.concat((int)aclPrefs[4] ? "Allowed" : "Disallowed");
        _response.concat("\nFull Lock: ");
        _response.concat((int)aclPrefs[5] ? "Allowed" : "Disallowed");
        _response.concat("\nFob Action 1: ");
        _response.concat((int)aclPrefs[6] ? "Allowed" : "Disallowed");
        _response.concat("\nFob Action 2: ");
        _response.concat((int)aclPrefs[7] ? "Allowed" : "Disallowed");
        _response.concat("\nFob Action 3: ");
        _response.concat((int)aclPrefs[8] ? "Allowed" : "Disallowed");
        _response.concat("\n\n------------ NUKI LOCK CONFIG ACL ------------");
        _response.concat("\nName: ");
        _response.concat((int)basicLockConfigAclPrefs[0] ? "Allowed" : "Disallowed");
        _response.concat("\nLatitude: ");
        _response.concat((int)basicLockConfigAclPrefs[1] ? "Allowed" : "Disallowed");
        _response.concat("\nLongitude: ");
        _response.concat((int)basicLockConfigAclPrefs[2] ? "Allowed" : "Disallowed");
        _response.concat("\nAuto Unlatch: ");
        _response.concat((int)basicLockConfigAclPrefs[3] ? "Allowed" : "Disallowed");
        _response.concat("\nPairing enabled: ");
        _response.concat((int)basicLockConfigAclPrefs[4] ? "Allowed" : "Disallowed");
        _response.concat("\nButton enabled: ");
        _response.concat((int)basicLockConfigAclPrefs[5] ? "Allowed" : "Disallowed");
        _response.concat("\nLED flash enabled: ");
        _response.concat((int)basicLockConfigAclPrefs[6] ? "Allowed" : "Disallowed");
        _response.concat("\nLED brightness: ");
        _response.concat((int)basicLockConfigAclPrefs[7] ? "Allowed" : "Disallowed");
        _response.concat("\nTimezone offset: ");
        _response.concat((int)basicLockConfigAclPrefs[8] ? "Allowed" : "Disallowed");
        _response.concat("\nDST mode: ");
        _response.concat((int)basicLockConfigAclPrefs[9] ? "Allowed" : "Disallowed");
        _response.concat("\nFob Action 1: ");
        _response.concat((int)basicLockConfigAclPrefs[10] ? "Allowed" : "Disallowed");
        _response.concat("\nFob Action 2: ");
        _response.concat((int)basicLockConfigAclPrefs[11] ? "Allowed" : "Disallowed");
        _response.concat("\nFob Action 3: ");
        _response.concat((int)basicLockConfigAclPrefs[12] ? "Allowed" : "Disallowed");
        _response.concat("\nSingle Lock: ");
        _response.concat((int)basicLockConfigAclPrefs[13] ? "Allowed" : "Disallowed");
        _response.concat("\nAdvertising Mode: ");
        _response.concat((int)basicLockConfigAclPrefs[14] ? "Allowed" : "Disallowed");
        _response.concat("\nTimezone ID: ");
        _response.concat((int)basicLockConfigAclPrefs[15] ? "Allowed" : "Disallowed");
        _response.concat("\nUnlocked Position Offset Degrees: ");
        _response.concat((int)advancedLockConfigAclPrefs[0] ? "Allowed" : "Disallowed");
        _response.concat("\nLocked Position Offset Degrees: ");
        _response.concat((int)advancedLockConfigAclPrefs[1] ? "Allowed" : "Disallowed");
        _response.concat("\nSingle Locked Position Offset Degrees: ");
        _response.concat((int)advancedLockConfigAclPrefs[2] ? "Allowed" : "Disallowed");
        _response.concat("\nUnlocked To Locked Transition Offset Degrees: ");
        _response.concat((int)advancedLockConfigAclPrefs[3] ? "Allowed" : "Disallowed");
        _response.concat("\nLock n Go timeout: ");
        _response.concat((int)advancedLockConfigAclPrefs[4] ? "Allowed" : "Disallowed");
        _response.concat("\nSingle button press action: ");
        _response.concat((int)advancedLockConfigAclPrefs[5] ? "Allowed" : "Disallowed");
        _response.concat("\nDouble button press action: ");
        _response.concat((int)advancedLockConfigAclPrefs[6] ? "Allowed" : "Disallowed");
        _response.concat("\nDetached cylinder: ");
        _response.concat((int)advancedLockConfigAclPrefs[7] ? "Allowed" : "Disallowed");
        _response.concat("\nBattery type: ");
        _response.concat((int)advancedLockConfigAclPrefs[8] ? "Allowed" : "Disallowed");
        _response.concat("\nAutomatic battery type detection: ");
        _response.concat((int)advancedLockConfigAclPrefs[9] ? "Allowed" : "Disallowed");
        _response.concat("\nUnlatch duration: ");
        _response.concat((int)advancedLockConfigAclPrefs[10] ? "Allowed" : "Disallowed");
        _response.concat("\nAuto lock timeout: ");
        _response.concat((int)advancedLockConfigAclPrefs[11] ? "Allowed" : "Disallowed");
        _response.concat("\nAuto unlock disabled: ");
        _response.concat((int)advancedLockConfigAclPrefs[12] ? "Allowed" : "Disallowed");
        _response.concat("\nNightmode enabled: ");
        _response.concat((int)advancedLockConfigAclPrefs[13] ? "Allowed" : "Disallowed");
        _response.concat("\nNightmode start time: ");
        _response.concat((int)advancedLockConfigAclPrefs[14] ? "Allowed" : "Disallowed");
        _response.concat("\nNightmode end time: ");
        _response.concat((int)advancedLockConfigAclPrefs[15] ? "Allowed" : "Disallowed");
        _response.concat("\nNightmode auto lock enabled: ");
        _response.concat((int)advancedLockConfigAclPrefs[16] ? "Allowed" : "Disallowed");
        _response.concat("\nNightmode auto unlock disabled: ");
        _response.concat((int)advancedLockConfigAclPrefs[17] ? "Allowed" : "Disallowed");
        _response.concat("\nNightmode immediate lock on start: ");
        _response.concat((int)advancedLockConfigAclPrefs[18] ? "Allowed" : "Disallowed");
        _response.concat("\nAuto lock enabled: ");
        _response.concat((int)advancedLockConfigAclPrefs[19] ? "Allowed" : "Disallowed");
        _response.concat("\nImmediate auto lock enabled: ");
        _response.concat((int)advancedLockConfigAclPrefs[20] ? "Allowed" : "Disallowed");
        _response.concat("\nAuto update enabled: ");
        _response.concat((int)advancedLockConfigAclPrefs[21] ? "Allowed" : "Disallowed");

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
            _response.concat("\n\n------------ NUKI LOCK PAIRING ------------");
            _response.concat("\nBLE Address: ");
            for (int i = 0; i < 6; i++)
            {
                sprintf(tmp, "%02x", currentBleAddress[i]);
                _response.concat(tmp);
            }
            _response.concat("\nSecretKeyK: ");
            for (int i = 0; i < 32; i++)
            {
                sprintf(tmp, "%02x", secretKeyK[i]);
                _response.concat(tmp);
            }
            _response.concat("\nAuthorizationId: ");
            for (int i = 0; i < 4; i++)
            {
                sprintf(tmp, "%02x", authorizationId[i]);
                _response.concat(tmp);
            }
            uint32_t authorizationIdInt = authorizationId[0] + 256U*authorizationId[1] + 65536U*authorizationId[2] + 16777216U*authorizationId[3];
            _response.concat("\nAuthorizationId (UINT32_T): ");
            _response.concat(authorizationIdInt);
        }
    }

    _response.concat("\n\n------------ NUKI OPENER ------------");
    if(_nukiOpener == nullptr || !_preferences->getBool(preference_opener_enabled, false)) _response.concat("\nOpener enabled: No");
    else
    {
        _response.concat("\nOpener enabled: Yes");
        _response.concat("\nPaired: ");
        _response.concat(_nukiOpener->isPaired() ? "Yes" : "No");
        _response.concat("\nNuki Hub device ID: ");
        _response.concat(_preferences->getUInt(preference_device_id_opener, 0));
        _response.concat("\nNuki device ID: ");
        _response.concat(_preferences->getUInt(preference_nuki_id_opener, 0) > 0 ? "***" : "Not set");
        _response.concat("\nFirmware version: ");
        _response.concat(_nukiOpener->firmwareVersion().c_str());
        _response.concat("\nHardware version: ");
        _response.concat(_nukiOpener->hardwareVersion().c_str());
        _response.concat("\nOpener valid PIN set: ");
        _response.concat(_nukiOpener->isPaired() ? _nukiOpener->isPinValid() ? "Yes" : "No" : "-");
        _response.concat("\nOpener has keypad: ");
        _response.concat(_nukiOpener->hasKeypad() ? "Yes" : "No");
        if(_nuki->hasKeypad())
        {
            _response.concat("\nKeypad highest entries count: ");
            _response.concat(_preferences->getInt(preference_opener_max_keypad_code_count, 0));
        }
        _response.concat("\nTimecontrol highest entries count: ");
        _response.concat(_preferences->getInt(preference_opener_max_timecontrol_entry_count, 0));
        _response.concat("\nRegister as: ");
        _response.concat(_preferences->getBool(preference_register_opener_as_app, false) ? "App" : "Bridge");
        _response.concat("\nNuki Opener Lock/Unlock action set to Continuous mode in Home Assistant: ");
        _response.concat(_preferences->getBool(preference_opener_continuous_mode, false) ? "Yes" : "No");
        uint32_t basicOpenerConfigAclPrefs[14];
        _preferences->getBytes(preference_conf_opener_basic_acl, &basicOpenerConfigAclPrefs, sizeof(basicOpenerConfigAclPrefs));
        uint32_t advancedOpenerConfigAclPrefs[20];
        _preferences->getBytes(preference_conf_opener_advanced_acl, &advancedOpenerConfigAclPrefs, sizeof(advancedOpenerConfigAclPrefs));
        _response.concat("\n\n------------ NUKI OPENER ACL ------------");
        _response.concat("\nActivate Ring-to-Open: ");
        _response.concat((int)aclPrefs[9] ? "Allowed" : "Disallowed");
        _response.concat("\nDeactivate Ring-to-Open: ");
        _response.concat((int)aclPrefs[10] ? "Allowed" : "Disallowed");
        _response.concat("\nElectric Strike Actuation: ");
        _response.concat((int)aclPrefs[11] ? "Allowed" : "Disallowed");
        _response.concat("\nActivate Continuous Mode: ");
        _response.concat((int)aclPrefs[12] ? "Allowed" : "Disallowed");
        _response.concat("\nDeactivate Continuous Mode: ");
        _response.concat((int)aclPrefs[13] ? "Allowed" : "Disallowed");
        _response.concat("\nFob Action 1: ");
        _response.concat((int)aclPrefs[14] ? "Allowed" : "Disallowed");
        _response.concat("\nFob Action 2: ");
        _response.concat((int)aclPrefs[15] ? "Allowed" : "Disallowed");
        _response.concat("\nFob Action 3: ");
        _response.concat((int)aclPrefs[16] ? "Allowed" : "Disallowed");
        _response.concat("\n\n------------ NUKI OPENER CONFIG ACL ------------");
        _response.concat("\nName: ");
        _response.concat((int)basicOpenerConfigAclPrefs[0] ? "Allowed" : "Disallowed");
        _response.concat("\nLatitude: ");
        _response.concat((int)basicOpenerConfigAclPrefs[1] ? "Allowed" : "Disallowed");
        _response.concat("\nLongitude: ");
        _response.concat((int)basicOpenerConfigAclPrefs[2] ? "Allowed" : "Disallowed");
        _response.concat("\nPairing enabled: ");
        _response.concat((int)basicOpenerConfigAclPrefs[3] ? "Allowed" : "Disallowed");
        _response.concat("\nButton enabled: ");
        _response.concat((int)basicOpenerConfigAclPrefs[4] ? "Allowed" : "Disallowed");
        _response.concat("\nLED flash enabled: ");
        _response.concat((int)basicOpenerConfigAclPrefs[5] ? "Allowed" : "Disallowed");
        _response.concat("\nTimezone offset: ");
        _response.concat((int)basicOpenerConfigAclPrefs[6] ? "Allowed" : "Disallowed");
        _response.concat("\nDST mode: ");
        _response.concat((int)basicOpenerConfigAclPrefs[7] ? "Allowed" : "Disallowed");
        _response.concat("\nFob Action 1: ");
        _response.concat((int)basicOpenerConfigAclPrefs[8] ? "Allowed" : "Disallowed");
        _response.concat("\nFob Action 2: ");
        _response.concat((int)basicOpenerConfigAclPrefs[9] ? "Allowed" : "Disallowed");
        _response.concat("\nFob Action 3: ");
        _response.concat((int)basicOpenerConfigAclPrefs[10] ? "Allowed" : "Disallowed");
        _response.concat("\nOperating Mode: ");
        _response.concat((int)basicOpenerConfigAclPrefs[11] ? "Allowed" : "Disallowed");
        _response.concat("\nAdvertising Mode: ");
        _response.concat((int)basicOpenerConfigAclPrefs[12] ? "Allowed" : "Disallowed");
        _response.concat("\nTimezone ID: ");
        _response.concat((int)basicOpenerConfigAclPrefs[13] ? "Allowed" : "Disallowed");
        _response.concat("\nIntercom ID: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[0] ? "Allowed" : "Disallowed");
        _response.concat("\nBUS mode Switch: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[1] ? "Allowed" : "Disallowed");
        _response.concat("\nShort Circuit Duration: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[2] ? "Allowed" : "Disallowed");
        _response.concat("\nEletric Strike Delay: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[3] ? "Allowed" : "Disallowed");
        _response.concat("\nRandom Electric Strike Delay: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[4] ? "Allowed" : "Disallowed");
        _response.concat("\nElectric Strike Duration: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[5] ? "Allowed" : "Disallowed");
        _response.concat("\nDisable RTO after ring: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[6] ? "Allowed" : "Disallowed");
        _response.concat("\nRTO timeout: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[7] ? "Allowed" : "Disallowed");
        _response.concat("\nDoorbell suppression: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[8] ? "Allowed" : "Disallowed");
        _response.concat("\nDoorbell suppression duration: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[9] ? "Allowed" : "Disallowed");
        _response.concat("\nSound Ring: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[10] ? "Allowed" : "Disallowed");
        _response.concat("\nSound Open: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[11] ? "Allowed" : "Disallowed");
        _response.concat("\nSound RTO: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[12] ? "Allowed" : "Disallowed");
        _response.concat("\nSound CM: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[13] ? "Allowed" : "Disallowed");
        _response.concat("\nSound confirmation: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[14] ? "Allowed" : "Disallowed");
        _response.concat("\nSound level: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[15] ? "Allowed" : "Disallowed");
        _response.concat("\nSingle button press action: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[16] ? "Allowed" : "Disallowed");
        _response.concat("\nDouble button press action: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[17] ? "Allowed" : "Disallowed");
        _response.concat("\nBattery type: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[18] ? "Allowed" : "Disallowed");
        _response.concat("\nAutomatic battery type detection: ");
        _response.concat((int)advancedOpenerConfigAclPrefs[19] ? "Allowed" : "Disallowed");
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
            _response.concat("\n\n------------ NUKI OPENER PAIRING ------------");
            _response.concat("\nBLE Address: ");
            for (int i = 0; i < 6; i++)
            {
                sprintf(tmp, "%02x", currentBleAddressOpn[i]);
                _response.concat(tmp);
            }
            _response.concat("\nSecretKeyK: ");
            for (int i = 0; i < 32; i++)
            {
                sprintf(tmp, "%02x", secretKeyKOpn[i]);
                _response.concat(tmp);
            }
            _response.concat("\nAuthorizationId: ");
            for (int i = 0; i < 4; i++)
            {
                sprintf(tmp, "%02x", authorizationIdOpn[i]);
                _response.concat(tmp);
            }
        }
    }

    _response.concat("\n\n------------ GPIO ------------\n");
    String gpioStr = "";
    _gpio->getConfigurationText(gpioStr, _gpio->pinConfiguration());
    _response.concat(gpioStr);
    _response.concat("</pre></body></html>");
    sendResponse(request);
}

void WebCfgServer::processUnpair(AsyncWebServerRequest *request, bool opener)
{
    String value = "";
    if(request->hasParam("CONFIRMTOKEN", true))
    {
        const AsyncWebParameter* p = request->getParam("CONFIRMTOKEN", true);
        if(p->value() != "") value = p->value();
    }

    if(value != _confirmCode)
    {
        buildConfirmHtml(request, "Confirm code is invalid.", 3, true);
        return;
    }

    buildConfirmHtml(request, opener ? "Unpairing Nuki Opener and restarting." : "Unpairing Nuki Lock and restarting.", 3, true);

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
    if(request->hasParam("CONFIRMTOKEN", true))
    {
        const AsyncWebParameter* p = request->getParam("CONFIRMTOKEN", true);
        if(p->value() != "") value = p->value();
    }

    bool resetWifi = false;
    if(value.length() == 0 || value != _confirmCode)
    {
        buildConfirmHtml(request, "Confirm code is invalid.", 3, true);
        return;
    }
    else
    {
        String value2 = "";
        if(request->hasParam("WIFI", true))
        {
            const AsyncWebParameter* p = request->getParam("WIFI", true);
            if(p->value() != "") value = p->value();
        }

        if(value2 == "1")
        {
            resetWifi = true;
            buildConfirmHtml(request, "Factory resetting Nuki Hub, unpairing Nuki Lock and Nuki Opener and resetting WiFi.", 3, true);
        }
        else
        {
            buildConfirmHtml(request, "Factory resetting Nuki Hub, unpairing Nuki Lock and Nuki Opener.", 3, true);
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
        wifi_config_t current_conf;
        esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &current_conf);
        memset(current_conf.sta.ssid, 0, sizeof(current_conf.sta.ssid));
        memset(current_conf.sta.password, 0, sizeof(current_conf.sta.password));
        esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &current_conf);
        _network->reconfigureDevice();
    }
    #endif

    waitAndProcess(false, 3000);
    restartEsp(RestartReason::NukiHubReset);
}

void WebCfgServer::printInputField(const char *token,
                                   const char *description,
                                   const char *value,
                                   const size_t& maxLength,
                                   const char *args,
                                   const bool& isPassword,
                                   const bool& showLengthRestriction)
{
    char maxLengthStr[20];

    itoa(maxLength, maxLengthStr, 10);

    _response.concat("<tr><td>");
    _response.concat(description);

    if(showLengthRestriction)
    {
        _response.concat(" (Max. ");
        _response.concat(maxLength);
        _response.concat(" characters)");
    }

    _response.concat("</td><td>");
    _response.concat("<input type=");
    _response.concat(isPassword ? "\"password\"" : "\"text\"");
    if(strcmp(args, "") != 0)
    {
        _response.concat(" ");
        _response.concat(args);
    }
    if(strcmp(value, "") != 0)
    {
    _response.concat(" value=\"");
    _response.concat(value);
    }
    _response.concat("\" name=\"");
    _response.concat(token);
    _response.concat("\" size=\"25\" maxlength=\"");
    _response.concat(maxLengthStr);
    _response.concat("\"/>");
    _response.concat("</td></tr>");
}

void WebCfgServer::printInputField(const char *token,
                                   const char *description,
                                   const int value,
                                   size_t maxLength,
                                   const char *args)
{
    char valueStr[20];
    itoa(value, valueStr, 10);
    printInputField(token, description, valueStr, maxLength, args);
}

void WebCfgServer::printCheckBox(const char *token, const char *description, const bool value, const char *htmlClass)
{
    _response.concat("<tr><td>");
    _response.concat(description);
    _response.concat("</td><td>");

    _response.concat("<input type=hidden name=\"");
    _response.concat(token);
    _response.concat("\" value=\"0\"");
    _response.concat("/>");

    _response.concat("<input type=checkbox name=\"");
    _response.concat(token);

    _response.concat("\" class=\"");
    _response.concat(htmlClass);

    _response.concat("\" value=\"1\"");
    _response.concat(value ? " checked=\"checked\"" : "");
    _response.concat("/></td></tr>");
}

void WebCfgServer::printTextarea(const char *token,
                                 const char *description,
                                 const char *value,
                                 const size_t& maxLength,
                                 const bool& enabled,
                                 const bool& showLengthRestriction)
{
    char maxLengthStr[20];

    itoa(maxLength, maxLengthStr, 10);

    _response.concat("<tr><td>");
    _response.concat(description);
    if(showLengthRestriction)
    {
        _response.concat(" (Max. ");
        _response.concat(maxLength);
        _response.concat(" characters)");
    }
    _response.concat("</td><td>");
    _response.concat(" <textarea ");
    if(!enabled)
    {
        _response.concat("disabled");
    }
    _response.concat(" name=\"");
    _response.concat(token);
    _response.concat("\" maxlength=\"");
    _response.concat(maxLengthStr);
    _response.concat("\">");
    _response.concat(value);
    _response.concat("</textarea>");
    _response.concat("</td></tr>");
}

void WebCfgServer::printDropDown(const char *token, const char *description, const String preselectedValue, const std::vector<std::pair<String, String>> options, const String className)
{
    _response.concat("<tr><td>");
    _response.concat(description);
    _response.concat("</td><td>");

    if(className.length() > 0) _response.concat("<select class=\"" + className + "\" name=\"");
    else _response.concat("<select name=\"");
    _response.concat(token);
    _response.concat("\">");

    for(const auto& option : options)
    {
        if(option.first == preselectedValue) _response.concat("<option selected=\"selected\" value=\"");
        else _response.concat("<option value=\"");
        _response.concat(option.first);
        _response.concat("\">");
        _response.concat(option.second);
        _response.concat("</option>");
    }

    _response.concat("</select>");
    _response.concat("</td></tr>");
}

void WebCfgServer::buildNavigationButton(const char *caption, const char *targetPath, const char* labelText)
{
    _response.concat("<form method=\"get\" action=\"");
    _response.concat(targetPath);
    _response.concat("\">");
    _response.concat("<button type=\"submit\">");
    _response.concat(caption);
    _response.concat("</button> ");
    _response.concat(labelText);
    _response.concat("</form>");
}

void WebCfgServer::buildNavigationMenuEntry(const char *title, const char *targetPath, const char* warningMessage)
{
    _response.concat("<a href=\"");
    _response.concat(targetPath);
    _response.concat("\">");
    _response.concat("<li>");
    _response.concat(title);
    if(strcmp(warningMessage, "") != 0){
        _response.concat("<span>");
        _response.concat(warningMessage);
        _response.concat("</span>");
    }
    _response.concat("</li></a>");
}

void WebCfgServer::printParameter(const char *description, const char *value, const char *link, const char *id)
{
    _response.concat("<tr>");
    _response.concat("<td>");
    _response.concat(description);
    _response.concat("</td>");
    if(strcmp(id, "") == 0) _response.concat("<td>");
    else
    {
        _response.concat("<td id=\"");
        _response.concat(id);
        _response.concat("\">");
    }
    if(strcmp(link, "") == 0) _response.concat(value);
    else
    {
        _response.concat("<a href=\"");
        _response.concat(link);
        _response.concat("\"> ");
        _response.concat(value);
        _response.concat("</a>");
    }
    _response.concat("</td>");
    _response.concat("</tr>");

}

const std::vector<std::pair<String, String>> WebCfgServer::getNetworkDetectionOptions() const
{
    std::vector<std::pair<String, String>> options;

    options.push_back(std::make_pair("1", "Wi-Fi only"));
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
