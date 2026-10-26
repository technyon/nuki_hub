#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include "FS.h"
#include "SPIFFS.h"

#ifndef NUKI_HUB_UPDATER
static const Setting advancedSettings[] =
{
    {"WEBLOG", "Enable WebSerial logging", preference_webserial_enabled, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::RestartNoReconnect},
    {"FRCHSTUPD", "Force slave Hosted update on next boot", preference_force_hosted_update, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::ConfigChanged},
    {"TSKNTWK", "Task size Network (min 12288, max 65536)", preference_task_size_network, SettingType::Int, NETWORK_TASK_SIZE, 12288, 65536, 6, "", SettingEffect::ConfigChanged},
    {"TSKNUKI", "Task size Nuki (min 8192, max 65536)", preference_task_size_nuki, SettingType::Int, NUKI_TASK_SIZE, 8192, 65536, 6, "", SettingEffect::ConfigChanged},
    {"BLEGENTIMEOUT", "BLE General timeout in ms (min 10000, max 65536)", preference_ble_general_timeout, SettingType::Int, 10000, 3000, 65536, 6, "", SettingEffect::None},
    {"BLECMDTIMEOUT", "BLE Command timeout in ms (min 3000, max 65536)", preference_ble_command_timeout, SettingType::Int, 3000, 3000, 65536, 6, "", SettingEffect::None},
    {"ALMAX", "Max auth log entries (min 1, max 100)", preference_authlog_max_entries, SettingType::Int, MAX_AUTHLOG, 1, 100, 3, "id=\"inputmaxauthlog\"", SettingEffect::None},
    {"KPMAX", "Max keypad entries (min 1, max 200)", preference_keypad_max_entries, SettingType::Int, MAX_KEYPAD, 1, 200, 3, "id=\"inputmaxkeypad\"", SettingEffect::None},
    {"TCMAX", "Max timecontrol entries (min 1, max 100)", preference_timecontrol_max_entries, SettingType::Int, MAX_TIMECONTROL, 1, 100, 3, "id=\"inputmaxtimecontrol\"", SettingEffect::None},
    {"AUTHMAX", "Max authorization entries (min 1, max 100)", preference_auth_max_entries, SettingType::Int, MAX_AUTH, 1, 100, 3, "id=\"inputmaxauth\"", SettingEffect::None},
    {"BUFFSIZE", "Char buffer size (min 4096, max 65536)", preference_buffer_size, SettingType::Int, CHAR_BUFFER_SIZE, 4096, 65536, 6, "", SettingEffect::ConfigChanged},
    {"BTLPRST", "Enable Bootloop prevention (Try to reset these settings to default on bootloop)", preference_enable_bootloop_reset, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"DISNTWNOCON", "Disable Network if not connected within 60s", preference_disable_network_not_connected, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::RestartReconnect},
    {"OTAUPD", "Custom URL to update Nuki Hub updater", preference_ota_updater_url, SettingType::String, 0, 0, 0, 255, "", SettingEffect::ConfigChanged},
    {"OTAMAIN", "Custom URL to update Nuki Hub", preference_ota_main_url, SettingType::String, 0, 0, 0, 255, "", SettingEffect::ConfigChanged},
    {"SHOWSECRETS", "Show Pairing secrets on Info page", preference_show_secrets, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"DBGCONN", "Enable Nuki connect debug logging", preference_debug_connect, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::RestartNoReconnect},
    {"DBGCOMMU", "Enable Nuki communication debug logging", preference_debug_communication, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::RestartNoReconnect},
    {"DBGHEAP", "Publish free heap over MQTT", preference_publish_debug_info, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::RestartNoReconnect},
    {"DBGREAD", "Enable Nuki readable data debug logging", preference_debug_readable_data, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::RestartNoReconnect},
    {"DBGHEX", "Enable Nuki hex data debug logging", preference_debug_hex_data, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::RestartNoReconnect},
    {"DBGCOMM", "Enable Nuki command debug logging", preference_debug_command, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::RestartNoReconnect},
    {"LCKFORCEID", "", preference_lock_force_id, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"LCKFORCEKP", "Force Lock Keypad connected", preference_lock_force_keypad, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"LCKFORCEDS", "Force Lock Doorsensor connected", preference_lock_force_doorsensor, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"OPFORCEID", "", preference_opener_force_id, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"OPFORCEKP", "Force Opener Keypad", preference_opener_force_keypad, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
};

static const size_t advancedSettingsCount = sizeof(advancedSettings) / sizeof(advancedSettings[0]);

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
    printSetting(&response, advancedSettings, advancedSettingsCount, "DISNTWNOCON");
    printSetting(&response, advancedSettings, advancedSettingsCount, "WEBLOG");
    printSetting(&response, advancedSettings, advancedSettingsCount, "FRCHSTUPD");
    printSetting(&response, advancedSettings, advancedSettingsCount, "BTLPRST");
    printSetting(&response, advancedSettings, advancedSettingsCount, "BUFFSIZE");
    printSetting(&response, advancedSettings, advancedSettingsCount, "TSKNTWK");
    response.print("<tr><td>Measured peak network task stack usage since boot</td><td>");
    response.print(_preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE) - uxTaskGetStackHighWaterMark(networkTaskHandle));
    response.print("</td></tr>");
    printSetting(&response, advancedSettings, advancedSettingsCount, "TSKNUKI");
    response.print("<tr><td>Measured peak Nuki task stack usage since boot</td><td>");
    response.print(_preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE) - uxTaskGetStackHighWaterMark(nukiTaskHandle));
    response.print("</td></tr>");
    printSetting(&response, advancedSettings, advancedSettingsCount, "BLEGENTIMEOUT");
    printSetting(&response, advancedSettings, advancedSettingsCount, "BLECMDTIMEOUT");
    printSetting(&response, advancedSettings, advancedSettingsCount, "ALMAX");
    printSetting(&response, advancedSettings, advancedSettingsCount, "KPMAX");
    printSetting(&response, advancedSettings, advancedSettingsCount, "TCMAX");
    printSetting(&response, advancedSettings, advancedSettingsCount, "AUTHMAX");
    printSetting(&response, advancedSettings, advancedSettingsCount, "SHOWSECRETS");
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
    printSetting(&response, advancedSettings, advancedSettingsCount, "OTAUPD");
    printSetting(&response, advancedSettings, advancedSettingsCount, "OTAMAIN");

    if(_nuki != nullptr)
    {
        char uidString[20];
        itoa(_preferences->getUInt(preference_nuki_id_lock, 0), uidString, 16);
        printCheckBox(&response, "LCKFORCEID", ((String)"Force Lock ID to current ID (" + uidString + ")").c_str(), _preferences->getBool(preference_lock_force_id, false), "");
        printSetting(&response, advancedSettings, advancedSettingsCount, "LCKFORCEKP");
        printSetting(&response, advancedSettings, advancedSettingsCount, "LCKFORCEDS");
    }

    if(_nukiOpener != nullptr)
    {
        char uidString[20];
        itoa(_preferences->getUInt(preference_nuki_id_opener, 0), uidString, 16);
        printCheckBox(&response, "OPFORCEID", ((String)"Force Opener ID to current ID (" + uidString + ")").c_str(), _preferences->getBool(preference_opener_force_id, false), "");
        printSetting(&response, advancedSettings, advancedSettingsCount, "OPFORCEKP");
    }

    printSetting(&response, advancedSettings, advancedSettingsCount, "DBGCONN");
    printSetting(&response, advancedSettings, advancedSettingsCount, "DBGCOMMU");
    printSetting(&response, advancedSettings, advancedSettingsCount, "DBGREAD");
    printSetting(&response, advancedSettings, advancedSettingsCount, "DBGHEX");
    printSetting(&response, advancedSettings, advancedSettingsCount, "DBGCOMM");
    printSetting(&response, advancedSettings, advancedSettingsCount, "DBGHEAP");
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

    applySettings(request, advancedSettings, advancedSettingsCount, state);

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
