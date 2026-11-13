#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include <esp_task_wdt.h>
#include <Update.h>
#include "FS.h"
#include "SPIFFS.h"
// The updater binary is always below this size, the Nuki Hub binary above it.
static constexpr size_t otaBinarySizeLimit = 1600000;

#ifndef NUKI_HUB_UPDATER
void WebCfgServer::printOtaForm(PsychicStreamResponse* response, const char* channel, const char* compareTo, const char* sameBuild, const char* confirmMessage, const char* buttonClass, const char* label, const char* channelValue)
{
    response->print("<form action=\"/get\" method=\"get\" data-confirm=\"");
    response->print(confirmMessage);
    response->print("\"");
    if(strcmp(compareTo, "") != 0)
    {
        response->print(" data-same-as=\"");
        response->print(compareTo);
        response->print("\" data-same-build=\"");
        response->print(sameBuild);
        response->print("\"");
    }
    response->print("><input type=\"hidden\" name=\"page\" value=\"autoupdate\"><input type=\"hidden\" name=\"");
    response->print(channel);
    response->print("\" value=\"");
    response->print(strcmp(channel, "other") == 0 ? channelValue : "1");
    response->print("\"><input type=\"hidden\" name=\"");
    response->print(strcmp(channel, "other") == 0 ? "release" : channelValue);
    response->print("\" value=\"1\"><input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"");
    response->print(_confirmCode);
    response->print("\"><input type=\"submit\"");
    if(strcmp(buttonClass, "") != 0)
    {
        response->print(" class=\"");
        response->print(buttonClass);
        response->print("\"");
    }
    response->print(" value=\"");
    response->print(label);
    response->print("\"></form>");
}

void WebCfgServer::printOtaImageForm(PsychicStreamResponse* response, bool keep, const char* label, const char* confirmMessage)
{
    response->print("<form action=\"/get\" method=\"get\"");
    if(strcmp(confirmMessage, "") != 0)
    {
        response->print(" data-confirm=\"");
        response->print(confirmMessage);
        response->print("\"");
    }
    response->print("><input type=\"hidden\" name=\"page\" value=\"otaimage\"><input type=\"hidden\" name=\"keep\" value=\"");
    response->print(keep ? "1" : "0");
    response->print("\"><input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"");
    response->print(_confirmCode);
    response->print("\"><input type=\"submit\" value=\"");
    response->print(label);
    response->print("\"></form>");
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
        response.print("<div class=\"banner danger\">Over-the-air update errored. Please check the logs for more info</div>");
    }

    if(_partitionType == 0)
    {
        response.print("<h4 class=\"warning\">You are currently running Nuki Hub with an outdated partition scheme. Because of this you cannot use OTA to update to 9.00 or higher. Please check GitHub for instructions on how to update to 9.00 and the new partition scheme</h4>");
        response.print("<div class=\"actions\"><a class=\"btn\" target=\"_blank\" href=\"");
        response.print(GITHUB_LATEST_RELEASE_URL);
        response.print("\">Open latest release on GitHub</a></div>");
        buildHtmlFooter(&response);
        return response.endSend();
    }

    response.print("<div id=\"msgdiv\" class=\"banner warn\" hidden><span id=\"uploadstatus\">Initiating Over-the-air update. This will take about two minutes, please be patient. You will be forwarded automatically when the update is complete.</span><progress id=\"uploadprogress\" max=\"100\" value=\"0\"></progress></div>");
#ifndef NUKI_HUB_UPDATER
    bool manifestSuccess = false;
    JsonDocument doc;

    if(_network->isInternetConnected())
    {
        manifestSuccess = NukiNetwork::fetchOtaManifest(doc, 2500);
    }

    response.print("<div id=\"autoupdform\"><h3>Update Nuki Hub</h3>");
    response.print("<p>Click on the button to reboot and automatically update Nuki Hub and the Nuki Hub updater to the latest versions from GitHub</p>");

    OtaImage currentImage = OtaImages::current();
    String imageReason;
    OtaImage recommendedImage = OtaImages::recommended(_network->networkDeviceType() == NetworkDeviceType::GL_S10, imageReason);
    bool keepImage = OtaImages::keepingCurrent(_preferences, recommendedImage);
    if(recommendedImage != OtaImage::Unknown && recommendedImage != currentImage)
    {
        if(keepImage)
        {
            response.print("<div class=\"banner image\"><div class=\"text\">Staying on the <b>");
            response.print(OtaImages::label(currentImage));
            response.print("</b> image although ");
            response.print(OtaImages::label(recommendedImage));
            response.print(" is recommended for this chip.</div>");
            printOtaImageForm(&response, false, "Switch to the recommended image on next update", "");
        }
        else
        {
            response.print("<div class=\"banner warn image\"><div class=\"text\">Updates will switch this hub to the <b>");
            response.print(OtaImages::label(recommendedImage));
            response.print("</b> image.<small>You are running the ");
            response.print(OtaImages::label(currentImage));
            response.print(" image, but ");
            response.print(imageReason);
            response.print(".</small></div>");
            String confirm = String("Keep updating the ") + OtaImages::label(currentImage) + " image although " + OtaImages::label(recommendedImage) + " is recommended?";
            printOtaImageForm(&response, true, "Keep the current image type", confirm.c_str());
        }
        response.print("</div>");
    }

    response.print("<div class=\"actions\">");

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
    String sameBuild = release_type == build_type ? "1" : "0";
    printOtaForm(&response, "release", "latestver", sameBuild.c_str(), "Do you really want to update to the latest release?", "ok", "Update to latest release", release_type.c_str());
    printOtaForm(&response, "beta", "betaver", sameBuild.c_str(), "Do you really want to update to the latest beta? This version could contain breaking bugs and necessitate downgrading to the latest release version using USB/Serial", "warn", "Update to latest beta", release_type.c_str());
    printOtaForm(&response, "master", "devver", sameBuild.c_str(), "Do you really want to update to the latest development version? This version could contain breaking bugs and necessitate downgrading to the latest release version using USB/Serial", "danger", "Update to latest development version", release_type.c_str());
#if defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_SPIRAM) && !defined(CONFIG_FREERTOS_UNICORE)
    if(recommendedImage == OtaImage::Unknown)
    {
        printOtaForm(&response, "other", "", "0", "Only continue if your board has PSRAM. Do you really want to update to the ESP32 with PSRAM image?", "", "Update to the ESP32 with PSRAM image (only if your board has PSRAM)", "1");
    }
#endif
    response.print("</div>");

    response.print("<table><tr><td>Current version</td><td><span id=\"currentver\">");
    response.print(NUKI_HUB_VERSION);
    response.print(" (");
    response.print(NUKI_HUB_BUILD);
    response.print(")</span>, ");
    response.print(NUKI_HUB_DATE);
    response.print("</td></tr>");

    response.print("<tr><td>Current image</td><td>");
    response.print(OtaImages::label(currentImage));
    response.print(" (");
    response.print(OtaImages::name(currentImage));
    response.print(")</td></tr>");
    if(recommendedImage != OtaImage::Unknown && recommendedImage != currentImage)
    {
        response.print("<tr><td>Recommended image</td><td>");
        response.print(OtaImages::label(recommendedImage));
        response.print(" (");
        response.print(OtaImages::name(recommendedImage));
        response.print("), ");
        response.print(imageReason);
        if(keepImage)
        {
            response.print(", staying on the current image by choice");
        }
        response.print("</td></tr>");
    }
    if(!manifestSuccess)
    {
        response.print("<tr><td>Latest versions</td><td>Could not be retrieved from GitHub<span id=\"latestver\" hidden>latestver</span><span id=\"devver\" hidden>devver</span><span id=\"betaver\" hidden>betaver</span></td></tr></table>");
    }
    else
    {
        response.print("<tr><td>Latest release version</td><td><span id=\"latestver\">");
        response.print(doc["release"]["fullversion"].as<const char*>());
        response.print(" (");
        response.print(doc["release"]["build"].as<const char*>());
        response.print(")</span>, ");
        response.print(doc["release"]["time"].as<const char*>());
        response.print("</td></tr>");
        response.print("<tr><td>Latest beta version</td><td><span id=\"betaver\">");
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
        response.print("</td></tr>");
        response.print("<tr><td>Latest development version</td><td><span id=\"devver\">");
        response.print(doc["master"]["fullversion"].as<const char*>());
        response.print(" (");
        response.print(doc["master"]["build"].as<const char*>());
        response.print(")</span>, ");
        response.print(doc["master"]["time"].as<const char*>());
        response.print("</td></tr></table>");

        const char* latestVersion = doc[NukiNetwork::otaChannel(doc)]["fullversion"];

        if(strcmp(latestVersion, _preferences->getString(preference_latest_version).c_str()) != 0)
        {
            _preferences->putString(preference_latest_version, latestVersion);
        }

        _network->publishLatestVersion(latestVersion);
    }
#endif
    response.print("</div>");

    if(_partitionType == 1)
    {
        response.print("<h3 id=\"manualtitle\"><a href=\"#manualupdate\" data-toggle=\"#manualupdate\">Manually update Nuki Hub</a></h3><div id=\"manualupdate\" hidden>");
        response.print("<div id=\"rebootform\"><h4>Reboot to Nuki Hub Updater</h4>");
        response.print("<p>Click on the button to reboot to the Nuki Hub updater, where you can select the latest Nuki Hub binary to update</p>");
        response.print("<form action=\"/get\" method=\"get\">");
        response.print("<input type=\"hidden\" name=\"page\" value=\"reboottoota\">");
        response.print("<input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\"><div class=\"actions\"><input type=\"submit\" value=\"Reboot to Nuki Hub Updater\"></div></form></div>");
        response.print("<div id=\"upform\"><h4>Update Nuki Hub Updater</h4>");
        response.print("<p>Select the latest Nuki Hub updater binary to update the Nuki Hub updater</p>");
        response.print(String("<form data-upload data-max-size=\"") + otaBinarySizeLimit + "\" data-size-error=\"This file is too large for the updater slot. Upload nuki_hub_updater.bin here; nuki_hub.bin must be uploaded from the Nuki Hub updater.\" enctype=\"multipart/form-data\" action=\"/uploadota\" method=\"post\"><table><tr><td>nuki_hub_updater.bin file</td><td><input name=\"uploadedfile\" type=\"file\" accept=\".bin\"></td></tr></table>");
    }
    else
    {
        response.print("<div id=\"manualupdate\">");
        response.print("<div id=\"rebootform\"><h3>Reboot to Nuki Hub</h3>");
        response.print("<p>Click on the button to reboot to Nuki Hub</p>");
        response.print("<form action=\"/get\" method=\"get\">");
        response.print("<input type=\"hidden\" name=\"page\" value=\"reboottoota\">");
        response.print("<input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\"><div class=\"actions\"><input type=\"submit\" value=\"Reboot to Nuki Hub\"></div></form></div>");
        response.print("<div id=\"upform\"><h3>Update Nuki Hub</h3>");
        response.print("<p>Select the latest Nuki Hub binary to update Nuki Hub</p>");
        response.print(String("<form data-upload") + (_partitionType == 2 ? String(" data-min-size=\"") + otaBinarySizeLimit + "\" data-size-error=\"This file is too small to be Nuki Hub. Upload nuki_hub.bin here; nuki_hub_updater.bin must be uploaded from Nuki Hub.\"" : String()) + " enctype=\"multipart/form-data\" action=\"/uploadota\" method=\"post\"><table><tr><td>nuki_hub.bin file</td><td><input name=\"uploadedfile\" type=\"file\" accept=\".bin\"></td></tr></table>");
    }
    response.print("<div class=\"actions\"><input id=\"submitbtn\" type=\"submit\" value=\"Upload File\"></div></form></div>");
    response.print("<div id=\"gitdiv\">");
    response.print("<h4>GitHub</h4>");
    response.print("<div class=\"actions stack\"><a class=\"btn\" target=\"_blank\" href=\"");
    response.print(GITHUB_LATEST_RELEASE_URL);
    response.print("\">Open latest release on GitHub</a>");
    response.print("<a class=\"btn\" href=\"");
    response.print(OtaImages::url("release", OtaImages::current(), false));
    response.print("\">Download latest binary from GitHub</a>");
    response.print("<a class=\"btn\" href=\"");
    response.print(OtaImages::url("release", OtaImages::current(), true));
    response.print("\">Download latest updater binary from GitHub</a></div></div></div>");
    buildHtmlFooter(&response);
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

    // The multipart parser ignores the return value and keeps delivering chunks.
    if (!index)
    {
        _otaRejected = false;
    }
    else if (_otaRejected)
    {
        return(ESP_FAIL);
    }

    if (!Update.hasError())
    {
        if (!index)
        {
            Update.clearError();

            Log->println("Starting manual OTA update");
            _otaContentLen = request->contentLength();

            if(_partitionType == 1 && _otaContentLen > otaBinarySizeLimit)
            {
                _otaRejected = true;
                Log->println("Uploaded OTA file too large, are you trying to upload a Nuki Hub binary instead of a Nuki Hub updater binary?");
                return(ESP_FAIL);
            }
            else if(_partitionType == 2 && _otaContentLen < otaBinarySizeLimit)
            {
                _otaRejected = true;
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

    OtaImage target = OtaImages::updateTarget(_preferences, _network->networkDeviceType() == NetworkDeviceType::GL_S10);
    const char* channel = "release";
    const char* channelName = "RELEASE";
    if(request->hasParam("beta"))
    {
        channel = "beta";
        channelName = "BETA";
    }
    else if(request->hasParam("master"))
    {
        channel = "master";
        channelName = "DEVELOPMENT";
    }
#if defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_SPIRAM) && !defined(CONFIG_FREERTOS_UNICORE)
    else if(request->hasParam("other"))
    {
        target = OtaImage::Esp32;
    }
#endif
    String message = String("Rebooting to update Nuki Hub and Nuki Hub updater<br/>Updating to latest ") + channelName + " version";
    if(target != OtaImages::current())
    {
        message += "<br/>Switching to the ";
        message += OtaImages::label(target);
        message += " image";
    }
    res = buildConfirmHtml(request, resp, message, 2, true);
    _preferences->putString(preference_ota_updater_url, OtaImages::url(channel, target, true));
    _preferences->putString(preference_ota_main_url, OtaImages::url(channel, target, false));
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

