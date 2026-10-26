#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include <esp_task_wdt.h>
#include <Update.h>
#include "FS.h"
#include "SPIFFS.h"
#ifndef NUKI_HUB_UPDATER
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#endif

extern const uint8_t x509_crt_imported_bundle_bin_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_imported_bundle_bin_end[]   asm("_binary_x509_crt_bundle_end");

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

#ifndef NUKI_HUB_UPDATER
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

#endif
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

