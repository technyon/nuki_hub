#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "hardware/WifiEthServer.h"
#include "Logger.h"
#include "Config.h"
#include "RestartReason.h"
#include <esp_task_wdt.h>

WebCfgServer::WebCfgServer(NukiWrapper* nuki, NukiOpenerWrapper* nukiOpener, Network* network, Gpio* gpio, EthServer* ethServer, Preferences* preferences, bool allowRestartToPortal)
: _server(ethServer),
  _nuki(nuki),
  _nukiOpener(nukiOpener),
  _network(network),
  _gpio(gpio),
  _preferences(preferences),
  _allowRestartToPortal(allowRestartToPortal)
{
    _confirmCode = generateConfirmCode();
    _hostname = _preferences->getString(preference_hostname);
    String str = _preferences->getString(preference_cred_user);

    if(str.length() > 0)
    {
        memset(&_credUser, 0, sizeof(_credUser));
        memset(&_credPassword, 0, sizeof(_credPassword));

        _hasCredentials = true;
        const char *user = str.c_str();
        memcpy(&_credUser, user, str.length());

        str = _preferences->getString(preference_cred_password);
        const char *pass = str.c_str();
        memcpy(&_credPassword, pass, str.length());
    }

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
    _server.on("/style.css", [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }
        sendCss();
    });
    _server.on("/favicon.ico", HTTP_GET, [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }
        sendFavicon();
    });
    _server.on("/acclvl", [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }
        String response = "";
        buildAccLvlHtml(response);
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
    _server.on("/gpiocfg", [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }
        String response = "";
        buildGpioConfigHtml(response);
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
            buildConfirmHtml(response, "Restarting. Connect to ESP access point to reconfigure Wi-Fi.", 0);
            _server.send(200, "text/html", response);
            waitAndProcess(true, 2000);
            _network->reconfigureDevice();
        }
    });
    _server.on("/savecfg", [&]()
    {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }
        String message = "";
        bool restart = processArgs(message);
        if(restart)
        {
            String response = "";
            buildConfirmHtml(response, message);
            _server.send(200, "text/html", response);
            Log->println(F("Restarting"));

            waitAndProcess(true, 1000);
            restartEsp(RestartReason::ConfigurationUpdated);
        }
        else
        {
            String response = "";
            buildConfirmHtml(response, message, 3);
            _server.send(200, "text/html", response);
            waitAndProcess(false, 1000);
        }
    });
    _server.on("/savegpiocfg", [&]()
    {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }
        processGpioArgs();

        String response = "";
        buildConfirmHtml(response, "");
        _server.send(200, "text/html", response);
        Log->println(F("Restarting"));

        waitAndProcess(true, 1000);
        restartEsp(RestartReason::GpioConfigurationUpdated);
    });

    _server.on("/ota", [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }
        String response = "";
        buildOtaHtml(response, _server.arg("errored") != "");
        _server.send(200, "text/html", response);
    });
    _server.on("/uploadota", HTTP_POST, [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }

        if (_ota.updateStarted() && _ota.updateCompleted()) {
            String response = "";
            buildOtaCompletedHtml(response);
            _server.send(200, "text/html", response);
            delay(2000);
            restartEsp(RestartReason::OTACompleted);
        } else {
            _ota.restart();
            _server.sendHeader("Location", "/ota?errored=true");
            _server.send(302, "text/plain", "");
        }
    }, [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }

        handleOtaUpload();
    });
    _server.on("/info", [&]() {
        if (_hasCredentials && !_server.authenticate(_credUser, _credPassword)) {
            return _server.requestAuthentication();
        }
        String response = "";
        buildInfoHtml(response);
        _server.send(200, "text/html", response);
    });
    _server.on("/debugon", [&]() {
        _preferences->putBool(preference_publish_debug_info, true);

        String response = "";
        buildConfirmHtml(response, "OK");
        _server.send(200, "text/html", response);
        Log->println(F("Restarting"));

        waitAndProcess(true, 1000);
        restartEsp(RestartReason::ConfigurationUpdated);
    });
    _server.on("/debugoff", [&]() {
        _preferences->putBool(preference_publish_debug_info, false);

        String response = "";
        buildConfirmHtml(response, "OK");
        _server.send(200, "text/html", response);
        Log->println(F("Restarting"));

        waitAndProcess(true, 1000);
        restartEsp(RestartReason::ConfigurationUpdated);
    });

    _server.begin();

    _network->setKeepAliveCallback([&]()
        {
            update();
        });
}

bool WebCfgServer::processArgs(String& message)
{
    bool configChanged = false;
    bool aclLvlChanged = false;
    bool clearMqttCredentials = false;
    bool clearCredentials = false;
    uint32_t aclPrefs[17] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    int count = _server.args();

    String pass1 = "";
    String pass2 = "";

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
        else if(key == "MQTTLOG")
        {
            _preferences->putBool(preference_mqtt_log_enabled, (value == "1"));
            configChanged = true;
        }
        else if(key == "CHECKUPDATE")
        {
            _preferences->putBool(preference_check_updates, (value == "1"));
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
        else if(key == "PRDTMO")
        {
            _preferences->putInt(preference_presence_detection_timeout, value.toInt());
            configChanged = true;
        }
        else if(key == "RSBC")
        {
            _preferences->putInt(preference_restart_ble_beacon_lost, value.toInt());
            configChanged = true;
        }
        else if(key == "ACLLVLCHANGED")
        {
            aclLvlChanged = true;
        }
        else if(key == "ACLCNF")
        {
            _preferences->putBool(preference_admin_enabled, (value == "1"));
            configChanged = true;
        }
        else if(key == "KPPUB")
        {
            _preferences->putBool(preference_keypad_info_enabled, (value == "1"));
            configChanged = true;
        }
        else if(key == "KPENA")
        {
            _preferences->putBool(preference_keypad_control_enabled, (value == "1"));
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
        else if(key == "REGAPP")
        {
            _preferences->putBool(preference_register_as_app, (value == "1"));
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
        }
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


void WebCfgServer::processGpioArgs()
{
    int count = _server.args();

    std::vector<PinEntry> pinConfiguration;

    for(int index = 0; index < count; index++)
    {
        String key = _server.argName(index);
        String value = _server.arg(index);

        PinRole role = (PinRole)value.toInt();
        if(role != PinRole::Disabled)
        {
            PinEntry entry;
            entry.pin = key.toInt();
            entry.role = role;
            pinConfiguration.push_back(entry);
        }
    }

    _gpio->savePinConfiguration(pinConfiguration);
}


void WebCfgServer::update()
{
    if(_otaStartTs > 0 && (millis() - _otaStartTs) > 120000)
    {
        Log->println(F("OTA time out, restarting"));
        delay(200);
        restartEsp(RestartReason::OTATimeout);
    }

    if(!_enabled) return;

    _server.handleClient();
}

void WebCfgServer::buildHtml(String& response)
{
    buildHtmlHeader(response);

    response.concat("<br><h3>Info</h3>\n");

    String version = NUKI_HUB_VERSION;

    response.concat("<table>");

    printParameter(response, "Hostname", _hostname.c_str());
    printParameter(response, "MQTT Connected", _network->mqttConnectionState() > 0 ? "Yes" : "No");
    if(_nuki != nullptr)
    {
        char lockstateArr[20];
        NukiLock::lockstateToString(_nuki->keyTurnerState().lockState, lockstateArr);
        printParameter(response, "Nuki Lock paired", _nuki->isPaired() ? ("Yes (BLE Address " + _nuki->getBleAddress().toString() + ")").c_str() : "No");
        printParameter(response, "Nuki Lock state", lockstateArr);
    }
    if(_nukiOpener != nullptr)
    {
        char lockstateArr[20];
        NukiOpener::lockstateToString(_nukiOpener->keyTurnerState().lockState, lockstateArr);
        printParameter(response, "Nuki Opener paired", _nukiOpener->isPaired() ? ("Yes (BLE Address " + _nukiOpener->getBleAddress().toString() + ")").c_str() : "No");

        if(_nukiOpener->keyTurnerState().nukiState == NukiOpener::State::ContinuousMode)
        {
            printParameter(response, "Nuki Opener state", "Open (Continuous Mode)");
        }
        else
        {
            printParameter(response, "Nuki Opener state", lockstateArr);
        }
    }
    printParameter(response, "Firmware", version.c_str(), "/info");

    if(_preferences->getBool(preference_check_updates))
    {
        //if(atof(_latestVersion) > atof(NUKI_HUB_VERSION) || (atof(_latestVersion) == atof(NUKI_HUB_VERSION) && _latestVersion != NUKI_HUB_VERSION)) {
        printParameter(response, "Latest Firmware", _preferences->getString(preference_latest_version).c_str(), "/ota");
        //}
    }

    response.concat("</table><br><table id=\"tblnav\"><tbody>");
    response.concat("<tr><td><h3>MQTT and Network Configuration</h3></td><td class=\"tdbtn\">");
    buildNavigationButton(response, "Edit", "/mqttconfig", _brokerConfigured ? "" : "<font color=\"#f07000\"><em>(!) Please configure MQTT broker</em></font>");
    response.concat("</td></tr><tr><td><h3>Nuki Configuration</h3></td><td class=\"tdbtn\">");
    buildNavigationButton(response, "Edit", "/nukicfg");
    response.concat("</td></tr><tr><td><h3>Access Level Configuration</h3></td><td class=\"tdbtn\">");
    buildNavigationButton(response, "Edit", "/acclvl");
    response.concat("</td></tr><tr><td><h3>Credentials</h3></td><td class=\"tdbtn\">");
    buildNavigationButton(response, "Edit", "/cred", _pinsConfigured ? "" : "<font color=\"#f07000\"><em>(!) Please configure PIN</em></font>");
    response.concat("</td></tr><tr><td><h3>GPIO Configuration</h3></td><td class=\"tdbtn\">");
    buildNavigationButton(response, "Edit", "/gpiocfg");
    response.concat("</td></tr><tr><td><h3>Firmware update</h3></td><td class=\"tdbtn\">");
    buildNavigationButton(response, "Open", "/ota");
    response.concat("</td></tr>");

    if(_allowRestartToPortal)
    {
        response.concat("<tr><td><h3>Wi-Fi</h3></td><td class=\"tdbtn\">");
        buildNavigationButton(response, "Restart and configure Wi-Fi", "/wifi");
        response.concat("</td></tr>");
    }

    response.concat("</tbody></table></body></html>");
}


void WebCfgServer::buildCredHtml(String &response)
{
    buildHtmlHeader(response);

    response.concat("<form method=\"post\" action=\"savecfg\">");
    response.concat("<h3>Credentials</h3>");
    response.concat("<table>");
    printInputField(response, "CREDUSER", "User (# to clear)", _preferences->getString(preference_cred_user).c_str(), 30, false, true);
    printInputField(response, "CREDPASS", "Password", "*", 30, true, true);
    printInputField(response, "CREDPASSRE", "Retype password", "*", 30, true);
    response.concat("</table>");
    response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.concat("</form>");

    if(_nuki != nullptr)
    {
        response.concat("<br><br><form method=\"post\" action=\"savecfg\">");
        response.concat("<h3>Nuki Lock PIN</h3>");
        response.concat("<table>");
        printInputField(response, "NUKIPIN", "PIN Code (# to clear)", "*", 20, true);
        response.concat("</table>");
        response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
        response.concat("</form>");
    }

    if(_nukiOpener != nullptr)
    {
        response.concat("<br><br><form method=\"post\" action=\"savecfg\">");
        response.concat("<h3>Nuki Opener PIN</h3>");
        response.concat("<table>");
        printInputField(response, "NUKIOPPIN", "PIN Code (# to clear)", "*", 20, true);
        response.concat("</table>");
        response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
        response.concat("</form>");
    }

    _confirmCode = generateConfirmCode();
    if(_nuki != nullptr)
    {
        response.concat("<br><br><h3>Unpair Nuki Lock</h3>");
        response.concat("<form method=\"post\" action=\"/unpairlock\">");
        response.concat("<table>");
        String message = "Type ";
        message.concat(_confirmCode);
        message.concat(" to confirm unpair");
        printInputField(response, "CONFIRMTOKEN", message.c_str(), "", 10);
        response.concat("</table>");
        response.concat("<br><button type=\"submit\">OK</button></form>");
    }

    if(_nukiOpener != nullptr)
    {
        response.concat("<br><br><h3>Unpair Nuki Opener</h3>");
        response.concat("<form method=\"post\" action=\"/unpairopener\">");
        response.concat("<table>");
        String message = "Type ";
        message.concat(_confirmCode);
        message.concat(" to confirm unpair");
        printInputField(response, "CONFIRMTOKEN", message.c_str(), "", 10);
        response.concat("</table>");
        response.concat("<br><button type=\"submit\">OK</button></form>");
    }
    response.concat("</body></html>");
}

void WebCfgServer::buildOtaHtml(String &response, bool errored)
{
    buildHtmlHeader(response);

    if(millis() < 60000)
    {
        response.concat("OTA functionality not ready. Please wait a moment and reload.");
        response.concat("</body></html>");
        return;
    }

    if (errored) {
        response.concat("<div>Over-the-air update errored. Please check the logs for more info</div><br/>");
    }

    response.concat("<form id=\"upform\" enctype=\"multipart/form-data\" action=\"/uploadota\" method=\"post\"><input type=\"hidden\" name=\"MAX_FILE_SIZE\" value=\"100000\" />Choose the updated nuki_hub.bin file to upload: <input name=\"uploadedfile\" type=\"file\" accept=\".bin\" /><br/>");
    response.concat("<br><input id=\"submitbtn\" type=\"submit\" value=\"Upload File\" /></form>");

    if(_preferences->getBool(preference_check_updates))
    {
        response.concat("<div id=\"gitdiv\"><button title=\"Open latest release on GitHub\" onclick=\" window.open('");
        response.concat(GITHUB_LATEST_RELEASE_URL);
        response.concat("', '_blank'); return false;\">Open latest release on GitHub</button>");

        response.concat("<br><br><button title=\"Download latest binary from GitHub\" onclick=\" window.open('");
        response.concat(GITHUB_LATEST_RELEASE_BINARY_URL);
        response.concat("'); return false;\">Download latest binary from GitHub</button></div>");
    }

    response.concat("<div id=\"msgdiv\" style=\"visibility:hidden\">Initiating Over-the-air update. This will take about two minutes, please be patient.<br>You will be forwarded automatically when the update is complete.</div>");
    response.concat("<script type=\"text/javascript\">");
    response.concat("window.addEventListener('load', function () {");
    response.concat("	var button = document.getElementById(\"submitbtn\");");
    response.concat("	button.addEventListener('click',hideshow,false);");
    response.concat("	function hideshow() {");
    response.concat("		document.getElementById('upform').style.visibility = 'hidden';");
    response.concat("		document.getElementById('gitdiv').style.visibility = 'hidden';");
    response.concat("		document.getElementById('msgdiv').style.visibility = 'visible';");
    response.concat("	}");
    response.concat("});");
    response.concat("</script>");
    response.concat("</body></html>");
}

void WebCfgServer::buildOtaCompletedHtml(String &response)
{
    buildHtmlHeader(response);

    response.concat("<div>Over-the-air update completed.<br>You will be forwarded automatically.</div>");
    response.concat("<script type=\"text/javascript\">");
    response.concat("window.addEventListener('load', function () {");
    response.concat("   setTimeout(\"location.href = '/';\",10000);");
    response.concat("});");
    response.concat("</script>");
    response.concat("</body></html>");
}

void WebCfgServer::buildMqttConfigHtml(String &response)
{
    buildHtmlHeader(response);
    response.concat("<form method=\"post\" action=\"savecfg\">");
    response.concat("<h3>Basic MQTT and Network Configuration</h3>");
    response.concat("<table>");
    printInputField(response, "HOSTNAME", "Host name", _preferences->getString(preference_hostname).c_str(), 100);
    printInputField(response, "MQTTSERVER", "MQTT Broker", _preferences->getString(preference_mqtt_broker).c_str(), 100);
    printInputField(response, "MQTTPORT", "MQTT Broker port", _preferences->getInt(preference_mqtt_broker_port), 5);
    printInputField(response, "MQTTUSER", "MQTT User (# to clear)", _preferences->getString(preference_mqtt_user).c_str(), 30, false, true);
    printInputField(response, "MQTTPASS", "MQTT Password", "*", 30, true, true);
    response.concat("</table><br>");

    response.concat("<h3>Advanced MQTT and Network Configuration</h3>");
    response.concat("<table>");
    printInputField(response, "HASSDISCOVERY", "Home Assistant discovery topic (empty to disable; usually homeassistant)", _preferences->getString(preference_mqtt_hass_discovery).c_str(), 30);
    printInputField(response, "HASSCUURL", "Home Assistant device configuration URL (empty to use http://LOCALIP; fill when using a reverse proxy for example)", _preferences->getString(preference_mqtt_hass_cu_url).c_str(), 261);
    if(_nukiOpener != nullptr) printCheckBox(response, "OPENERCONT", "Set Nuki Opener Lock/Unlock action in Home Assistant to Continuous mode", _preferences->getBool(preference_opener_continuous_mode));
    printTextarea(response, "MQTTCA", "MQTT SSL CA Certificate (*, optional)", _preferences->getString(preference_mqtt_ca).c_str(), TLS_CA_MAX_SIZE, _network->encryptionSupported(), true);
    printTextarea(response, "MQTTCRT", "MQTT SSL Client Certificate (*, optional)", _preferences->getString(preference_mqtt_crt).c_str(), TLS_CERT_MAX_SIZE, _network->encryptionSupported(), true);
    printTextarea(response, "MQTTKEY", "MQTT SSL Client Key (*, optional)", _preferences->getString(preference_mqtt_key).c_str(), TLS_KEY_MAX_SIZE, _network->encryptionSupported(), true);
    printDropDown(response, "NWHW", "Network hardware", String(_preferences->getInt(preference_network_hardware)), getNetworkDetectionOptions());
    printCheckBox(response, "NWHWWIFIFB", "Disable fallback to Wi-Fi / Wi-Fi config portal", _preferences->getBool(preference_network_wifi_fallback_disabled));
    printInputField(response, "RSSI", "RSSI Publish interval (seconds; -1 to disable)", _preferences->getInt(preference_rssi_publish_interval), 6);
    printInputField(response, "NETTIMEOUT", "Network Timeout until restart (seconds; -1 to disable)", _preferences->getInt(preference_network_timeout), 5);
    printCheckBox(response, "RSTDISC", "Restart on disconnect", _preferences->getBool(preference_restart_on_disconnect));
    printCheckBox(response, "MQTTLOG", "Enable MQTT logging", _preferences->getBool(preference_mqtt_log_enabled));
    printCheckBox(response, "CHECKUPDATE", "Check for Firmware Updates every 24h", _preferences->getBool(preference_check_updates));
    response.concat("</table>");
    response.concat("* If no encryption is configured for the MQTT broker, leave empty. Only supported for Wi-Fi connections.<br><br>");

    response.concat("<h3>IP Address assignment</h3>");
    response.concat("<table>");
    printCheckBox(response, "DHCPENA", "Enable DHCP", _preferences->getBool(preference_ip_dhcp_enabled));
    printInputField(response, "IPADDR", "Static IP address", _preferences->getString(preference_ip_address).c_str(), 15);
    printInputField(response, "IPSUB", "Subnet", _preferences->getString(preference_ip_subnet).c_str(), 15);
    printInputField(response, "IPGTW", "Default gateway", _preferences->getString(preference_ip_gateway).c_str(), 15);
    printInputField(response, "DNSSRV", "DNS Server", _preferences->getString(preference_ip_dns_server).c_str(), 15);
    response.concat("</table>");

    response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.concat("</form>");
    response.concat("</body></html>");
}

void WebCfgServer::buildAccLvlHtml(String &response)
{
    buildHtmlHeader(response);
    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    response.concat("<form method=\"post\" action=\"savecfg\">");
    response.concat("<input type=\"hidden\" name=\"ACLLVLCHANGED\" value=\"1\">");
    response.concat("<h3>Nuki General Access Control</h3>");
    response.concat("<table><tr><th>Setting</th><th>Enabled</th></tr>");
    printCheckBox(response, "ACLCNF", "Change Nuki configuration", _preferences->getBool(preference_admin_enabled));
    if((_nuki != nullptr && (_preferences->getString(preference_lock_force_keypad) || _nuki->hasKeypad())) || (_nukiOpener != nullptr && (_preferences->getString(preference_opener_force_keypad) || _nukiOpener->hasKeypad())))
    {
        printCheckBox(response, "KPPUB", "Publish keypad codes information", _preferences->getBool(preference_keypad_info_enabled));
        printCheckBox(response, "KPENA", "Add, modify and delete keypad codes", _preferences->getBool(preference_keypad_control_enabled));
    }
    printCheckBox(response, "PUBAUTH", "Publish authorisation log (may reduce battery life)", _preferences->getBool(preference_publish_authdata));
    response.concat("</table><br>");
    if(_nuki != nullptr)
    {
        response.concat("<h3>Nuki Lock Access Control</h3>");
        response.concat("<table><tr><th>Action</th><th>Allowed</th></tr>");

        printCheckBox(response, "ACLLCKLCK", "Lock", ((int)aclPrefs[0] == 1));
        printCheckBox(response, "ACLLCKUNLCK", "Unlock", ((int)aclPrefs[1] == 1));
        printCheckBox(response, "ACLLCKUNLTCH", "Unlatch", ((int)aclPrefs[2] == 1));
        printCheckBox(response, "ACLLCKLNG", "Lock N Go", ((int)aclPrefs[3] == 1));
        printCheckBox(response, "ACLLCKLNGU", "Lock N Go Unlatch", ((int)aclPrefs[4] == 1));
        printCheckBox(response, "ACLLCKFLLCK", "Full Lock", ((int)aclPrefs[5] == 1));
        printCheckBox(response, "ACLLCKFOB1", "Fob Action 1", ((int)aclPrefs[6] == 1));
        printCheckBox(response, "ACLLCKFOB2", "Fob Action 2", ((int)aclPrefs[7] == 1));
        printCheckBox(response, "ACLLCKFOB3", "Fob Action 3", ((int)aclPrefs[8] == 1));
        response.concat("</table><br>");
    }
    if(_nukiOpener != nullptr)
    {
        response.concat("<h3>Nuki Opener Access Control</h3>");
        response.concat("<table><tr><th>Action</th><th>Allowed</th></tr>");

        printCheckBox(response, "ACLOPNUNLCK", "Activate Ring-to-Open", ((int)aclPrefs[9] == 1));
        printCheckBox(response, "ACLOPNLCK", "Deactivate Ring-to-Open", ((int)aclPrefs[10] == 1));
        printCheckBox(response, "ACLOPNUNLTCH", "Electric Strike Actuation", ((int)aclPrefs[11] == 1));
        printCheckBox(response, "ACLOPNUNLCKCM", "Activate Continuous Mode", ((int)aclPrefs[12] == 1));
        printCheckBox(response, "ACLOPNLCKCM", "Deactivate Continuous Mode", ((int)aclPrefs[13] == 1));
        printCheckBox(response, "ACLOPNFOB1", "Fob Action 1", ((int)aclPrefs[14] == 1));
        printCheckBox(response, "ACLOPNFOB2", "Fob Action 2", ((int)aclPrefs[15] == 1));
        printCheckBox(response, "ACLOPNFOB3", "Fob Action 3", ((int)aclPrefs[16] == 1));
        response.concat("</table><br>");
    }
    response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.concat("</form>");
    response.concat("</body></html>");
}

void WebCfgServer::buildNukiConfigHtml(String &response)
{
    buildHtmlHeader(response);

    response.concat("<form method=\"post\" action=\"savecfg\">");
    response.concat("<h3>Basic Nuki Configuration</h3>");
    response.concat("<table>");
    printCheckBox(response, "LOCKENA", "Nuki Smartlock enabled", _preferences->getBool(preference_lock_enabled));
    if(_preferences->getBool(preference_lock_enabled))
    {
        printInputField(response, "MQTTPATH", "MQTT Nuki Smartlock Path", _preferences->getString(preference_mqtt_lock_path).c_str(), 180);
        printCheckBox(response, "LCKFORCEKPAD", "Force Lock Keypad availability", _preferences->getBool(preference_lock_force_keypad));
        printCheckBox(response, "LCKFORCEDRSNSR", "Force Lock Door Sensor availability", _preferences->getBool(preference_lock_force_doorsensor));
    }
    printCheckBox(response, "OPENA", "Nuki Opener enabled", _preferences->getBool(preference_opener_enabled));
    if(_preferences->getBool(preference_opener_enabled))
    {
        printInputField(response, "MQTTOPPATH", "MQTT Nuki Opener Path", _preferences->getString(preference_mqtt_opener_path).c_str(), 180);
        printCheckBox(response, "OPFORCEKPAD", "Force Opener Keypad availability", _preferences->getBool(preference_opener_force_keypad));
    }
    response.concat("</table><br>");

    response.concat("<h3>Advanced Nuki Configuration</h3>");
    response.concat("<table>");

    printInputField(response, "LSTINT", "Query interval lock state (seconds)", _preferences->getInt(preference_query_interval_lockstate), 10);
    printInputField(response, "CFGINT", "Query interval configuration (seconds)", _preferences->getInt(preference_query_interval_configuration), 10);
    printInputField(response, "BATINT", "Query interval battery (seconds)", _preferences->getInt(preference_query_interval_battery), 10);
    if((_nuki != nullptr && (_preferences->getString(preference_lock_force_keypad) || _nuki->hasKeypad())) || (_nukiOpener != nullptr && (_preferences->getString(preference_opener_force_keypad) || _nukiOpener->hasKeypad())))
    {
        printInputField(response, "KPINT", "Query interval keypad (seconds)", _preferences->getInt(preference_query_interval_keypad), 10);
    }
    printInputField(response, "NRTRY", "Number of retries if command failed", _preferences->getInt(preference_command_nr_of_retries), 10);
    printInputField(response, "TRYDLY", "Delay between retries (milliseconds)", _preferences->getInt(preference_command_retry_delay), 10);
    printCheckBox(response, "REGAPP", "Nuki Bridge is running alongside Nuki Hub (needs re-pairing if changed)", _preferences->getBool(preference_register_as_app));
    printInputField(response, "PRDTMO", "Presence detection timeout (seconds; -1 to disable)", _preferences->getInt(preference_presence_detection_timeout), 10);
    printInputField(response, "RSBC", "Restart if bluetooth beacons not received (seconds; -1 to disable)", _preferences->getInt(preference_restart_ble_beacon_lost), 10);
    response.concat("</table>");
    response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.concat("</form>");
    response.concat("</body></html>");
}

void WebCfgServer::buildGpioConfigHtml(String &response)
{
    buildHtmlHeader(response);

    response.concat("<form method=\"post\" action=\"savegpiocfg\">");
    response.concat("<h3>GPIO Configuration</h3>");
    response.concat("<table>");

    const auto& availablePins = _gpio->availablePins();
    for(const auto& pin : availablePins)
    {
        String pinStr = String(pin);
        String pinDesc = "Gpio " + pinStr;

        printDropDown(response, pinStr.c_str(), pinDesc.c_str(), getPreselectionForGpio(pin), getGpioOptions());
    }

    response.concat("</table>");
    response.concat("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.concat("</form>");
    response.concat("</body></html>");
}

void WebCfgServer::buildConfirmHtml(String &response, const String &message, uint32_t redirectDelay)
{
    String delay(redirectDelay);

    response.concat("<html>\n");
    response.concat("<head>\n");
    response.concat("<title>Nuki Hub</title>\n");
    response.concat("<meta http-equiv=\"Refresh\" content=\"");
    response.concat(redirectDelay);
    response.concat("; url=/\" />");
    response.concat("\n</head>\n");
    response.concat("<body>\n");
    response.concat(message);

    response.concat("</body></html>");
}

void WebCfgServer::buildConfigureWifiHtml(String &response)
{
    buildHtmlHeader(response);

    response.concat("<h3>Wi-Fi</h3>");
    response.concat("Click confirm to restart ESP into Wi-Fi configuration mode. After restart, connect to ESP access point to reconfigure Wi-Fi.<br><br>");
    buildNavigationButton(response, "Confirm", "/wifimanager");

    response.concat("</body></html>");
}

void WebCfgServer::buildInfoHtml(String &response)
{
    DebugPreferences debugPreferences;

    buildHtmlHeader(response);
    response.concat("<h3>System Information</h3> <pre>");

    response.concat("Nuki Hub version: ");
    response.concat(NUKI_HUB_VERSION);
    response.concat("\n");

    response.concat(debugPreferences.preferencesToString(_preferences));

    response.concat("MQTT connected: ");
    response.concat(_network->mqttConnectionState() > 0 ? "Yes\n" : "No\n");

    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    if(_nuki != nullptr)
    {
        response.concat("Lock firmware version: ");
        response.concat(_nuki->firmwareVersion().c_str());
        response.concat("\nLock hardware version: ");
        response.concat(_nuki->hardwareVersion().c_str());
        response.concat("\nLock paired: ");
        response.concat(_nuki->isPaired() ? "Yes\n" : "No\n");
        response.concat("Lock PIN set: ");
        response.concat(_nuki->isPaired() ? _nuki->isPinSet() ? "Yes\n" : "No\n" : "-\n");
        response.concat("Lock has door sensor: ");
        response.concat(_nuki->hasDoorSensor() ? "Yes\n" : "No\n");
        response.concat("Lock has keypad: ");
        response.concat(_nuki->hasKeypad() ? "Yes\n" : "No\n");
        response.concat("Lock ACL (Lock): ");
        response.concat((int)aclPrefs[0] ? "Allowed\n" : "Disallowed\n");
        response.concat("Lock ACL (Unlock): ");
        response.concat((int)aclPrefs[1] ? "Allowed\n" : "Disallowed\n");
        response.concat("Lock ACL (Unlatch): ");
        response.concat((int)aclPrefs[2] ? "Allowed\n" : "Disallowed\n");
        response.concat("Lock ACL (Lock N Go): ");
        response.concat((int)aclPrefs[3] ? "Allowed\n" : "Disallowed\n");
        response.concat("Lock ACL (Lock N Go Unlatch): ");
        response.concat((int)aclPrefs[4] ? "Allowed\n" : "Disallowed\n");
        response.concat("Lock ACL (Full Lock): ");
        response.concat((int)aclPrefs[5] ? "Allowed\n" : "Disallowed\n");
        response.concat("Lock ACL (Fob Action 1): ");
        response.concat((int)aclPrefs[6] ? "Allowed\n" : "Disallowed\n");
        response.concat("Lock ACL (Fob Action 2): ");
        response.concat((int)aclPrefs[7] ? "Allowed\n" : "Disallowed\n");
        response.concat("Lock ACL (Fob Action 3): ");
        response.concat((int)aclPrefs[8] ? "Allowed\n" : "Disallowed\n");
    }

    if(_nukiOpener != nullptr)
    {
        response.concat("Opener firmware version: ");
        response.concat(_nukiOpener->firmwareVersion().c_str());
        response.concat("\nOpener hardware version: ");
        response.concat(_nukiOpener->hardwareVersion().c_str());        response.concat("\nOpener paired: ");
        response.concat(_nukiOpener->isPaired() ? "Yes\n" : "No\n");
        response.concat("Opener PIN set: ");
        response.concat(_nukiOpener->isPaired() ? _nukiOpener->isPinSet() ? "Yes\n" : "No\n" : "-\n");
        response.concat("Opener has keypad: ");
        response.concat(_nukiOpener->hasKeypad() ? "Yes\n" : "No\n");
        response.concat("Opener ACL (Activate Ring-to-Open): ");
        response.concat((int)aclPrefs[9] ? "Allowed\n" : "Disallowed\n");
        response.concat("Opener ACL (Deactivate Ring-to-Open): ");
        response.concat((int)aclPrefs[10] ? "Allowed\n" : "Disallowed\n");
        response.concat("Opener ACL (Electric Strike Actuation): ");
        response.concat((int)aclPrefs[11] ? "Allowed\n" : "Disallowed\n");
        response.concat("Opener ACL (Activate Continuous Mode): ");
        response.concat((int)aclPrefs[12] ? "Allowed\n" : "Disallowed\n");
        response.concat("Opener ACL (Deactivate Continuous Mode): ");
        response.concat((int)aclPrefs[13] ? "Allowed\n" : "Disallowed\n");
        response.concat("Opener ACL (Fob Action 1): ");
        response.concat((int)aclPrefs[14] ? "Allowed\n" : "Disallowed\n");
        response.concat("Opener ACL (Fob Action 2): ");
        response.concat((int)aclPrefs[15] ? "Allowed\n" : "Disallowed\n");
        response.concat("Opener ACL (Fob Action 3): ");
        response.concat((int)aclPrefs[16] ? "Allowed\n" : "Disallowed\n");
    }

    response.concat("Network device: ");
    response.concat(_network->networkDeviceName());
    response.concat("\n");

    response.concat("Uptime: ");
    response.concat(millis() / 1000 / 60);
    response.concat(" minutes\n");

    response.concat("Heap: ");
    response.concat(esp_get_free_heap_size());
    response.concat("\n");

    response.concat("Stack watermarks: nw: ");
    response.concat(uxTaskGetStackHighWaterMark(networkTaskHandle));
    response.concat(", nuki: ");
    response.concat(uxTaskGetStackHighWaterMark(nukiTaskHandle));
    response.concat(", pd: ");
    response.concat(uxTaskGetStackHighWaterMark(presenceDetectionTaskHandle));
    response.concat("\n");

    _gpio->getConfigurationText(response, _gpio->pinConfiguration());

    response.concat("Restart reason FW: ");
    response.concat(getRestartReason());
    response.concat( "\n");

    response.concat("Restart reason ESP: ");
    response.concat(getEspRestartReason());
    response.concat("\n");

    response.concat("</pre> </body></html>");
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

    buildConfirmHtml(response, opener ? "Unpairing Nuki Opener and restarting." : "Unpairing Nuki Lock and restarting.", 3);
    _server.send(200, "text/html", response);
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

void WebCfgServer::buildHtmlHeader(String &response)
{
    response.concat("<html><head>");
    response.concat("<meta name='viewport' content='width=device-width, initial-scale=1'>");
//    response.concat("<style>");
//    response.concat(stylecss);
//    response.concat("</style>");
    response.concat("<link rel='stylesheet' href='/style.css'>");
    response.concat("<title>Nuki Hub</title></head><body>");

    srand(millis());
}

void WebCfgServer::printInputField(String& response,
                                   const char *token,
                                   const char *description,
                                   const char *value,
                                   const size_t& maxLength,
                                   const bool& isPassword,
                                   const bool& showLengthRestriction)
{
    char maxLengthStr[20];

    itoa(maxLength, maxLengthStr, 10);

    response.concat("<tr><td>");
    response.concat(description);

    if(showLengthRestriction)
    {
        response.concat(" (Max. ");
        response.concat(maxLength);
        response.concat(" characters)");
    }

    response.concat("</td><td>");
    response.concat("<input type=");
    response.concat(isPassword ? "password" : "text");
    response.concat(" value=\"");
    response.concat(value);
    response.concat("\" name=\"");
    response.concat(token);
    response.concat("\" size=\"25\" maxlength=\"");
    response.concat(maxLengthStr);
    response.concat("\"/>");
    response.concat("</td></tr>");
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

    response.concat("<input type=hidden name=\"");
    response.concat(token);
    response.concat("\" value=\"0\"");
    response.concat("/>");

    response.concat("<input type=checkbox name=\"");
    response.concat(token);
    response.concat("\" value=\"1\"");
    response.concat(value ? " checked=\"checked\"" : "");
    response.concat("/></td></tr>");
}

void WebCfgServer::printTextarea(String& response,
                                   const char *token,
                                   const char *description,
                                   const char *value,
                                   const size_t& maxLength,
                                   const bool& enabled,
                                   const bool& showLengthRestriction)
{
    char maxLengthStr[20];

    itoa(maxLength, maxLengthStr, 10);

    response.concat("<tr><td>");
    response.concat(description);
    if(showLengthRestriction)
    {
        response.concat(" (Max. ");
        response.concat(maxLength);
        response.concat(" characters)");
    }
    response.concat("</td><td>");
    response.concat(" <textarea ");
    if(!enabled)
    {
        response.concat("disabled");
    }
    response.concat(" name=\"");
    response.concat(token);
    response.concat("\" maxlength=\"");
    response.concat(maxLengthStr);
    response.concat("\">");
    response.concat(value);
    response.concat("</textarea>");
    response.concat("</td></tr>");
}

void WebCfgServer::printDropDown(String &response, const char *token, const char *description, const String preselectedValue, const std::vector<std::pair<String, String>> options)
{
    response.concat("<tr><td>");
    response.concat(description);
    response.concat("</td><td>");

    response.concat("<select name=\"");
    response.concat(token);
    response.concat("\">");

    for(const auto option : options)
    {
        if(option.first == preselectedValue)
        {
            response.concat("<option selected=\"selected\" value=\"");
        }
        else
        {
            response.concat("<option value=\"");
        }
        response.concat(option.first);
        response.concat("\">");
        response.concat(option.second);
        response.concat("</option>");
    }

    response.concat("</select>");
    response.concat("</td></tr>");
}

void WebCfgServer::buildNavigationButton(String &response, const char *caption, const char *targetPath, const char* labelText)
{
    response.concat("<form method=\"get\" action=\"");
    response.concat(targetPath);
    response.concat("\">");
    response.concat("<button type=\"submit\">");
    response.concat(caption);
    response.concat("</button> ");
    response.concat(labelText);
    response.concat("</form>");
}

void WebCfgServer::printParameter(String& response, const char *description, const char *value, const char *link)
{
    response.concat("<tr>");
    response.concat("<td>");
    response.concat(description);
    response.concat("</td>");
    response.concat("<td>");
    if(strcmp(link, "") == 0)
    {
        response.concat(value);
    }
    else
    {
        response.concat("<a href=\"");
        response.concat(link);
        response.concat("\"> ");
        response.concat(value);
        response.concat("</a>");
    }
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
    if (_server.uri() != "/uploadota")
    {
        return;
    }
    if(millis() < 60000)
    {
        return;
    }

    HTTPUpload& upload = _server.upload();

    if(upload.filename == "")
    {
        Log->println("Invalid file for OTA upload");
        return;
    }

    if (upload.status == UPLOAD_FILE_START)
    {
        String filename = upload.filename;
        if (!filename.startsWith("/"))
        {
            filename = "/" + filename;
        }
        _otaStartTs = millis();
        esp_task_wdt_init(30, false);
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
        Log->print("handleFileUpload Name: "); Log->println(filename);
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        _transferredSize = _transferredSize + upload.currentSize;
        Log->println(_transferredSize);
        _ota.updateFirmware(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END)
    {
        Log->println();
        Log->print("handleFileUpload Size: "); Log->println(upload.totalSize);
    }
    else if(upload.status == UPLOAD_FILE_ABORTED)
    {
        Log->println();
        Log->println("OTA aborted, restarting ESP.");
        restartEsp(RestartReason::OTAAborted);
    }
    else
    {
        Log->println();
        Log->print("OTA unknown state: ");
        Log->println((int)upload.status);
        restartEsp(RestartReason::OTAUnknownState);
    }
}

void WebCfgServer::sendCss()
{
    // escaped by https://www.cescaper.com/
    _server.send(200, "text/css", stylecss, sizeof(stylecss));
}

void WebCfgServer::sendFavicon()
{
    _server.send(200, "image/png", (const char*)favicon_32x32, sizeof(favicon_32x32));
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