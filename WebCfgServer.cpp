#include "WebCfgServer.h"
#include "PreferencesKeys.h"
#include "Version.h"
#include "hardware/WifiEthServer.h"

WebCfgServer::WebCfgServer(NukiWrapper* nuki, NukiOpenerWrapper* nukiOpener, Network* network, EthServer* ethServer, Preferences* preferences, bool allowRestartToPortal)
: _server(ethServer),
  _nuki(nuki),
  _nukiOpener(nukiOpener),
  _network(network),
  _preferences(preferences),
  _allowRestartToPortal(allowRestartToPortal)
{
    _confirmCode = generateConfirmCode();

    String str = _preferences->getString(preference_cred_user);

    if(str.length() > 0)
    {
        _hasCredentials = true;
        const char *user = str.c_str();
        memcpy(&_credUser, user, str.length());

        str = _preferences->getString(preference_cred_password);
        const char *pass = str.c_str();
        memcpy(&_credPassword, pass, str.length());
    }
}

void WebCfgServer::initialize()
{
    _server.on("/", [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }
        String response = "";
        buildHtml(response);
        _server.send(200, "text/html", response);
    });
    _server.on("/cred", [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }
        String response = "";
        buildCredHtml(response);
        _server.send(200, "text/html", response);
    });
    _server.on("/mqttconfig", [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }
        String response = "";
        buildMqttConfigHtml(response);
        _server.send(200, "text/html", response);
    });
    _server.on("/nukicfg", [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }
        String response = "";
        buildNukiConfigHtml(response);
        _server.send(200, "text/html", response);
    });
    _server.on("/wifi", [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }
        String response = "";
        buildConfigureWifiHtml(response);
        _server.send(200, "text/html", response);
    });
    _server.on("/unpairlock", [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }

        processUnpair(false);
    });
    _server.on("/unpairopener", [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }

        processUnpair(true);
    });
    _server.on("/wifimanager", [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }
        if(_allowRestartToPortal)
        {
            String response = "";
            buildConfirmHtml(response, "Restarting. Connect to ESP access point to reconfigure WiFi.", 0);
            _server.send(200, "text/html", response);
            waitAndProcess(true, 2000);
            _network->restartAndConfigureWifi();
        }
    });
    _server.on("/method=get", [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }
        String message = "";
        bool restartEsp = processArgs(message);
        if(restartEsp)
        {
            String response = "";
            buildConfirmHtml(response, message);
            _server.send(200, "text/html", response);
            Serial.println(F("Restarting"));

            waitAndProcess(true, 1000);
            ESP.restart();
        }
        else
        {
            String response = "";
            buildConfirmHtml(response, message, 3);
            _server.send(200, "text/html", response);
            waitAndProcess(false, 1000);
        }
    });
    _server.on("/ota", [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }
        String response = "";
        buildOtaHtml(response);
        _server.send(200, "text/html", response);
    });
    _server.on("/uploadota", HTTP_POST, [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }

        _server.send(200, "text/html", "");
    }, [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }

        handleOtaUpload();
    });

    _server.begin();
}

bool WebCfgServer::processArgs(String& message)
{
    bool configChanged = false;
    bool clearMqttCredentials = false;
    bool clearCredentials = false;

    int count = _server.args();
    for(int index = 0; index < count; index++)
    {
        String key = _server.argName(index);
        String value = _server.arg(index);

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
        else if(key == "LSTINT")
        {
            _preferences->putInt(preference_query_interval_lockstate, value.toInt());
            configChanged = true;
        }
        else if(key == "BATINT")
        {
            _preferences->putInt(preference_query_interval_battery, value.toInt());
            configChanged = true;
        }
        else if(key == "PRDTMO")
        {
            _preferences->putInt(preference_presence_detection_timeout, value.toInt());
            configChanged = true;
        }
        else if(key == "PUBAUTH")
        {
            _preferences->putBool(preference_publish_authdata, (value == "1"));
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
            _preferences->putString(preference_cred_password, value);
            configChanged = true;
        }
        else if(key == "NUKIPIN" && _nuki != nullptr)
        {
            if(value == "#")
            {
                message = "NUKI Lock PIN cleared";
                _nuki->setPin(0xffff);
            }
            else
            {
                message = "NUKI Lock PIN saved";
                _nuki->setPin(value.toInt());
            }
        }
        else if(key == "NUKIOPPIN" && _nukiOpener != nullptr)
        {
            if(value == "#")
            {
                message = "NUKI Opener PIN cleared";
                _nukiOpener->setPin(0xffff);
            }
            else
            {
                message = "NUKI Opener PIN saved";
                _nukiOpener->setPin(value.toInt());
            }
        }
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

    if(configChanged)
    {
        message = "Configuration saved ... restarting.";
        _enabled = false;
        _preferences->end();
    }

    return configChanged;
}

void WebCfgServer::update()
{
    if(!_enabled) return;

    _server.handleClient();
}

void WebCfgServer::buildHtml(String& response)
{
    buildHtmlHeader(response);

    response.concat("<br><h3>Info</h3>\n");

    String version = nuki_hub_version;

    response.concat("<table>");

    printParameter(response, "MQTT Connected", _network->isMqttConnected() ? "Yes" : "No");
    if(_nuki != nullptr)
    {
        char lockstateArr[20];
        NukiLock::lockstateToString(_nuki->keyTurnerState().lockState, lockstateArr);
        printParameter(response, "NUKI Lock paired", _nuki->isPaired() ? "Yes" : "No");
        printParameter(response, "NUKI Lock state", lockstateArr);
    }
    if(_nukiOpener != nullptr)
    {
        char lockstateArr[20];
        NukiOpener::lockstateToString(_nukiOpener->keyTurnerState().lockState, lockstateArr);
        printParameter(response, "NUKI Opener paired", _nukiOpener->isPaired() ? "Yes" : "No");
        printParameter(response, "NUKI Opener state", lockstateArr);
    }
    printParameter(response, "Firmware", version.c_str());
    response.concat("</table><br><br>");

    response.concat("<h3>MQTT and Network Configuration</h3>");
    buildNavigationButton(response, "Edit", "/mqttconfig");

    response.concat("<BR><BR><h3>NUKI Configuration</h3>");
    buildNavigationButton(response, "Edit", "/nukicfg");

    response.concat("<BR><BR><h3>Credentials</h3>");
    buildNavigationButton(response, "Edit", "/cred");

    response.concat("<BR><BR><h3>Firmware update</h3>");
    buildNavigationButton(response, "Open", "/ota");

    if(_allowRestartToPortal)
    {
        response.concat("<br><br><h3>WiFi</h3>");
        buildNavigationButton(response, "Restart and configure wifi", "/wifi");
    }

    response.concat("</BODY></HTML>");
}


void WebCfgServer::buildCredHtml(String &response)
{
    buildHtmlHeader(response);

    response.concat("<FORM ACTION=method=get >");
    response.concat("<h3>Credentials</h3>");
    response.concat("<table>");
    printInputField(response, "CREDUSER", "User (# to clear)", _preferences->getString(preference_cred_user).c_str(), 20);
    printInputField(response, "CREDPASS", "Password", "*", 30, true);
    response.concat("</table>");
    response.concat("<br><INPUT TYPE=SUBMIT NAME=\"submit\" VALUE=\"Save\">");
    response.concat("</FORM>");

    if(_nuki != nullptr)
    {
        response.concat("<br><br><FORM ACTION=method=get >");
        response.concat("<h3>NUKI Lock PIN</h3>");
        response.concat("<table>");
        printInputField(response, "NUKIPIN", "PIN Code (# to clear)", "*", 20, true);
        response.concat("</table>");
        response.concat("<br><INPUT TYPE=SUBMIT NAME=\"submit\" VALUE=\"Save\">");
        response.concat("</FORM>");
    }

    if(_nukiOpener != nullptr)
    {
        response.concat("<br><br><FORM ACTION=method=get >");
        response.concat("<h3>NUKI Opener PIN</h3>");
        response.concat("<table>");
        printInputField(response, "NUKIOPPIN", "PIN Code (# to clear)", "*", 20, true);
        response.concat("</table>");
        response.concat("<br><INPUT TYPE=SUBMIT NAME=\"submit\" VALUE=\"Save\">");
        response.concat("</FORM>");
    }

    _confirmCode = generateConfirmCode();
    if(_nuki != nullptr)
    {
        response.concat("<br><br><h3>Unpair NUKI Lock</h3>");
        response.concat("<form method=\"get\" action=\"/unpairlock\">");
        String message = "Type ";
        message.concat(_confirmCode);
        message.concat(" to confirm unpair");
        printInputField(response, "CONFIRMTOKEN", message.c_str(), "", 10);
        response.concat("<br><br><button type=\"submit\">OK</button></form>");
    }

    if(_nukiOpener != nullptr)
    {
        response.concat("<br><br><h3>Unpair NUKI Opener</h3>");
        response.concat("<form method=\"get\" action=\"/unpairopener\">");
        String message = "Type ";
        message.concat(_confirmCode);
        message.concat(" to confirm unpair");
        printInputField(response, "CONFIRMTOKEN", message.c_str(), "", 10);
        response.concat("<br><br><button type=\"submit\">OK</button></form>");
    }
    response.concat("</BODY></HTML>");
}

void WebCfgServer::buildOtaHtml(String &response)
{
    buildHtmlHeader(response);
    response.concat("<form id=\"upform\" enctype=\"multipart/form-data\" action=\"/uploadota\" method=\"POST\"><input type=\"hidden\" name=\"MAX_FILE_SIZE\" value=\"100000\" />Choose a file to upload: <input name=\"uploadedfile\" type=\"file\" accept=\".bin\" /><br/>");
    response.concat("<br><input id=\"submitbtn\" type=\"submit\" value=\"Upload File\" /></form>");
    response.concat("<div id=\"msgdiv\" style=\"visibility:hidden\">Initiating Over-the-air update. This will take about a minute, please be patient.<br>You will be forwarwed automatically when the update is complete.</div>");
    response.concat("<script type=\"text/javascript\">");
    response.concat("window.addEventListener('load', function () {");
    response.concat("	var button = document.getElementById(\"submitbtn\");");
    response.concat("	button.addEventListener('click',hideshow,false);");
    response.concat("	function hideshow() {");
    response.concat("		document.getElementById('upform').style.visibility = 'hidden';");
    response.concat("		document.getElementById('msgdiv').style.visibility = 'visible';");
    response.concat("		setTimeout(\"location.href = '/';\",60000);");
    response.concat("	}");
    response.concat("});");
    response.concat("</script>");
    response.concat("</BODY></HTML>");
}

void WebCfgServer::buildMqttConfigHtml(String &response)
{
    buildHtmlHeader(response);
    response.concat("<FORM ACTION=method=get >");
    response.concat("<h3>MQTT Configuration</h3>");
    response.concat("<table>");
    printInputField(response, "HOSTNAME", "Host name", _preferences->getString(preference_hostname).c_str(), 100);
    printInputField(response, "MQTTSERVER", "MQTT Broker", _preferences->getString(preference_mqtt_broker).c_str(), 100);
    printInputField(response, "MQTTPORT", "MQTT Broker port", _preferences->getInt(preference_mqtt_broker_port), 5);
    printInputField(response, "MQTTUSER", "MQTT User (# to clear)", _preferences->getString(preference_mqtt_user).c_str(), 30);
    printInputField(response, "MQTTPASS", "MQTT Password", "*", 30, true);
    printTextarea(response, "MQTTCA", "MQTT SSL CA Certificate (*, optional)", _preferences->getString(preference_mqtt_ca).c_str(), TLS_CA_MAX_SIZE);
    printTextarea(response, "MQTTCRT", "MQTT SSL Client Certificate (*, optional)", _preferences->getString(preference_mqtt_crt).c_str(), TLS_CERT_MAX_SIZE);
    printTextarea(response, "MQTTKEY", "MQTT SSL Client Key (*, optional)", _preferences->getString(preference_mqtt_key).c_str(), TLS_KEY_MAX_SIZE);
    printInputField(response, "NETTIMEOUT", "Network Timeout until restart (seconds; -1 to disable)", _preferences->getInt(preference_network_timeout), 5);
    response.concat("</table>");
    response.concat("* If no encryption is configured for the MQTT broeker, leave empty.<br>");

    response.concat("<br><INPUT TYPE=SUBMIT NAME=\"submit\" VALUE=\"Save\">");
    response.concat("</FORM>");
    response.concat("</BODY></HTML>");
}


void WebCfgServer::buildNukiConfigHtml(String &response)
{
    buildHtmlHeader(response);

    response.concat("<FORM ACTION=method=get >");
    response.concat("<h3>MQTT Configuration</h3>");
    response.concat("<table>");
    printCheckBox(response, "LOCKENA", "NUKI Lock enabled", _preferences->getBool(preference_lock_enabled));
    if(_preferences->getBool(preference_lock_enabled))
    {
        printInputField(response, "MQTTPATH", "MQTT Lock Path", _preferences->getString(preference_mqtt_lock_path).c_str(), 180);
    }
    printCheckBox(response, "OPENA", "NUKI Opener enabled", _preferences->getBool(preference_opener_enabled));
    if(_preferences->getBool(preference_opener_enabled))
    {
        printInputField(response, "MQTTOPPATH", "MQTT Opener Path", _preferences->getString(preference_mqtt_opener_path).c_str(), 180);
    }
    printInputField(response, "LSTINT", "Query interval lock state (seconds)", _preferences->getInt(preference_query_interval_lockstate), 10);
    printInputField(response, "BATINT", "Query interval battery (seconds)", _preferences->getInt(preference_query_interval_battery), 10);
    printCheckBox(response, "PUBAUTH", "Publish auth data (May reduce battery life)", _preferences->getBool(preference_publish_authdata));
    printInputField(response, "PRDTMO", "Presence detection timeout (seconds; -1 to disable)", _preferences->getInt(preference_presence_detection_timeout), 10);
    response.concat("</table>");
    response.concat("<br><INPUT TYPE=SUBMIT NAME=\"submit\" VALUE=\"Save\">");
    response.concat("</FORM>");
    response.concat("</BODY></HTML>");
}

void WebCfgServer::buildConfirmHtml(String &response, const String &message, uint32_t redirectDelay)
{
    String delay(redirectDelay);

    response.concat("<HTML>\n");
    response.concat("<HEAD>\n");
    response.concat("<TITLE>NUKI Hub</TITLE>\n");
    response.concat("<meta http-equiv=\"Refresh\" content=\"");
    response.concat(redirectDelay);
    response.concat("; url=/\" />");
    response.concat("\n</HEAD>\n");
    response.concat("<BODY>\n");
    response.concat(message);

    response.concat("</BODY></HTML>");
}

void WebCfgServer::buildConfigureWifiHtml(String &response)
{
    buildHtmlHeader(response);

    response.concat("<h3>WiFi</h3>");
    response.concat("Click confirm to restart ESP into WiFi configuration mode. After restart, connect to ESP access point to reconfigure WiFI.<br><br>");
    buildNavigationButton(response, "Confirm", "/wifimanager");

    response.concat("</BODY></HTML>");
}

void WebCfgServer::processUnpair(bool opener)
{
    String response = "";
    if(_server.args() == 0)
    {
        buildConfirmHtml(response, "Confirm code is invalid.", 3);
        _server.send(200, "text/html", response);
        return;
    }
    else
    {
        String key = _server.argName(0);
        String value = _server.arg(0);

        if(key != "CONFIRMTOKEN" || value != _confirmCode)
        {
            buildConfirmHtml(response, "Confirm code is invalid.", 3);
            _server.send(200, "text/html", response);
            return;
        }
    }

    buildConfirmHtml(response, opener ? "Unpairing NUKI Opener and restarting." : "Unpairing NUKI Lock and restarting.", 3);
    _server.send(200, "text/html", response);
    if(!opener && _nuki != nullptr)
    {
        _nuki->unpair();
    }
    if(opener && _nukiOpener != nullptr)
    {
        _nukiOpener->unpair();
    }
    waitAndProcess(false, 1000);
    ESP.restart();
}

void WebCfgServer::buildHtmlHeader(String &response)
{
    response.concat("<HTML><HEAD>");
    response.concat("<meta name='viewport' content='width=device-width, initial-scale=1'>");
    response.concat("<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/open-fonts@1.1.1/fonts/inter.min.css'><link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/@exampledev/new.css@1.1.2/new.min.css'>");
    response.concat("<TITLE>NUKI Hub</TITLE></HEAD><BODY>");
}

void WebCfgServer::printInputField(String& response,
                                   const char *token,
                                   const char *description,
                                   const char *value,
                                   const size_t maxLength,
                                   const bool isPassword)
{
    char maxLengthStr[20];

    itoa(maxLength, maxLengthStr, 10);

    response.concat("<tr><td>");
    response.concat(description);
    response.concat("</td><td>");
    response.concat("<INPUT TYPE=");
    response.concat(isPassword ? "PASSWORD" : "TEXT");
    response.concat(" VALUE=\"");
    response.concat(value);
    response.concat("\" NAME=\"");
    response.concat(token);
    response.concat("\" SIZE=\"25\" MAXLENGTH=\"");
    response.concat(maxLengthStr);
    response.concat("\\\"/>");
    response.concat("</td></tr>\"");
}

void WebCfgServer::printInputField(String& response,
                                   const char *token,
                                   const char *description,
                                   const int value,
                                   size_t maxLength)
{
    char valueStr[20];
    itoa(value, valueStr, 10);
    printInputField(response, token, description, valueStr, maxLength);
}

void WebCfgServer::printCheckBox(String &response, const char *token, const char *description, const bool value)
{
    response.concat("<tr><td>");
    response.concat(description);
    response.concat("</td><td>");

    response.concat("<INPUT TYPE=hidden NAME=\"");
    response.concat(token);
    response.concat("\" value=\"0\"");
    response.concat("/>");

    response.concat("<INPUT TYPE=checkbox NAME=\"");
    response.concat(token);
    response.concat("\" value=\"1\"");
    response.concat(value ? " checked=\"checked\"" : "");
    response.concat("/></td></tr>");
}

void WebCfgServer::printTextarea(String& response,
                                   const char *token,
                                   const char *description,
                                   const char *value,
                                   const size_t maxLength)
{
    char maxLengthStr[20];

    itoa(maxLength, maxLengthStr, 10);

    response.concat("<tr><td>");
    response.concat(description);
    response.concat("</td><td>");
    response.concat(" <TEXTAREA NAME=\"");
    response.concat(token);
    response.concat("\" MAXLENGTH=\"");
    response.concat(maxLengthStr);
    response.concat("\\\">");
    response.concat(value);
    response.concat("</TEXTAREA>");
    response.concat("</td></tr>");
}

void WebCfgServer::buildNavigationButton(String &response, const char *caption, const char *targetPath)
{
    response.concat("<form method=\"get\" action=\"");
    response.concat(targetPath);
    response.concat("\">");
    response.concat("<button type=\"submit\">");
    response.concat(caption);
    response.concat("</button>");
    response.concat("</form>");
}

void WebCfgServer::printParameter(String& response, const char *description, const char *value)
{
    response.concat("<tr>");
    response.concat("<td>");
    response.concat(description);
    response.concat("</td>");
    response.concat("<td>");
    response.concat(value);
    response.concat("</td>");
    response.concat("</tr>");

}


String WebCfgServer::generateConfirmCode()
{
    int code = random(1000,9999);
    return String(code);
}

void WebCfgServer::waitAndProcess(const bool blocking, const uint32_t duration)
{
    unsigned long timeout = millis() + duration;
    while(millis() < timeout)
    {
        _server.handleClient();
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

void WebCfgServer::handleOtaUpload()
{
    if (_server.uri() != "/uploadota") {
        return;
    }

    esp_task_wdt_init(30, false);

    HTTPUpload& upload = _server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        if (!filename.startsWith("/")) {
            filename = "/" + filename;
        }
        Serial.print("handleFileUpload Name: "); Serial.println(filename);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        _transferredSize = _transferredSize + upload.currentSize;
        Serial.println(_transferredSize);
        _ota.updateFirmware(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
        Serial.println();
        Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
    }
}
