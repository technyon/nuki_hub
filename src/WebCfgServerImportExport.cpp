#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include "ArduinoJson.h"

#ifndef NUKI_HUB_UPDATER
esp_err_t WebCfgServer::buildImportExportHtml(PsychicRequest *request, PsychicResponse* resp)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<div id=\"upform\"><h4>Import configuration</h4>");
    response.print("<form method=\"post\" action=\"post\"><input type=\"file\" id=\"importfile\" accept=\".json,application/json\" onchange=\"var reader = new FileReader(); reader.onload = function(e) { document.getElementById('importjson').value = e.target.result; }; reader.readAsText(this.files[0]);\"><br/><br/>");
    response.print("<textarea id=\"importjson\" name=\"importjson\" rows=\"10\" cols=\"50\"></textarea><br/>");
    response.print("<input type=\"hidden\" name=\"page\" value=\"import\">");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Import\"></form><br><br></div>");
    response.print("<div id=\"gitdiv\">");
    response.print("<h4>Export configuration</h4><br>");
    response.print("<button title=\"Export without sensitive settings\" onclick=\" window.open('/get?page=export', '_self'); return false;\">Export without sensitive settings</button>");
    response.print("<br><br><button title=\"Export including sensitive settings\" onclick=\" window.open('/get?page=export&redacted=1'); return false;\">Export including sensitive settings</button>");
    response.print("<br><br><button title=\"Export including sensitive settings and pairing data\" onclick=\" window.open('/get?page=export&redacted=1&pairing=1'); return false;\">Export including sensitive settings and pairing data</button>");
    if(nuki_hub_https_server_enabled)
    {
        response.print("<br><br><button title=\"Export HTTP SSL certificate and key\" onclick=\" window.open('/get?page=export&type=https'); return false;\">Export HTTP SSL certificate and key</button>");
    }
    response.print("<br><br><button title=\"Export MQTT SSL CA, client certificate and client key\" onclick=\" window.open('/get?page=export&type=mqtts'); return false;\">Export MQTT SSL CA, client certificate and client key</button>");
    response.print("<br><br><button title=\"Export Coredump\" onclick=\" window.open('/get?page=coredump'); return false;\">Export Coredump</button>");
    response.print("</div></body></html>");
    return response.endSend();
}

bool WebCfgServer::processImport(PsychicRequest *request, PsychicResponse* resp, String& message)
{
    bool configChanged = false;

    if(request->hasParam("importjson"))
    {
        const PsychicWebParameter* p = request->getParam("importjson");
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

#endif
