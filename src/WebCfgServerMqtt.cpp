#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include "FS.h"
#include "SPIFFS.h"

#ifndef NUKI_HUB_UPDATER
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
                file.close();

                printTextarea(&response, "MQTTCA", "MQTT SSL CA Certificate", "*", certSize(), true, true);
                found = true;
            }
        }

        if (!found)
        {
            printTextarea(&response, "MQTTCA", "MQTT SSL CA Certificate", "", certSize(), true, true);
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
                file.close();

                printTextarea(&response, "MQTTCRT", "MQTT SSL Client Certificate", "*", certSize(), true, true);
                found = true;
            }
        }

        if (!found)
        {
            printTextarea(&response, "MQTTCRT", "MQTT SSL Client Certificate", "", certSize(), true, true);
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
                file.close();

                printTextarea(&response, "MQTTKEY", "MQTT SSL Client Key", "*", certSize(), true, true);
                found = true;
            }
        }

        if (!found)
        {
            printTextarea(&response, "MQTTKEY", "MQTT SSL Client Key", "", certSize(), true, true);
        }
    }
    response.print("</table>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form>");
    response.print("</body>");
    response.print("</html>");
    return response.endSend();
}

void WebCfgServer::processMqttArgs(PsychicRequest *request, ProcessArgsState& state, String& message)
{
    bool clearMqttCredentials = false;
    String value;

    if(request->hasParam("MQTTSERVER"))
    {
        value = request->getParam("MQTTSERVER")->value();
        if(_preferences->getString(preference_mqtt_broker, "") != value)
        {
            _preferences->putString(preference_mqtt_broker, value);

            state.restartServicesReconnect = true;
        }
    }

    if(request->hasParam("MQTTPORT"))
    {
        value = request->getParam("MQTTPORT")->value();
        if(_preferences->getInt(preference_mqtt_broker_port, 0) !=  value.toInt())
        {
            _preferences->putInt(preference_mqtt_broker_port,  value.toInt());

            state.restartServicesReconnect = true;
        }
    }

    if(request->hasParam("MQTTUSER"))
    {
        value = request->getParam("MQTTUSER")->value();
        if(value == "#")
        {
            clearMqttCredentials = true;
        }
        else
        {
            if(_preferences->getString(preference_mqtt_user, "") != value)
            {
                _preferences->putString(preference_mqtt_user, value);

                state.restartServicesReconnect = true;
            }
        }
    }

    if(request->hasParam("MQTTPASS"))
    {
        value = request->getParam("MQTTPASS")->value();
        if(value != "*")
        {
            if(_preferences->getString(preference_mqtt_password, "") != value)
            {
                _preferences->putString(preference_mqtt_password, value);
                state.restartServicesReconnect = true;
            }
        }
    }

    if(request->hasParam("MQTTPATH"))
    {
        value = request->getParam("MQTTPATH")->value();
        if(_preferences->getString(preference_mqtt_lock_path, "") != value)
        {
            _preferences->putString(preference_mqtt_lock_path, value);

            state.restartServicesReconnect = true;
        }
    }

    if(request->hasParam("MQTTCA"))
    {
        value = request->getParam("MQTTCA")->value();
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
                state.restartServicesReconnect = true;
            }
        }
    }

    if(request->hasParam("MQTTCRT"))
    {
        value = request->getParam("MQTTCRT")->value();
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
                state.restartServicesReconnect = true;
            }
        }
    }

    if(request->hasParam("MQTTKEY"))
    {
        value = request->getParam("MQTTKEY")->value();
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
                state.restartServicesReconnect = true;
            }
        }
    }

    if(request->hasParam("HADEVDISC"))
    {
        bool v = isParameterTrue(request, "DUOAPPROVAL");
        if(_preferences->getBool(preference_hass_device_discovery, false) != v)
        {
            _network->disableHASS();
            _preferences->putBool(preference_hass_device_discovery, v);
            state.restartServicesReconnect = true;
        }
    }

    if(request->hasParam("ENHADISC"))
    {
        bool v = isParameterTrue(request, "ENHADISC");
        if(_preferences->getBool(preference_mqtt_hass_enabled, false) != v)
        {
            _network->disableHASS();
            _preferences->putBool(preference_mqtt_hass_enabled, v);

            state.restartServicesReconnect = true;
        }
    }

    if(request->hasParam("HASSDISCOVERY"))
    {
        value = request->getParam("HASSDISCOVERY")->value();
        if(_preferences->getString(preference_mqtt_hass_discovery, "") != value)
        {
            _network->disableHASS();
            _preferences->putString(preference_mqtt_hass_discovery, value);

            state.restartServicesReconnect = true;
        }
    }

    if(request->hasParam("OPENERCONT"))
    {
        bool v = isParameterTrue(request, "OPENERCONT");
        if(_preferences->getBool(preference_opener_continuous_mode, false) != v)
        {
            _preferences->putBool(preference_opener_continuous_mode, v);
        }
    }

    if(request->hasParam("NETTIMEOUT"))
    {
        value = request->getParam("NETTIMEOUT")->value();
        if(_preferences->getInt(preference_network_timeout, 60) != value.toInt())
        {
            _preferences->putInt(preference_network_timeout, value.toInt());

        }
    }

    if(request->hasParam("MQTTLOG"))
    {
        bool v = isParameterTrue(request, "MQTTLOG");
        if(_preferences->getBool(preference_mqtt_log_enabled, false) != v)
        {
            _preferences->putBool(preference_mqtt_log_enabled, v);
            state.configChanged = true;
        }
    }

    if(request->hasParam("MQTTSENA"))
    {
        bool v = isParameterTrue(request, "MQTTSENA");
        if(_preferences->getBool(preference_mqtt_ssl_enabled, false) != v)
        {
            _preferences->putBool(preference_mqtt_ssl_enabled, v);

            state.restartServicesReconnect = true;
        }
    }

    if(request->hasParam("UPDATEMQTT"))
    {
        bool v = isParameterTrue(request, "UPDATEMQTT");
        if(_preferences->getBool(preference_update_from_mqtt, false) != v)
        {
            _preferences->putBool(preference_update_from_mqtt, v);

            state.restartServicesReconnect = true;
        }
    }

    if(request->hasParam("OFFHYBRID"))
    {
        bool v = isParameterTrue(request, "OFFHYBRID");
        if(_preferences->getBool(preference_official_hybrid_enabled, false) != v)
        {
            _preferences->putBool(preference_official_hybrid_enabled, v);
            if(v)
            {
                _preferences->putBool(preference_register_as_app, true);
            }

            state.restartServicesReconnect = true;
        }
    }

    if(request->hasParam("HYBRIDACT"))
    {
        bool v = isParameterTrue(request, "HYBRIDACT");
        if(_preferences->getBool(preference_official_hybrid_actions, false) != v)
        {
            _preferences->putBool(preference_official_hybrid_actions, v);
            if(v)
            {
                _preferences->putBool(preference_register_as_app, true);
            }
        }
    }

    if(request->hasParam("HYBRIDTIMER"))
    {
        value = request->getParam("HYBRIDTIMER")->value();
        if(_preferences->getInt(preference_query_interval_hybrid_lockstate, 600) != value.toInt())
        {
            _preferences->putInt(preference_query_interval_hybrid_lockstate, value.toInt());

        }
    }

    if(request->hasParam("HYBRIDRETRY"))
    {
        value = request->getParam("HYBRIDRETRY")->value();
        if(_preferences->getBool(preference_official_hybrid_retry, false) != (value == "1"))
        {
            _preferences->putBool(preference_official_hybrid_retry, (value == "1"));

        }
    }

    if(request->hasParam("HYBRIDREBOOT"))
    {
        bool v = isParameterTrue(request, "HYBRIDACT");
        if(_preferences->getBool(preference_hybrid_reboot_on_disconnect, false) != v)
        {
            _preferences->putBool(preference_hybrid_reboot_on_disconnect, v);

            state.restartServicesReconnect = true;
        }
    }

    if(request->hasParam("DISNONJSON"))
    {
        bool v = isParameterTrue(request, "DISNONJSON");
        if(_preferences->getBool(preference_disable_non_json, false) != v)
        {
            _preferences->putBool(preference_disable_non_json, v);

            state.restartServicesReconnect = true;
        }
    }

    if(clearMqttCredentials)
    {
        if(_preferences->getString(preference_mqtt_user, "") != "")
        {
            _preferences->putString(preference_mqtt_user, "");
            Log->print("Setting changed: ");
            Log->println("MQTTUSER");
            state.restartServicesReconnect = true;
        }
        if(_preferences->getString(preference_mqtt_password, "") != "")
        {
            _preferences->putString(preference_mqtt_password, "");
            Log->print("Setting changed: ");
            Log->println("MQTTPASS");
            state.restartServicesReconnect = true;
        }
    }
}

#endif
