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
    response.print("<h3>Import configuration</h3>");
    response.print("<form method=\"post\" action=\"post\"><table>");
    response.print("<tr><td>Configuration file</td><td><input type=\"file\" id=\"importfile\" accept=\".json,application/json\" data-fill=\"#importjson\"></td></tr>");
    response.print("<tr><td>Configuration JSON</td><td><textarea id=\"importjson\" name=\"importjson\"></textarea></td></tr></table>");
    response.print("<input type=\"hidden\" name=\"page\" value=\"import\">");
    response.print("<div class=\"actions\"><input type=\"submit\" name=\"submit\" value=\"Import\"></div></form>");
    response.print("<h3>Export configuration</h3>");
    response.print("<div class=\"actions stack\">");
    response.print("<a class=\"btn\" href=\"/get?page=export\">Export without sensitive settings</a>");
    response.print("<a class=\"btn\" href=\"/get?page=export&redacted=1\">Export including sensitive settings</a>");
    response.print("<a class=\"btn\" href=\"/get?page=export&redacted=1&pairing=1\">Export including sensitive settings and pairing data</a>");
    if(nuki_hub_https_server_enabled)
    {
        response.print("<a class=\"btn\" href=\"/get?page=export&type=https\">Export HTTP SSL certificate and key</a>");
    }
    response.print("<a class=\"btn\" href=\"/get?page=export&type=mqtts\">Export MQTT SSL CA, client certificate and client key</a>");
    response.print("<a class=\"btn\" href=\"/get?page=coredump\">Export Coredump</a>");
    response.print("</div>");
    buildHtmlFooter(&response);
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
