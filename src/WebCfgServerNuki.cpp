#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"

#ifndef NUKI_HUB_UPDATER
static const Setting nukiSettings[] =
{
    {"LSTINT", "Query interval lock state (seconds)", preference_query_interval_lockstate, SettingType::Int, 1800, 0, 0, 10, "", SettingEffect::None},
    {"CFGINT", "Query interval configuration (seconds)", preference_query_interval_configuration, SettingType::Int, 3600, 0, 0, 10, "", SettingEffect::None},
    {"BATINT", "Query interval battery (seconds)", preference_query_interval_battery, SettingType::Int, 1800, 0, 0, 10, "", SettingEffect::None},
    {"KPINT", "Query interval keypad (seconds)", preference_query_interval_keypad, SettingType::Int, 1800, 0, 0, 10, "", SettingEffect::None},
    {"NRTRY", "Number of retries if command failed", preference_command_nr_of_retries, SettingType::Int, 3, 0, 0, 10, "", SettingEffect::None},
    {"TRYDLY", "Delay between retries (milliseconds)", preference_command_retry_delay, SettingType::Int, 100, 0, 0, 10, "", SettingEffect::None},
    {"RSBC", "Restart if bluetooth beacons not received (seconds; -1 to disable)", preference_restart_ble_beacon_lost, SettingType::Int, 60, 0, 0, 10, "", SettingEffect::None},
    {"CONFPUB", "Publish Nuki configuration information", preference_conf_info_enabled, SettingType::Bool, 1, 0, 0, 0, "", SettingEffect::None},
    {"KPPUB", "Publish keypad entries information", preference_keypad_info_enabled, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"KPCODE", "Also publish keypad codes (<span class=\"warning\">Disadvised for security reasons</span>)", preference_keypad_publish_code, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"KPCHECK", "Allow checking if keypad codes are valid (<span class=\"warning\">Disadvised for security reasons</span>)", preference_keypad_check_code_enabled, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"KPENA", "Add, modify and delete keypad codes", preference_keypad_control_enabled, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::RestartReconnect},
    {"TCPUB", "Publish time control entries information", preference_timecontrol_info_enabled, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"AUTHPUB", "Publish authorization entries information", preference_auth_info_enabled, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"KPPER", "Publish a topic per keypad entry and create HA sensor", preference_keypad_topic_per_entry, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"KPHAID", "Identify the HA keypad entities by code id instead of list position", preference_keypad_ha_by_code_id, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"TCPER", "Publish a topic per time control entry and create HA sensor", preference_timecontrol_topic_per_entry, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"TCENA", "Add, modify and delete time control entries", preference_timecontrol_control_enabled, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::RestartReconnect},
    {"AUTHPER", "Publish a topic per authorization entry and create HA sensor", preference_auth_topic_per_entry, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"AUTHHAID", "Identify the HA authorization entities by authorization id instead of list position", preference_auth_ha_by_auth_id, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"AUTHENA", "Modify and delete authorization entries", preference_auth_control_enabled, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::RestartReconnect},
    {"PUBAUTH", "Publish authorization log", preference_publish_authdata, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"SAVELOGNUM", "Save auth log number to flash", preference_save_log_num, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"REGAPP", "Lock: Nuki Bridge is running alongside Nuki Hub (needs re-pairing if changed)", preference_register_as_app, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"REGAPPOPN", "Opener: Nuki Bridge is running alongside Nuki Hub (needs re-pairing if changed)", preference_register_opener_as_app, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"LOCKENA", "Nuki Lock enabled", preference_lock_enabled, SettingType::Bool, 1, 0, 0, 0, "", SettingEffect::RestartReconnect},
    {"OPENA", "Nuki Opener enabled", preference_opener_enabled, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::RestartReconnect},
    {"ALMAX", "Max auth log entries (min 1, max 100)", preference_authlog_max_entries, SettingType::Int, MAX_AUTHLOG, 1, 100, 3, "id=\"inputmaxauthlog\"", SettingEffect::None},
    {"KPMAX", "Max keypad entries (min 1, max 200)", preference_keypad_max_entries, SettingType::Int, MAX_KEYPAD, 1, 200, 3, "id=\"inputmaxkeypad\"", SettingEffect::None},
    {"TCMAX", "Max timecontrol entries (min 1, max 100)", preference_timecontrol_max_entries, SettingType::Int, MAX_TIMECONTROL, 1, 100, 3, "id=\"inputmaxtimecontrol\"", SettingEffect::None},
    {"AUTHMAX", "Max authorization entries (min 1, max 100)", preference_auth_max_entries, SettingType::Int, MAX_AUTH, 1, 100, 3, "id=\"inputmaxauth\"", SettingEffect::None},
    {"LCKFORCEID", "", preference_lock_force_id, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"LCKFORCEKP", "Force Lock Keypad connected", preference_lock_force_keypad, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"LCKFORCEDS", "Force Lock Doorsensor connected", preference_lock_force_doorsensor, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"OPFORCEID", "", preference_opener_force_id, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
    {"OPFORCEKP", "Force Opener Keypad", preference_opener_force_keypad, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::None},
};

static const size_t nukiSettingsCount = sizeof(nukiSettings) / sizeof(nukiSettings[0]);

esp_err_t WebCfgServer::buildNukiConfigHtml(PsychicRequest *request, PsychicResponse* resp)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    bool hasKeypad = (_nuki != nullptr && _nuki->hasKeypad()) || (_nukiOpener != nullptr && _nukiOpener->hasKeypad());

    response.print("<h3>Devices and pairing</h3>");
    response.print("<table>");
    printSetting(&response, nukiSettings, nukiSettingsCount, "LOCKENA");
    printCheckBox(&response, "GEMINIENA", "Nuki Smartlock Ultra/Go/5th gen enabled", _preferences->getBool(preference_lock_gemini_enabled, false), "");
    printSetting(&response, nukiSettings, nukiSettingsCount, "OPENA");
    if(_preferences->getBool(preference_lock_enabled, true) && !_preferences->getBool(preference_lock_gemini_enabled, false))
    {
        printSetting(&response, nukiSettings, nukiSettingsCount, "REGAPP");
    }
    if(_preferences->getBool(preference_opener_enabled, false))
    {
        printSetting(&response, nukiSettings, nukiSettingsCount, "REGAPPOPN");
    }
    if(_nuki != nullptr)
    {
        char uidString[20];
        itoa(_preferences->getUInt(preference_nuki_id_lock, 0), uidString, 16);
        printCheckBox(&response, "LCKFORCEID", ((String)"Force Lock ID to current ID (" + uidString + ")").c_str(), _preferences->getBool(preference_lock_force_id, false), "");
        printSetting(&response, nukiSettings, nukiSettingsCount, "LCKFORCEKP");
        printSetting(&response, nukiSettings, nukiSettingsCount, "LCKFORCEDS");
    }
    if(_nukiOpener != nullptr)
    {
        char uidString[20];
        itoa(_preferences->getUInt(preference_nuki_id_opener, 0), uidString, 16);
        printCheckBox(&response, "OPFORCEID", ((String)"Force Opener ID to current ID (" + uidString + ")").c_str(), _preferences->getBool(preference_opener_force_id, false), "");
        printSetting(&response, nukiSettings, nukiSettingsCount, "OPFORCEKP");
    }
    response.print("</table>");

    response.print("<h3>Polling and Bluetooth</h3>");
    response.print("<table>");
    printSetting(&response, nukiSettings, nukiSettingsCount, "LSTINT");
    printSetting(&response, nukiSettings, nukiSettingsCount, "CFGINT");
    printSetting(&response, nukiSettings, nukiSettingsCount, "BATINT");
    if(hasKeypad)
    {
        printSetting(&response, nukiSettings, nukiSettingsCount, "KPINT");
    }
    printSetting(&response, nukiSettings, nukiSettingsCount, "NRTRY");
    printSetting(&response, nukiSettings, nukiSettingsCount, "TRYDLY");
    printSetting(&response, nukiSettings, nukiSettingsCount, "RSBC");
#if defined(CONFIG_IDF_TARGET_ESP32)
    printInputField(&response, "TXPWR", "BLE transmit power in dB (minimum -12, maximum 9)", _preferences->getInt(preference_ble_tx_power, 9), 10, "");
#else
    printInputField(&response, "TXPWR", "BLE transmit power in dB (minimum -12, maximum 20)", _preferences->getInt(preference_ble_tx_power, 9), 10, "");
#endif
    response.print("</table>");

    response.print("<h3>Time</h3>");
    response.print("<table>");
    printCheckBox(&response, "UPTIME", "Update Nuki Hub and Lock/Opener time using NTP", _preferences->getBool(preference_update_time, false), "");
    printInputField(&response, "TIMESRV", "NTP server", _preferences->getString(preference_time_server, "pool.ntp.org").c_str(), 255, "");
    response.print("</table>");

    response.print("<h3>Nuki configuration</h3>");
    response.print("<table>");
    printSetting(&response, nukiSettings, nukiSettingsCount, "CONFPUB");
    response.print("</table>");

    if(hasKeypad)
    {
        response.print("<h3>Keypad</h3>");
        response.print("<table>");
        printSetting(&response, nukiSettings, nukiSettingsCount, "KPPUB");
        printSetting(&response, nukiSettings, nukiSettingsCount, "KPENA");
        printSetting(&response, nukiSettings, nukiSettingsCount, "KPCODE");
        printSetting(&response, nukiSettings, nukiSettingsCount, "KPCHECK");
        printSetting(&response, nukiSettings, nukiSettingsCount, "KPPER");
        printSetting(&response, nukiSettings, nukiSettingsCount, "KPHAID");
        printSetting(&response, nukiSettings, nukiSettingsCount, "KPMAX");
        response.print("</table>");
    }

    response.print("<h3>Time control</h3>");
    response.print("<table>");
    printSetting(&response, nukiSettings, nukiSettingsCount, "TCPUB");
    printSetting(&response, nukiSettings, nukiSettingsCount, "TCENA");
    printSetting(&response, nukiSettings, nukiSettingsCount, "TCPER");
    printSetting(&response, nukiSettings, nukiSettingsCount, "TCMAX");
    response.print("</table>");

    response.print("<h3>Authorizations</h3>");
    response.print("<table>");
    printSetting(&response, nukiSettings, nukiSettingsCount, "AUTHPUB");
    printSetting(&response, nukiSettings, nukiSettingsCount, "AUTHENA");
    printSetting(&response, nukiSettings, nukiSettingsCount, "AUTHPER");
    printSetting(&response, nukiSettings, nukiSettingsCount, "AUTHHAID");
    printSetting(&response, nukiSettings, nukiSettingsCount, "AUTHMAX");
    response.print("</table>");

    response.print("<h3>Authorization log</h3>");
    response.print("<table>");
    printSetting(&response, nukiSettings, nukiSettingsCount, "PUBAUTH");
    printSetting(&response, nukiSettings, nukiSettingsCount, "SAVELOGNUM");
    printSetting(&response, nukiSettings, nukiSettingsCount, "ALMAX");
    response.print("</table>");
    response.print("<div class=\"actions\"><input type=\"submit\" name=\"submit\" value=\"Save\"></div>");
    response.print("</form>");
    buildHtmlFooter(&response);
    return response.endSend();
}

esp_err_t WebCfgServer::buildAccLvlHtml(PsychicRequest *request, PsychicResponse* resp)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);

    uint32_t aclPrefs[17];
    _preferences->getBytes(preference_acl, &aclPrefs, sizeof(aclPrefs));

    response.print("<form method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response.print("<input type=\"hidden\" name=\"ACLLVLCHANGED\" value=\"1\">");

    if(_nuki != nullptr)
    {
        uint32_t basicLockConfigAclPrefs[16];
        _preferences->getBytes(preference_conf_lock_basic_acl, &basicLockConfigAclPrefs, sizeof(basicLockConfigAclPrefs));
        uint32_t advancedLockConfigAclPrefs[26];
        _preferences->getBytes(preference_conf_lock_advanced_acl, &advancedLockConfigAclPrefs, sizeof(advancedLockConfigAclPrefs));

        response.print("<h3>Nuki Lock Access Control</h3>");
        response.print("<div class=\"actions\"><button type=\"button\" data-check=\"chk_access_lock\" data-value=\"1\">Allow all</button><button type=\"button\" data-check=\"chk_access_lock\" data-value=\"0\">Disallow all</button></div>");
        response.print("<table><tr><th>Action</th><th>Allowed</th></tr>");

        printCheckBox(&response, "ACLLCKLCK", "Lock", ((int)aclPrefs[0] == 1), "chk_access_lock");
        printCheckBox(&response, "ACLLCKUNLCK", "Unlock", ((int)aclPrefs[1] == 1), "chk_access_lock");
        printCheckBox(&response, "ACLLCKUNLTCH", "Unlatch", ((int)aclPrefs[2] == 1), "chk_access_lock");
        printCheckBox(&response, "ACLLCKLNG", "Lock N Go", ((int)aclPrefs[3] == 1), "chk_access_lock");
        printCheckBox(&response, "ACLLCKLNGU", "Lock N Go Unlatch", ((int)aclPrefs[4] == 1), "chk_access_lock");
        printCheckBox(&response, "ACLLCKFLLCK", "Full Lock", ((int)aclPrefs[5] == 1), "chk_access_lock");
        printCheckBox(&response, "ACLLCKFOB1", "Fob Action 1", ((int)aclPrefs[6] == 1), "chk_access_lock");
        printCheckBox(&response, "ACLLCKFOB2", "Fob Action 2", ((int)aclPrefs[7] == 1), "chk_access_lock");
        printCheckBox(&response, "ACLLCKFOB3", "Fob Action 3", ((int)aclPrefs[8] == 1), "chk_access_lock");
        response.print("</table>");

        response.print("<h3>Nuki Lock Config Control (Requires PIN to be set)</h3>");
        response.print("<div class=\"actions\"><button type=\"button\" data-check=\"chk_config_lock\" data-value=\"1\">Allow all</button><button type=\"button\" data-check=\"chk_config_lock\" data-value=\"0\">Disallow all</button></div>");
        response.print("<table><tr><th>Change</th><th>Allowed</th></tr>");

        printCheckBox(&response, "CONFLCKNAME", "Name", ((int)basicLockConfigAclPrefs[0] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKLAT", "Latitude", ((int)basicLockConfigAclPrefs[1] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKLONG", "Longitude", ((int)basicLockConfigAclPrefs[2] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKAUNL", "Auto unlatch", ((int)basicLockConfigAclPrefs[3] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKPRENA", "Pairing enabled", ((int)basicLockConfigAclPrefs[4] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKBTENA", "Button enabled", ((int)basicLockConfigAclPrefs[5] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKLEDENA", "LED flash enabled", ((int)basicLockConfigAclPrefs[6] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKLEDBR", "LED brightness", ((int)basicLockConfigAclPrefs[7] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKTZOFF", "Timezone offset", ((int)basicLockConfigAclPrefs[8] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKDSTM", "DST mode", ((int)basicLockConfigAclPrefs[9] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKFOB1", "Fob Action 1", ((int)basicLockConfigAclPrefs[10] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKFOB2", "Fob Action 2", ((int)basicLockConfigAclPrefs[11] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKFOB3", "Fob Action 3", ((int)basicLockConfigAclPrefs[12] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKSGLLCK", "Single Lock", ((int)basicLockConfigAclPrefs[13] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKADVM", "Advertising Mode", ((int)basicLockConfigAclPrefs[14] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKTZID", "Timezone ID", ((int)basicLockConfigAclPrefs[15] == 1), "chk_config_lock");

        printCheckBox(&response, "CONFLCKUPOD", "Unlocked Position Offset Degrees", ((int)advancedLockConfigAclPrefs[0] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKLPOD", "Locked Position Offset Degrees", ((int)advancedLockConfigAclPrefs[1] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKSLPOD", "Single Locked Position Offset Degrees", ((int)advancedLockConfigAclPrefs[2] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKUTLTOD", "Unlocked To Locked Transition Offset Degrees", ((int)advancedLockConfigAclPrefs[3] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKLNGT", "Lock n Go timeout", ((int)advancedLockConfigAclPrefs[4] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKSBPA", "Single button press action", ((int)advancedLockConfigAclPrefs[5] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKDBPA", "Double button press action", ((int)advancedLockConfigAclPrefs[6] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKDC", "Detached cylinder", ((int)advancedLockConfigAclPrefs[7] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKBATT", "Battery type", ((int)advancedLockConfigAclPrefs[8] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKABTD", "Automatic battery type detection", ((int)advancedLockConfigAclPrefs[9] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKUNLD", "Unlatch duration", ((int)advancedLockConfigAclPrefs[10] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKALT", "Auto lock timeout", ((int)advancedLockConfigAclPrefs[11] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKAUNLD", "Auto unlock disabled", ((int)advancedLockConfigAclPrefs[12] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKNMENA", "Nightmode enabled", ((int)advancedLockConfigAclPrefs[13] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKNMST", "Nightmode start time", ((int)advancedLockConfigAclPrefs[14] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKNMET", "Nightmode end time", ((int)advancedLockConfigAclPrefs[15] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKNMALENA", "Nightmode auto lock enabled", ((int)advancedLockConfigAclPrefs[16] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKNMAULD", "Nightmode auto unlock disabled", ((int)advancedLockConfigAclPrefs[17] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKNMLOS", "Nightmode immediate lock on start", ((int)advancedLockConfigAclPrefs[18] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKALENA", "Auto lock enabled", ((int)advancedLockConfigAclPrefs[19] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKIALENA", "Immediate auto lock enabled", ((int)advancedLockConfigAclPrefs[20] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKAUENA", "Auto update enabled", ((int)advancedLockConfigAclPrefs[21] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKRBTNUKI", "Reboot Nuki", ((int)advancedLockConfigAclPrefs[22] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKMTRSPD", "Motor speed", ((int)advancedLockConfigAclPrefs[23] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKESSDNM", "Enable slow speed during nightmode", ((int)advancedLockConfigAclPrefs[24] == 1), "chk_config_lock");
        printCheckBox(&response, "CONFLCKRCBRTNUKI", "Recalibrate Nuki", ((int)advancedLockConfigAclPrefs[25] == 1), "chk_config_lock");
        response.print("</table>");
        response.print("<div class=\"actions\"><input type=\"submit\" name=\"submit\" value=\"Save\"></div>");
    }
    if(_nukiOpener != nullptr)
    {
        uint32_t basicOpenerConfigAclPrefs[14];
        _preferences->getBytes(preference_conf_opener_basic_acl, &basicOpenerConfigAclPrefs, sizeof(basicOpenerConfigAclPrefs));
        uint32_t advancedOpenerConfigAclPrefs[22];
        _preferences->getBytes(preference_conf_opener_advanced_acl, &advancedOpenerConfigAclPrefs, sizeof(advancedOpenerConfigAclPrefs));

        response.print("<h3>Nuki Opener Access Control</h3>");
        response.print("<div class=\"actions\"><button type=\"button\" data-check=\"chk_access_opener\" data-value=\"1\">Allow all</button><button type=\"button\" data-check=\"chk_access_opener\" data-value=\"0\">Disallow all</button></div>");
        response.print("<table><tr><th>Action</th><th>Allowed</th></tr>");

        printCheckBox(&response, "ACLOPNUNLCK", "Activate Ring-to-Open", ((int)aclPrefs[9] == 1), "chk_access_opener");
        printCheckBox(&response, "ACLOPNLCK", "Deactivate Ring-to-Open", ((int)aclPrefs[10] == 1), "chk_access_opener");
        printCheckBox(&response, "ACLOPNUNLTCH", "Electric Strike Actuation", ((int)aclPrefs[11] == 1), "chk_access_opener");
        printCheckBox(&response, "ACLOPNUNLCKCM", "Activate Continuous Mode", ((int)aclPrefs[12] == 1), "chk_access_opener");
        printCheckBox(&response, "ACLOPNLCKCM", "Deactivate Continuous Mode", ((int)aclPrefs[13] == 1), "chk_access_opener");
        printCheckBox(&response, "ACLOPNFOB1", "Fob Action 1", ((int)aclPrefs[14] == 1), "chk_access_opener");
        printCheckBox(&response, "ACLOPNFOB2", "Fob Action 2", ((int)aclPrefs[15] == 1), "chk_access_opener");
        printCheckBox(&response, "ACLOPNFOB3", "Fob Action 3", ((int)aclPrefs[16] == 1), "chk_access_opener");
        response.print("</table>");

        response.print("<h3>Nuki Opener Config Control (Requires PIN to be set)</h3>");
        response.print("<div class=\"actions\"><button type=\"button\" data-check=\"chk_config_opener\" data-value=\"1\">Allow all</button><button type=\"button\" data-check=\"chk_config_opener\" data-value=\"0\">Disallow all</button></div>");
        response.print("<table><tr><th>Change</th><th>Allowed</th></tr>");

        printCheckBox(&response, "CONFOPNNAME", "Name", ((int)basicOpenerConfigAclPrefs[0] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNLAT", "Latitude", ((int)basicOpenerConfigAclPrefs[1] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNLONG", "Longitude", ((int)basicOpenerConfigAclPrefs[2] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNPRENA", "Pairing enabled", ((int)basicOpenerConfigAclPrefs[3] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNBTENA", "Button enabled", ((int)basicOpenerConfigAclPrefs[4] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNLEDENA", "LED flash enabled", ((int)basicOpenerConfigAclPrefs[5] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNTZOFF", "Timezone offset", ((int)basicOpenerConfigAclPrefs[6] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNDSTM", "DST mode", ((int)basicOpenerConfigAclPrefs[7] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNFOB1", "Fob Action 1", ((int)basicOpenerConfigAclPrefs[8] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNFOB2", "Fob Action 2", ((int)basicOpenerConfigAclPrefs[9] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNFOB3", "Fob Action 3", ((int)basicOpenerConfigAclPrefs[10] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNOPM", "Operating Mode", ((int)basicOpenerConfigAclPrefs[11] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNADVM", "Advertising Mode", ((int)basicOpenerConfigAclPrefs[12] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNTZID", "Timezone ID", ((int)basicOpenerConfigAclPrefs[13] == 1), "chk_config_opener");

        printCheckBox(&response, "CONFOPNICID", "Intercom ID", ((int)advancedOpenerConfigAclPrefs[0] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNBUSMS", "BUS mode Switch", ((int)advancedOpenerConfigAclPrefs[1] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNSCDUR", "Short Circuit Duration", ((int)advancedOpenerConfigAclPrefs[2] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNESD", "Eletric Strike Delay", ((int)advancedOpenerConfigAclPrefs[3] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNRESD", "Random Electric Strike Delay", ((int)advancedOpenerConfigAclPrefs[4] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNESDUR", "Electric Strike Duration", ((int)advancedOpenerConfigAclPrefs[5] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNDRTOAR", "Disable RTO after ring", ((int)advancedOpenerConfigAclPrefs[6] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNRTOT", "RTO timeout", ((int)advancedOpenerConfigAclPrefs[7] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNDRBSUP", "Doorbell suppression", ((int)advancedOpenerConfigAclPrefs[8] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNDRBSUPDUR", "Doorbell suppression duration", ((int)advancedOpenerConfigAclPrefs[9] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNSRING", "Sound Ring", ((int)advancedOpenerConfigAclPrefs[10] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNSOPN", "Sound Open", ((int)advancedOpenerConfigAclPrefs[11] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNSRTO", "Sound RTO", ((int)advancedOpenerConfigAclPrefs[12] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNSCM", "Sound CM", ((int)advancedOpenerConfigAclPrefs[13] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNSCFRM", "Sound confirmation", ((int)advancedOpenerConfigAclPrefs[14] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNSLVL", "Sound level", ((int)advancedOpenerConfigAclPrefs[15] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNSBPA", "Single button press action", ((int)advancedOpenerConfigAclPrefs[16] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNDBPA", "Double button press action", ((int)advancedOpenerConfigAclPrefs[17] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNBATT", "Battery type", ((int)advancedOpenerConfigAclPrefs[18] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNABTD", "Automatic battery type detection", ((int)advancedOpenerConfigAclPrefs[19] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNRBTNUKI", "Reboot Nuki", ((int)advancedOpenerConfigAclPrefs[20] == 1), "chk_config_opener");
        printCheckBox(&response, "CONFOPNRCBRTNUKI", "Recalibrate Nuki", ((int)advancedOpenerConfigAclPrefs[21] == 1), "chk_config_opener");
        response.print("</table>");
        response.print("<div class=\"actions\"><input type=\"submit\" name=\"submit\" value=\"Save\"></div>");
    }
    response.print("</form>");
    buildHtmlFooter(&response);
    return response.endSend();
}

esp_err_t WebCfgServer::processUnpair(PsychicRequest *request, PsychicResponse* resp, bool opener)
{
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

    esp_err_t res = buildConfirmHtml(request, resp, opener ? "Unpairing Nuki Opener." : "Unpairing Nuki Lock.", 3, true);

    if(!opener && _nuki != nullptr)
    {
        _nuki->unpair();
        _preferences->putInt(preference_lock_pin_status, (int)NukiPinState::NotConfigured);
    }
    if(opener && _nukiOpener != nullptr)
    {
        _nukiOpener->unpair();
        _preferences->putInt(preference_opener_pin_status, (int)NukiPinState::NotConfigured);
    }

    _network->disableHASS();
    waitAndProcess(false, 1000);
    _network->setRestartServices(true);
    return res;
}

const String WebCfgServer::pinStateToString(const NukiPinState& value) const
{
    switch(value)
    {
    case NukiPinState::NotSet:
        return String("PIN not set");
    case NukiPinState::Valid:
        return String("PIN valid");
    case NukiPinState::Invalid:
        return String("PIN set but invalid");
    case NukiPinState::NotConfigured:
    default:
        return String("Unknown");
    }
}

void WebCfgServer::processNukiArgs(PsychicRequest *request, ProcessArgsState& state, String& message)
{
    bool aclLvlChanged = false;
    uint32_t aclPrefs[17] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t basicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t basicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t advancedLockConfigAclPrefs[26] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t advancedOpenerConfigAclPrefs[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    String value;

    if(request->hasParam("UPTIME"))
    {
        _preferences->putBool(preference_update_time, isParameterTrue(request, "UPTIME"));
    }

    if(request->hasParam("TIMESRV"))
    {
        value = request->getParam("TIMESRV")->value();
        if(_preferences->getString(preference_time_server, "pool.ntp.org") != value)
        {
            _preferences->putString(preference_time_server, value);

            state.configChanged = true;
        }
    }

    applySettings(request, nukiSettings, nukiSettingsCount, state);

    if(request->hasParam("TXPWR"))
    {
        value = request->getParam("TXPWR")->value();
#if defined(CONFIG_IDF_TARGET_ESP32)
        if(value.toInt() >= -12 && value.toInt() <= 9)
#else
        if(value.toInt() >= -12 && value.toInt() <= 20)
#endif
        {
            if(_preferences->getInt(preference_ble_tx_power, 9) != value.toInt())
            {
                _preferences->putInt(preference_ble_tx_power, value.toInt());
            }
        }
    }

    if(request->hasParam("ACLLVLCHANGED"))
    {
        aclLvlChanged = true;
    }

    if(request->hasParam("CONFNHPUB"))
    {
        bool v = isParameterTrue(request, "CONFNHPUB");
        if(_preferences->getBool(preference_publish_config, false) != v)
        {
            if(_preferences->getBool(preference_config_from_mqtt, false) && _preferences->getInt(preference_buffer_size, CHAR_BUFFER_SIZE) < 8192)
            {
                _preferences->putInt(preference_buffer_size, 8192);
            }
            _preferences->putBool(preference_publish_config, v);

            state.restartServicesReconnect = true;
        }
    }

    if(request->hasParam("CONFNHCTRL"))
    {
        bool v = isParameterTrue(request, "CONFNHCTRL");
        if(_preferences->getBool(preference_config_from_mqtt, false) != v)
        {
            if(_preferences->getBool(preference_config_from_mqtt, false) && _preferences->getInt(preference_buffer_size, CHAR_BUFFER_SIZE) < 8192)
            {
                _preferences->putInt(preference_buffer_size, 8192);
            }
            _preferences->putBool(preference_config_from_mqtt, v);

            state.restartServicesReconnect = true;
        }
    }

    if(request->hasParam("ACLLCKLCK"))
    {
        bool v = isParameterTrue(request, "ACLLCKLCK");
        aclPrefs[0] = v ? 1 : 0;
    }

    if(request->hasParam("ACLLCKUNLCK"))
    {
        bool v = isParameterTrue(request, "ACLLCKUNLCK");
        aclPrefs[1] = v ? 1 : 0;
    }

    if(request->hasParam("ACLLCKUNLTCH"))
    {
        bool v = isParameterTrue(request, "ACLLCKUNLTCH");
        aclPrefs[2] = v ? 1 : 0;
    }

    if(request->hasParam("ACLLCKLNG"))
    {
        bool v = isParameterTrue(request, "ACLLCKLNG");
        aclPrefs[3] = v ? 1 : 0;
    }

    if(request->hasParam("ACLLCKLNGU"))
    {
        bool v = isParameterTrue(request, "ACLLCKLNGU");
        aclPrefs[4] = v ? 1 : 0;
    }

    if(request->hasParam("ACLLCKFLLCK"))
    {
        bool v = isParameterTrue(request, "ACLLCKFLLCK");
        aclPrefs[5] = v ? 1 : 0;
    }

    if(request->hasParam("ACLLCKFOB1"))
    {
        bool v = isParameterTrue(request, "ACLLCKFOB1");
        aclPrefs[6] = v ? 1 : 0;
    }

    if(request->hasParam("ACLLCKFOB2"))
    {
        bool v = isParameterTrue(request, "ACLLCKFOB2");
        aclPrefs[7] = v ? 1 : 0;
    }

    if(request->hasParam("ACLLCKFOB3"))
    {
        bool v = isParameterTrue(request, "ACLLCKFOB3");
        aclPrefs[8] = v ? 1 : 0;
    }

    if(request->hasParam("ACLOPNUNLCK"))
    {
        bool v = isParameterTrue(request, "ACLOPNUNLCK");
        aclPrefs[9] = v ? 1 : 0;
    }

    if(request->hasParam("ACLOPNLCK"))
    {
        bool v = isParameterTrue(request, "ACLOPNLCK");
        aclPrefs[10] = v ? 1 : 0;
    }

    if(request->hasParam("ACLOPNUNLTCH"))
    {
        bool v = isParameterTrue(request, "ACLOPNUNLTCH");
        aclPrefs[11] = v ? 1 : 0;
    }

    if(request->hasParam("ACLOPNUNLCKCM"))
    {
        bool v = isParameterTrue(request, "ACLOPNUNLCKCM");
        aclPrefs[12] = v ? 1 : 0;
    }

    if(request->hasParam("ACLOPNLCKCM"))
    {
        bool v = isParameterTrue(request, "ACLOPNLCKCM");
        aclPrefs[13] = v ? 1 : 0;
    }

    if(request->hasParam("ACLOPNFOB1"))
    {
        bool v = isParameterTrue(request, "ACLOPNFOB1");
        aclPrefs[14] = v ? 1 : 0;
    }

    if(request->hasParam("ACLOPNFOB2"))
    {
        bool v = isParameterTrue(request, "ACLOPNFOB2");
        aclPrefs[15] = v ? 1 : 0;
    }

    if(request->hasParam("ACLOPNFOB3"))
    {
        bool v = isParameterTrue(request, "ACLOPNFOB3");
        aclPrefs[16] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKNAME"))
    {
        bool v = isParameterTrue(request, "CONFLCKNAME");
        basicLockConfigAclPrefs[0] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKLAT"))
    {
        bool v = isParameterTrue(request, "CONFLCKLAT");
        basicLockConfigAclPrefs[1] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKLONG"))
    {
        bool v = isParameterTrue(request, "CONFLCKLONG");
        basicLockConfigAclPrefs[2] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKAUNL"))
    {
        bool v = isParameterTrue(request, "CONFLCKAUNL");
        basicLockConfigAclPrefs[3] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKPRENA"))
    {
        bool v = isParameterTrue(request, "CONFLCKPRENA");
        basicLockConfigAclPrefs[4] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKBTENA"))
    {
        bool v = isParameterTrue(request, "CONFLCKBTENA");
        basicLockConfigAclPrefs[5] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKLEDENA"))
    {
        bool v = isParameterTrue(request, "CONFLCKLEDENA");
        basicLockConfigAclPrefs[6] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKLEDBR"))
    {
        bool v = isParameterTrue(request, "CONFLCKLEDBR");
        basicLockConfigAclPrefs[7] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKTZOFF"))
    {
        bool v = isParameterTrue(request, "CONFLCKTZOFF");
        basicLockConfigAclPrefs[8] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKDSTM"))
    {
        bool v = isParameterTrue(request, "CONFLCKDSTM");
        basicLockConfigAclPrefs[9] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKFOB1"))
    {
        bool v = isParameterTrue(request, "CONFLCKFOB1");
        basicLockConfigAclPrefs[10] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKFOB2"))
    {
        bool v = isParameterTrue(request, "CONFLCKFOB2");
        basicLockConfigAclPrefs[11] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKFOB3"))
    {
        bool v = isParameterTrue(request, "CONFLCKFOB3");
        basicLockConfigAclPrefs[12] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKSGLLCK"))
    {
        bool v = isParameterTrue(request, "CONFLCKSGLLCK");
        basicLockConfigAclPrefs[13] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKADVM"))
    {
        bool v = isParameterTrue(request, "CONFLCKADVM");
        basicLockConfigAclPrefs[14] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKTZID"))
    {
        bool v = isParameterTrue(request, "CONFLCKTZID");
        basicLockConfigAclPrefs[15] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKUPOD"))
    {
        bool v = isParameterTrue(request, "CONFLCKUPOD");
        advancedLockConfigAclPrefs[0] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKLPOD"))
    {
        bool v = isParameterTrue(request, "CONFLCKLPOD");
        advancedLockConfigAclPrefs[1] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKSLPOD"))
    {
        bool v = isParameterTrue(request, "CONFLCKSLPOD");
        advancedLockConfigAclPrefs[2] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKUTLTOD"))
    {
        bool v = isParameterTrue(request, "CONFLCKUTLTOD");
        advancedLockConfigAclPrefs[3] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKLNGT"))
    {
        bool v = isParameterTrue(request, "CONFLCKLNGT");
        advancedLockConfigAclPrefs[4] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKSBPA"))
    {
        bool v = isParameterTrue(request, "CONFLCKSBPA");
        advancedLockConfigAclPrefs[5] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKDBPA"))
    {
        bool v = isParameterTrue(request, "CONFLCKDBPA");
        advancedLockConfigAclPrefs[6] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKDC"))
    {
        bool v = isParameterTrue(request, "CONFLCKDC");
        advancedLockConfigAclPrefs[7] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKBATT"))
    {
        bool v = isParameterTrue(request, "CONFLCKBATT");
        advancedLockConfigAclPrefs[8] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKABTD"))
    {
        bool v = isParameterTrue(request, "CONFLCKABTD");
        advancedLockConfigAclPrefs[9] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKUNLD"))
    {
        bool v = isParameterTrue(request, "CONFLCKUNLD");
        advancedLockConfigAclPrefs[10] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKALT"))
    {
        bool v = isParameterTrue(request, "CONFLCKALT");
        advancedLockConfigAclPrefs[11] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKAUNLD"))
    {
        bool v = isParameterTrue(request, "CONFLCKAUNLD");
        advancedLockConfigAclPrefs[12] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKNMENA"))
    {
        bool v = isParameterTrue(request, "CONFLCKNMENA");
        advancedLockConfigAclPrefs[13] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKNMST"))
    {
        bool v = isParameterTrue(request, "CONFLCKNMST");
        advancedLockConfigAclPrefs[14] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKNMET"))
    {
        bool v = isParameterTrue(request, "CONFLCKNMET");
        advancedLockConfigAclPrefs[15] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKNMALENA"))
    {
        bool v = isParameterTrue(request, "CONFLCKNMALENA");
        advancedLockConfigAclPrefs[16] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKNMAULD"))
    {
        bool v = isParameterTrue(request, "CONFLCKNMAULD");
        advancedLockConfigAclPrefs[17] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKNMLOS"))
    {
        bool v = isParameterTrue(request, "CONFLCKNMLOS");
        advancedLockConfigAclPrefs[18] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKALENA"))
    {
        bool v = isParameterTrue(request, "CONFLCKALENA");
        advancedLockConfigAclPrefs[19] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKIALENA"))
    {
        bool v = isParameterTrue(request, "CONFLCKIALENA");
        advancedLockConfigAclPrefs[20] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKAUENA"))
    {
        bool v = isParameterTrue(request, "CONFLCKAUENA");
        advancedLockConfigAclPrefs[21] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKRBTNUKI"))
    {
        bool v = isParameterTrue(request, "CONFLCKRBTNUKI");
        advancedLockConfigAclPrefs[22] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKMTRSPD"))
    {
        bool v = isParameterTrue(request, "CONFLCKMTRSPD");
        advancedLockConfigAclPrefs[23] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKESSDNM"))
    {
        bool v = isParameterTrue(request, "CONFLCKESSDNM");
        advancedLockConfigAclPrefs[24] = v ? 1 : 0;
    }

    if(request->hasParam("CONFLCKRCBRTNUKI"))
    {
        bool v = isParameterTrue(request, "CONFLCKRCBRTNUKI");
        advancedLockConfigAclPrefs[25] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNNAME"))
    {
        bool v = isParameterTrue(request, "CONFOPNNAME");
        basicOpenerConfigAclPrefs[0] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNLAT"))
    {
        bool v = isParameterTrue(request, "CONFOPNLAT");
        basicOpenerConfigAclPrefs[1] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNLONG"))
    {
        bool v = isParameterTrue(request, "CONFOPNLONG");
        basicOpenerConfigAclPrefs[2] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNPRENA"))
    {
        bool v = isParameterTrue(request, "CONFOPNPRENA");
        basicOpenerConfigAclPrefs[3] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNBTENA"))
    {
        bool v = isParameterTrue(request, "CONFOPNBTENA");
        basicOpenerConfigAclPrefs[4] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNLEDENA"))
    {
        bool v = isParameterTrue(request, "CONFOPNLEDENA");
        basicOpenerConfigAclPrefs[5] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNTZOFF"))
    {
        bool v = isParameterTrue(request, "CONFOPNTZOFF");
        basicOpenerConfigAclPrefs[6] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNDSTM"))
    {
        bool v = isParameterTrue(request, "CONFOPNDSTM");
        basicOpenerConfigAclPrefs[7] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNFOB1"))
    {
        bool v = isParameterTrue(request, "CONFOPNFOB1");
        basicOpenerConfigAclPrefs[8] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNFOB2"))
    {
        bool v = isParameterTrue(request, "CONFOPNFOB2");
        basicOpenerConfigAclPrefs[9] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNFOB3"))
    {
        bool v = isParameterTrue(request, "CONFOPNFOB3");
        basicOpenerConfigAclPrefs[10] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNOPM"))
    {
        bool v = isParameterTrue(request, "CONFOPNOPM");
        basicOpenerConfigAclPrefs[11] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNADVM"))
    {
        bool v = isParameterTrue(request, "CONFOPNADVM");
        basicOpenerConfigAclPrefs[12] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNTZID"))
    {
        bool v = isParameterTrue(request, "CONFOPNTZID");
        basicOpenerConfigAclPrefs[13] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNICID"))
    {
        bool v = isParameterTrue(request, "CONFOPNICID");
        advancedOpenerConfigAclPrefs[0] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNBUSMS"))
    {
        bool v = isParameterTrue(request, "CONFOPNBUSMS");
        advancedOpenerConfigAclPrefs[1] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNSCDUR"))
    {
        bool v = isParameterTrue(request, "CONFOPNSCDUR");
        advancedOpenerConfigAclPrefs[2] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNESD"))
    {
        bool v = isParameterTrue(request, "CONFOPNESD");
        advancedOpenerConfigAclPrefs[3] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNRESD"))
    {
        bool v = isParameterTrue(request, "CONFOPNRESD");
        advancedOpenerConfigAclPrefs[4] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNESDUR"))
    {
        bool v = isParameterTrue(request, "CONFOPNESDUR");
        advancedOpenerConfigAclPrefs[5] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNDRTOAR"))
    {
        bool v = isParameterTrue(request, "CONFOPNDRTOAR");
        advancedOpenerConfigAclPrefs[6] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNRTOT"))
    {
        bool v = isParameterTrue(request, "CONFOPNRTOT");
        advancedOpenerConfigAclPrefs[7] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNDRBSUP"))
    {
        bool v = isParameterTrue(request, "CONFOPNDRBSUP");
        advancedOpenerConfigAclPrefs[8] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNDRBSUPDUR"))
    {
        bool v = isParameterTrue(request, "CONFOPNDRBSUPDUR");
        advancedOpenerConfigAclPrefs[9] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNSRING"))
    {
        bool v = isParameterTrue(request, "CONFOPNSRING");
        advancedOpenerConfigAclPrefs[10] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNSOPN"))
    {
        bool v = isParameterTrue(request, "CONFOPNSOPN");
        advancedOpenerConfigAclPrefs[11] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNSRTO"))
    {
        bool v = isParameterTrue(request, "CONFOPNSRTO");
        advancedOpenerConfigAclPrefs[12] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNSCM"))
    {
        bool v = isParameterTrue(request, "CONFOPNSCM");
        advancedOpenerConfigAclPrefs[13] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNSCFRM"))
    {
        bool v = isParameterTrue(request, "CONFOPNSCFRM");
        advancedOpenerConfigAclPrefs[14] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNSLVL"))
    {
        bool v = isParameterTrue(request, "CONFOPNSLVL");
        advancedOpenerConfigAclPrefs[15] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNSBPA"))
    {
        bool v = isParameterTrue(request, "CONFOPNSBPA");
        advancedOpenerConfigAclPrefs[16] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNDBPA"))
    {
        bool v = isParameterTrue(request, "CONFOPNDBPA");
        advancedOpenerConfigAclPrefs[17] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNBATT"))
    {
        bool v = isParameterTrue(request, "CONFOPNBATT");
        advancedOpenerConfigAclPrefs[18] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNABTD"))
    {
        bool v = isParameterTrue(request, "CONFOPNABTD");
        advancedOpenerConfigAclPrefs[19] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNRBTNUKI"))
    {
        bool v = isParameterTrue(request, "CONFOPNRBTNUKI");
        advancedOpenerConfigAclPrefs[20] = v ? 1 : 0;
    }

    if(request->hasParam("CONFOPNRCBRTNUKI"))
    {
        bool v = isParameterTrue(request, "CONFOPNRCBRTNUKI");
        advancedOpenerConfigAclPrefs[21] = v ? 1 : 0;
    }

    if(request->hasParam("GEMINIENA"))
    {
        bool v = isParameterTrue(request, "GEMINIENA");
        if(_preferences->getBool(preference_lock_gemini_enabled, false) != v)
        {
            _preferences->putBool(preference_lock_gemini_enabled, v);
            if (v)
            {
                _preferences->putBool(preference_register_as_app, true);
                _preferences->putBool(preference_lock_enabled, true);
                _preferences->putBool(preference_official_hybrid_enabled, true);
                _preferences->putBool(preference_official_hybrid_actions, true);
            }
            state.restartServicesNoReconnect = true;
        }
    }

    if(aclLvlChanged)
    {
        uint32_t curAclPrefs[17] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        uint32_t curBasicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        uint32_t curAdvancedLockConfigAclPrefs[26] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        uint32_t curBasicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        uint32_t curAdvancedOpenerConfigAclPrefs[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
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
                Log->print("Setting changed: ");
                Log->println("ACLPREFS");
                break;
            }
        }
        for(int i=0; i < 16; i++)
        {
            if(curBasicLockConfigAclPrefs[i] != basicLockConfigAclPrefs[i])
            {
                _preferences->putBytes(preference_conf_lock_basic_acl, (byte*)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
                Log->print("Setting changed: ");
                Log->println("ACLCONFBASICLOCK");
                break;
            }
        }
        for(int i=0; i < 26; i++)
        {
            if(curAdvancedLockConfigAclPrefs[i] != advancedLockConfigAclPrefs[i])
            {
                _preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
                Log->print("Setting changed: ");
                Log->println("ACLCONFADVANCEDLOCK");
                break;

            }
        }
        for(int i=0; i < 14; i++)
        {
            if(curBasicOpenerConfigAclPrefs[i] != basicOpenerConfigAclPrefs[i])
            {
                _preferences->putBytes(preference_conf_opener_basic_acl, (byte*)(&basicOpenerConfigAclPrefs), sizeof(basicOpenerConfigAclPrefs));
                Log->print("Setting changed: ");
                Log->println("ACLCONFBASICOPENER");
                break;
            }
        }
        for(int i=0; i < 22; i++)
        {
            if(curAdvancedOpenerConfigAclPrefs[i] != advancedOpenerConfigAclPrefs[i])
            {
                _preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));
                Log->print("Setting changed: ");
                Log->println("ACLCONFADVANCEDOPENER");
                break;
            }
        }
    }
}

#endif
