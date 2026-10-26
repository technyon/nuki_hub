#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include "FS.h"
#include "SPIFFS.h"

#ifndef NUKI_HUB_UPDATER
esp_err_t WebCfgServer::buildAdvancedConfigHtml(PsychicRequest *request, PsychicResponse* resp)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response.print("<h3>Advanced Configuration</h3>");
    response.print("<h4 class=\"warning\">Warning: Changing these settings can lead to bootloops that might require you to erase the ESP32 and reflash Nuki Hub using USB/serial</h4>");
    response.print("<table>");
    response.print("<tr><td>Current bootloop prevention state</td><td>");
    response.print(_preferences->getBool(preference_enable_bootloop_reset, false) ? "Enabled" : "Disabled");
    response.print("</td></tr>");
    printCheckBox(&response, "DISNTWNOCON", "Disable Network if not connected within 60s", _preferences->getBool(preference_disable_network_not_connected, false), "");
    printCheckBox(&response, "WEBLOG", "Enable WebSerial logging", _preferences->getBool(preference_webserial_enabled), "");
    printCheckBox(&response, "FRCHSTUPD", "Force slave Hosted update on next boot", _preferences->getBool(preference_force_hosted_update, false), "");
    printCheckBox(&response, "BTLPRST", "Enable Bootloop prevention (Try to reset these settings to default on bootloop)", true, "");
    printInputField(&response, "BUFFSIZE", "Char buffer size (min 4096, max 65536)", _preferences->getInt(preference_buffer_size, CHAR_BUFFER_SIZE), 6, "");
    printInputField(&response, "TSKNTWK", "Task size Network (min 12288, max 65536)", _preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE), 6, "");
    response.print("<tr><td>Measured peak network task stack usage since boot</td><td>");
    response.print(_preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE) - uxTaskGetStackHighWaterMark(networkTaskHandle));
    response.print("</td></tr>");
    printInputField(&response, "TSKNUKI", "Task size Nuki (min 8192, max 65536)", _preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE), 6, "");
    response.print("<tr><td>Measured peak Nuki task stack usage since boot</td><td>");
    response.print(_preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE) - uxTaskGetStackHighWaterMark(nukiTaskHandle));
    response.print("</td></tr>");
    printInputField(&response, "BLEGENTIMEOUT", "BLE General timeout in ms (min 10000, max 65536)", _preferences->getInt(preference_ble_general_timeout, 10000), 6, "");
    printInputField(&response, "BLECMDTIMEOUT", "BLE Command timeout in ms (min 3000, max 65536)", _preferences->getInt(preference_ble_command_timeout, 3000), 6, "");
    printInputField(&response, "ALMAX", "Max auth log entries (min 1, max 100)", _preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG), 3, "id=\"inputmaxauthlog\"");
    printInputField(&response, "KPMAX", "Max keypad entries (min 1, max 200)", _preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD), 3, "id=\"inputmaxkeypad\"");
    printInputField(&response, "TCMAX", "Max timecontrol entries (min 1, max 100)", _preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL), 3, "id=\"inputmaxtimecontrol\"");
    printInputField(&response, "AUTHMAX", "Max authorization entries (min 1, max 100)", _preferences->getInt(preference_auth_max_entries, MAX_AUTH), 3, "id=\"inputmaxauth\"");
    printCheckBox(&response, "SHOWSECRETS", "Show Pairing secrets on Info page", _preferences->getBool(preference_show_secrets), "");
    if(_preferences->getBool(preference_lock_enabled, true))
    {
        printCheckBox(&response, "LCKMANPAIR", "Manually set lock pairing data (enable to save values below)", false, "");
        printInputField(&response, "LCKBLEADDR", "currentBleAddress", "", 12, "");
        printInputField(&response, "LCKSECRETK", "secretKeyK", "", 64, "");
        printInputField(&response, "LCKAUTHID", "authorizationId", "", 8, "");
        printCheckBox(&response, "LCKISULTRA", "isUltra", false, "");
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

    if(_nuki != nullptr)
    {
        char uidString[20];
        itoa(_preferences->getUInt(preference_nuki_id_lock, 0), uidString, 16);
        printCheckBox(&response, "LCKFORCEID", ((String)"Force Lock ID to current ID (" + uidString + ")").c_str(), _preferences->getBool(preference_lock_force_id, false), "");
        printCheckBox(&response, "LCKFORCEKP", "Force Lock Keypad connected", _preferences->getBool(preference_lock_force_keypad, false), "");
        printCheckBox(&response, "LCKFORCEDS", "Force Lock Doorsensor connected", _preferences->getBool(preference_lock_force_doorsensor, false), "");
    }

    if(_nukiOpener != nullptr)
    {
        char uidString[20];
        itoa(_preferences->getUInt(preference_nuki_id_opener, 0), uidString, 16);
        printCheckBox(&response, "OPFORCEID", ((String)"Force Opener ID to current ID (" + uidString + ")").c_str(), _preferences->getBool(preference_opener_force_id, false), "");
        printCheckBox(&response, "OPFORCEKP", "Force Opener Keypad", _preferences->getBool(preference_opener_force_keypad, false), "");
    }

    printCheckBox(&response, "DBGCONN", "Enable Nuki connect debug logging", _preferences->getBool(preference_debug_connect, false), "");
    printCheckBox(&response, "DBGCOMMU", "Enable Nuki communication debug logging", _preferences->getBool(preference_debug_communication, false), "");
    printCheckBox(&response, "DBGREAD", "Enable Nuki readable data debug logging", _preferences->getBool(preference_debug_readable_data, false), "");
    printCheckBox(&response, "DBGHEX", "Enable Nuki hex data debug logging", _preferences->getBool(preference_debug_hex_data, false), "");
    printCheckBox(&response, "DBGCOMM", "Enable Nuki command debug logging", _preferences->getBool(preference_debug_command, false), "");
    printCheckBox(&response, "DBGHEAP", "Publish free heap over MQTT", _preferences->getBool(preference_publish_debug_info, false), "");
    response.print("</table>");

    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form>");
    response.print("</body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::processFactoryReset(PsychicRequest *request, PsychicResponse* resp)
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
        return buildConfirmHtml(request, resp, "Confirm code is invalid.", 3, true);
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
            res = buildConfirmHtml(request, resp, "Factory resetting Nuki Hub, unpairing Nuki Lock and Nuki Opener and resetting WiFi.", 3, true);
        }
        else
        {
            res = buildConfirmHtml(request, resp, "Factory resetting Nuki Hub, unpairing Nuki Lock and Nuki Opener.", 3, true);
        }
    }

    waitAndProcess(false, 2000);

    if(_nuki != nullptr)
    {
        _nuki->unpair();
    }
    if(_nukiOpener != nullptr)
    {
        _nukiOpener->unpair();
    }

    String ssid = _preferences->getString(preference_wifi_ssid, "");
    String pass = _preferences->getString(preference_wifi_pass, "");

    _network->disableHASS();
    _preferences->clear();

#ifndef CONFIG_IDF_TARGET_ESP32H2
    if(!resetWifi)
    {
        _preferences->putString(preference_wifi_ssid, ssid);
        _preferences->putString(preference_wifi_pass, pass);
    }
#endif

    waitAndProcess(false, 3000);
    restartEsp(RestartReason::NukiHubReset);
    return res;
}

void WebCfgServer::processAdvancedArgs(PsychicRequest *request, ProcessArgsState& state, String& message)
{
    bool manPairLck = false;
    bool manPairOpn = false;
    unsigned char currentBleAddress[6];
    unsigned char authorizationId[4] = {0x00};
    unsigned char secretKeyK[32] = {0x00};
    unsigned char pincode[2] = {0x00};
    unsigned char ultraPincode[4] = {0x00};
    bool isUltra = false;
    unsigned char currentBleAddressOpn[6];
    unsigned char authorizationIdOpn[4] = {0x00};
    unsigned char secretKeyKOpn[32] = {0x00};
    String value;

    if(request->hasParam("WEBLOG"))
    {
        bool v = isParameterTrue(request, "WEBLOG");
        if(_preferences->getBool(preference_webserial_enabled, false) != v)
        {
            _preferences->putBool(preference_webserial_enabled, v);

            state.restartServicesNoReconnect = true;
        }
    }

    if(request->hasParam("FRCHSTUPD"))
    {
        bool v = isParameterTrue(request, "FRCHSTUPD");
        if(_preferences->getBool(preference_force_hosted_update, false) != v)
        {
            _preferences->putBool(preference_force_hosted_update, v);

            state.configChanged = true;
        }
    }

    if(request->hasParam("TSKNTWK"))
    {
        value = request->getParam("TSKNTWK")->value();
        if(value.toInt() > 12287 && value.toInt() < 65537)
        {
            if(_preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE) != value.toInt())
            {
                _preferences->putInt(preference_task_size_network, value.toInt());
                state.configChanged = true;
            }
        }
    }

    if(request->hasParam("TSKNUKI"))
    {
        value = request->getParam("TSKNUKI")->value();
        if(value.toInt() > 8191 && value.toInt() < 65537)
        {
            if(_preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE) != value.toInt())
            {
                _preferences->putInt(preference_task_size_nuki, value.toInt());
                state.configChanged = true;
            }
        }
    }

    if(request->hasParam("BLEGENTIMEOUT"))
    {
        value = request->getParam("BLEGENTIMEOUT")->value();
        if(value.toInt() > 2999 && value.toInt() < 65537)
        {
            if(_preferences->getInt(preference_ble_general_timeout, 10000) != value.toInt())
            {
                _preferences->putInt(preference_ble_general_timeout, value.toInt());
            }
        }
    }

    if(request->hasParam("BLECMDTIMEOUT"))
    {
        value = request->getParam("BLECMDTIMEOUT")->value();
        if(value.toInt() > 2999 && value.toInt() < 65537)
        {
            if(_preferences->getInt(preference_ble_command_timeout, 3000) != value.toInt())
            {
                _preferences->putInt(preference_ble_command_timeout, value.toInt());
            }
        }
    }

    if(request->hasParam("ALMAX"))
    {
        value = request->getParam("ALMAX")->value();
        if(value.toInt() > 0 && value.toInt() < 101)
        {
            if(_preferences->getInt(preference_authlog_max_entries, MAX_AUTHLOG) != value.toInt())
            {
                _preferences->putInt(preference_authlog_max_entries, value.toInt());
            }
        }
    }

    if(request->hasParam("KPMAX"))
    {
        value = request->getParam("KPMAX")->value();
        if(value.toInt() > 0 && value.toInt() < 201)
        {
            if(_preferences->getInt(preference_keypad_max_entries, MAX_KEYPAD) != value.toInt())
            {
                _preferences->putInt(preference_keypad_max_entries, value.toInt());
            }
        }
    }

    if(request->hasParam("TCMAX"))
    {
        value = request->getParam("TCMAX")->value();
        if(value.toInt() > 0 && value.toInt() < 101)
        {
            if(_preferences->getInt(preference_timecontrol_max_entries, MAX_TIMECONTROL) != value.toInt())
            {
                _preferences->putInt(preference_timecontrol_max_entries, value.toInt());
            }
        }
    }

    if(request->hasParam("AUTHMAX"))
    {
        value = request->getParam("AUTHMAX")->value();
        if(value.toInt() > 0 && value.toInt() < 101)
        {
            if(_preferences->getInt(preference_auth_max_entries, MAX_AUTH) != value.toInt())
            {
                _preferences->putInt(preference_auth_max_entries, value.toInt());
            }
        }
    }

    if(request->hasParam("BUFFSIZE"))
    {
        value = request->getParam("BUFFSIZE")->value();
        if(value.toInt() > 4095 && value.toInt() < 65537)
        {
            if(_preferences->getInt(preference_buffer_size, CHAR_BUFFER_SIZE) != value.toInt())
            {
                _preferences->putInt(preference_buffer_size, value.toInt());
                state.configChanged = true;
            }
        }
    }

    if(request->hasParam("BTLPRST"))
    {
        bool v = isParameterTrue(request, "BTLPRST");
        if(_preferences->getBool(preference_enable_bootloop_reset, false) != v)
        {
            _preferences->putBool(preference_enable_bootloop_reset, v);

        }
    }

    if(request->hasParam("DISNTWNOCON"))
    {
        bool v = isParameterTrue(request, "DISNTWNOCON");
        if(_preferences->getBool(preference_disable_network_not_connected, false) != v)
        {
            _preferences->putBool(preference_disable_network_not_connected, v);

            state.restartServicesReconnect = true;
        }
    }

    if(request->hasParam("OTAUPD"))
    {
        value = request->getParam("OTAUPD")->value();
        if(_preferences->getString(preference_ota_updater_url, "") != value)
        {
            _preferences->putString(preference_ota_updater_url, value);

            state.configChanged = true;
        }
    }

    if(request->hasParam("OTAMAIN"))
    {
        value = request->getParam("OTAMAIN")->value();
        if(_preferences->getString(preference_ota_main_url, "") != value)
        {
            _preferences->putString(preference_ota_main_url, value);

            state.configChanged = true;
        }
    }

    if(request->hasParam("SHOWSECRETS"))
    {
        bool v = isParameterTrue(request, "SHOWSECRETS");
        if(_preferences->getBool(preference_show_secrets, false) != v)
        {
            _preferences->putBool(preference_show_secrets, v);
        }
    }

    if(request->hasParam("DBGCONN"))
    {
        bool v = isParameterTrue(request, "DBGCONN");
        if(_preferences->getBool(preference_debug_connect, false) != v)
        {
            _preferences->putBool(preference_debug_connect, v);

            state.restartServicesNoReconnect = true;
        }
    }

    if(request->hasParam("DBGCOMMU"))
    {
        bool v = isParameterTrue(request, "DBGCOMMU");
        if(_preferences->getBool(preference_debug_communication, false) != v)
        {
            _preferences->putBool(preference_debug_communication, v);

            state.restartServicesNoReconnect = true;
        }
    }

    if(request->hasParam("DBGHEAP"))
    {
        bool v = isParameterTrue(request, "DBGHEAP");
        if(_preferences->getBool(preference_publish_debug_info, false) != v)
        {
            _preferences->putBool(preference_publish_debug_info, v);

            state.restartServicesNoReconnect = true;
        }
    }

    if(request->hasParam("DBGREAD"))
    {
        bool v = isParameterTrue(request, "DBGREAD");
        if(_preferences->getBool(preference_debug_readable_data, false) != v)
        {
            _preferences->putBool(preference_debug_readable_data, v);

            state.restartServicesNoReconnect = true;
        }
    }

    if(request->hasParam("DBGHEX"))
    {
        bool v = isParameterTrue(request, "DBGHEX");
        if(_preferences->getBool(preference_debug_hex_data, false) != v)
        {
            _preferences->putBool(preference_debug_hex_data, v);

            state.restartServicesNoReconnect = true;
        }
    }

    if(request->hasParam("DBGCOMM"))
    {
        bool v = isParameterTrue(request, "DBGCOMM");
        if(_preferences->getBool(preference_debug_command, false) != v)
        {
            _preferences->putBool(preference_debug_command, v);

            state.restartServicesNoReconnect = true;
        }
    }

    if(request->hasParam("LCKFORCEID"))
    {
        bool v = isParameterTrue(request, "LCKFORCEID");
        _preferences->putBool(preference_lock_force_id, v);
    }

    if(request->hasParam("LCKFORCEKP"))
    {
        bool v = isParameterTrue(request, "LCKFORCEKP");
        _preferences->putBool(preference_lock_force_keypad, v);
    }

    if(request->hasParam("LCKFORCEDS"))
    {
        bool v = isParameterTrue(request, "LCKFORCEDS");
        _preferences->putBool(preference_lock_force_doorsensor, v);
    }

    if(request->hasParam("OPFORCEID"))
    {
        bool v = isParameterTrue(request, "OPFORCEID");
        _preferences->putBool(preference_opener_force_id, v);
    }

    if(request->hasParam("OPFORCEKP"))
    {
        bool v = isParameterTrue(request, "OPFORCEKP");
        _preferences->putBool(preference_opener_force_keypad, v);
    }

    if(request->hasParam( "LCKMANPAIR") && (value == "1"))
    {
        manPairLck = true;
    }

    if(request->hasParam("OPNMANPAIR") && (value == "1"))
    {
        manPairOpn = true;
    }

    if(request->hasParam("LCKBLEADDR"))
    {
        value = request->getParam("LCKBLEADDR")->value();
        if(value.length() == 12) for(int i=0; i<value.length(); i+=2)
            {
                currentBleAddress[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
            }
    }

    if(request->hasParam("LCKSECRETK"))
    {
        value = request->getParam("LCKSECRETK")->value();
        if(value.length() == 64) for(int i=0; i<value.length(); i+=2)
        {
            secretKeyK[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
        }
    }

    if(request->hasParam("LCKAUTHID"))
    {
        value = request->getParam("LCKAUTHID")->value();
        if(value.length() == 8) for(int i=0; i<value.length(); i+=2)
        {
            authorizationId[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
        }
    }
    else if(request->hasParam("LCKISULTRA") && (value == "1"))
    {
        isUltra = true;
    }

    if(request->hasParam("OPNBLEADDR"))
    {
        value = request->getParam("OPNBLEADDR")->value();
        if(value.length() == 12) for(int i=0; i<value.length(); i+=2)
        {
            currentBleAddressOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
        }
    }

    if(request->hasParam("OPNSECRETK"))
    {
        value = request->getParam("OPNSECRETK")->value();
        if(value.length() == 64) for(int i=0; i<value.length(); i+=2)
        {
            secretKeyKOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
        }
    }

    if(request->hasParam("OPNAUTHID"))
    {
        value = request->getParam("OPNAUTHID")->value();
        if(value.length() == 8) for(int i=0; i<value.length(); i+=2)
        {
            authorizationIdOpn[(i/2)] = std::stoi(value.substring(i, i+2).c_str(), nullptr, 16);
        }
    }

    if(manPairLck)
    {
        Log->println("Changing lock pairing");
        Preferences nukiBlePref;
        nukiBlePref.begin("NukiHub", false);
        nukiBlePref.putBytes("bleAddress", currentBleAddress, 6);
        nukiBlePref.putBytes("secretKeyK", secretKeyK, 32);
        nukiBlePref.putBytes("authorizationId", authorizationId, 4);
        nukiBlePref.putBytes("securityPinCode", pincode, 2);
        nukiBlePref.putBytes("ultraPinCode", ultraPincode, 4);
        nukiBlePref.putBool("isUltra", isUltra);

        nukiBlePref.end();
        Log->print("Setting changed: ");
        Log->println("Lock pairing data");
        state.restartServicesNoReconnect = true;
    }

    if(manPairOpn)
    {
        Log->println("Changing opener pairing");
        Preferences nukiBlePref;
        nukiBlePref.begin("NukiHubopener", false);
        nukiBlePref.putBytes("bleAddress", currentBleAddressOpn, 6);
        nukiBlePref.putBytes("secretKeyK", secretKeyKOpn, 32);
        nukiBlePref.putBytes("authorizationId", authorizationIdOpn, 4);
        nukiBlePref.putBytes("securityPinCode", pincode, 2);
        nukiBlePref.end();
        Log->print("Setting changed: ");
        Log->println("Opener pairing data");
        state.restartServicesNoReconnect = true;
    }
}

#endif
