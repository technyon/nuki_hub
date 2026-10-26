#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#ifndef CONFIG_IDF_TARGET_ESP32H2
#include <esp_wifi.h>
#include <WiFi.h>
#endif

#ifndef CONFIG_IDF_TARGET_ESP32H2
esp_err_t WebCfgServer::buildSSIDListHtml(PsychicRequest *request, PsychicResponse* resp)
{
    _network->scan(true, false);
    createSsidList();

    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();

    for (int i = 0; i < _ssidList.size(); i++)
    {
        response.print("<tr class=\"trssid\" onclick=\"document.getElementById('inputssid').value = '" + _ssidList[i] + "';\"><td colspan=\"2\">" + _ssidList[i] + String((" (")) + String(_rssiList[i]) + String((" %)")) + "</td></tr>");
    }
    return response.endSend();
}

void WebCfgServer::createSsidList()
{
    int _foundNetworks = WiFi.scanComplete();
    std::vector<String> _tmpSsidList;
    std::vector<int> _tmpRssiList;

    for (int i = 0; i < _foundNetworks; i++)
    {
        int rssi = constrain((100.0 + WiFi.RSSI(i)) * 2, 0, 100);
        auto it1 = std::find(_ssidList.begin(), _ssidList.end(), WiFi.SSID(i));
        auto it2 = std::find(_tmpSsidList.begin(), _tmpSsidList.end(), WiFi.SSID(i));

        if(it1 == _ssidList.end())
        {
            _ssidList.push_back(WiFi.SSID(i));
            _rssiList.push_back(rssi);
            _tmpSsidList.push_back(WiFi.SSID(i));
            _tmpRssiList.push_back(rssi);
        }
        else if (it2 == _tmpSsidList.end())
        {
            _tmpSsidList.push_back(WiFi.SSID(i));
            _tmpRssiList.push_back(rssi);
            int index = it1 - _ssidList.begin();
            _rssiList[index] = rssi;
        }
        else
        {
            int index = it1 - _ssidList.begin();
            int index2 = it2 - _tmpSsidList.begin();
            if (_tmpRssiList[index2] < rssi)
            {
                _tmpRssiList[index2] = rssi;
                _rssiList[index] = rssi;
            }
        }
    }
}

esp_err_t WebCfgServer::buildWifiConnectHtml(PsychicRequest *request, PsychicResponse* resp)
{
    String header = "<style>.trssid:hover { cursor: pointer; color: blue; }</style><script>let intervalId; window.onload = function() { intervalId = setInterval(updateSSID, 3000); }; function updateSSID() { var request = new XMLHttpRequest(); request.open('GET', '/ssidlist', true); request.onload = () => { if (document.getElementById(\"aplist\") !== null) { document.getElementById(\"aplist\").innerHTML = request.responseText; } }; request.send(); }</script>";
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response, header);
    response.print("<h3>Available WiFi networks</h3>");
    response.print("<table id=\"aplist\">");
    createSsidList();
    for (int i = 0; i < _ssidList.size(); i++)
    {
        response.print("<tr class=\"trssid\" onclick=\"document.getElementById('inputssid').value = '" + _ssidList[i] + "';\"><td colspan=\"2\">" + _ssidList[i] + String((" (")) + String(_rssiList[i]) + String((" %)")) + "</td></tr>");
    }
    response.print("</table>");
    response.print("<form class=\"adapt\" method=\"post\" action=\"savewifi\">");
    response.print("<h3>WiFi credentials</h3>");
    response.print("<table>");
    printInputField(&response, "WIFISSID", "SSID", "", 32, "id=\"inputssid\"", false, true);
    printInputField(&response, "WIFIPASS", "Secret key", "", 63, "id=\"inputpass\"", false, true);
    printCheckBox(&response, "FINDBESTRSSI", "Find AP with best signal (disable for hidden SSID)", _preferences->getBool(preference_find_best_rssi, true), "");
    printCheckBox(&response, "WIFIPSDIS", "Disable Wi-Fi power save (improves weak connections)", _preferences->getBool(preference_wifi_power_save_disabled, false), "");
    printInputField(&response, "WIFIBSSID", "Preferred access point MAC address (BSSID, e.g. aa:bb:cc:dd:ee:ff, falls back to any access point after two failed attempts)", _preferences->getString(preference_wifi_preferred_bssid).c_str(), 17, "");
    response.print("</table>");
    response.print("<h3>IP Address assignment</h3>");
    response.print("<table>");
    printCheckBox(&response, "DHCPENA", "Enable DHCP", _preferences->getBool(preference_ip_dhcp_enabled), "");
    printInputField(&response, "IPADDR", "Static IP address", _preferences->getString(preference_ip_address).c_str(), 15, "");
    printInputField(&response, "IPSUB", "Subnet", _preferences->getString(preference_ip_subnet).c_str(), 15, "");
    printInputField(&response, "IPGTW", "Default gateway", _preferences->getString(preference_ip_gateway).c_str(), 15, "");
    printInputField(&response, "DNSSRV", "DNS Server", _preferences->getString(preference_ip_dns_server).c_str(), 15, "");
    response.print("</table>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form>");
#ifndef NUKI_HUB_UPDATER
    response.print("<h3>Import configuration</h3>");
    response.print("<form method=\"post\" action=\"post\"><input type=\"file\" id=\"importfile\" accept=\".json,application/json\" onchange=\"var reader = new FileReader(); reader.onload = function(e) { document.getElementById('importjson').value = e.target.result; }; reader.readAsText(this.files[0]);\"><br/>");
    response.print("<textarea id=\"importjson\" name=\"importjson\" rows=\"10\" cols=\"50\" style=\"display: none;\"></textarea>");
    response.print("<input type=\"hidden\" name=\"page\" value=\"import\">");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Import\"></form>");
#endif
    response.print("<form action=\"/reboot\" method=\"get\"><br>");
    response.print("<input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\" /><input type=\"submit\" value=\"Reboot\" /></form>");
    response.print("</body></html>");
    return response.endSend();
}

bool WebCfgServer::processWiFi(PsychicRequest *request, PsychicResponse* resp, String& message)
{
    bool res = false;

    String ssid;
    String pass;

    if(request->hasParam("WIFISSID"))
    {
        ssid = request->getParam("WIFISSID")->value();
    }
    if(request->hasParam("WIFIPASS"))
    {
        pass = request->getParam("WIFIPASS")->value();
    }
    if(request->hasParam("DHCPENA"))
    {
        String dhcpEna = request->getParam("DHCPENA")->value();
        if(_preferences->getBool(preference_ip_dhcp_enabled, true) != (dhcpEna == "1"))
        {
            _preferences->putBool(preference_ip_dhcp_enabled, (dhcpEna == "1"));
        }
    }
    if(request->hasParam("IPADDR"))
    {
        String ipAddr = request->getParam("IPADDR")->value();
        if(_preferences->getString(preference_ip_address, "") != ipAddr)
        {
            _preferences->putString(preference_ip_address, ipAddr);
        }
    }
    if(request->hasParam("IPSUB"))
    {
        String ipSub = request->getParam("IPSUB")->value();
        if(_preferences->getString(preference_ip_subnet, "") != ipSub)
        {
            _preferences->putString(preference_ip_subnet, ipSub);
        }
    }
    if(request->hasParam("IPGTW"))
    {
        String ipGtw = request->getParam("IPGTW")->value();
        if(_preferences->getString(preference_ip_gateway, "") != ipGtw)
        {
            _preferences->putString(preference_ip_gateway, ipGtw);
        }
    }
    if(request->hasParam("DNSSRV"))
    {
        String dnsSrv = request->getParam("DNSSRV")->value();
        if(_preferences->getString(preference_ip_dns_server, "") != dnsSrv)
        {
            _preferences->putString(preference_ip_dns_server, dnsSrv);
        }
    }
    if(request->hasParam("FINDBESTRSSI"))
    {
        String findBestRssi = request->getParam("FINDBESTRSSI")->value();
        if(_preferences->getBool(preference_find_best_rssi, false) != (findBestRssi == "1"))
        {
            _preferences->putBool(preference_find_best_rssi, (findBestRssi == "1"));
        }
    }
    if(request->hasParam("WIFIPSDIS"))
    {
        String wifiPowerSaveDisabled = request->getParam("WIFIPSDIS")->value();
        if(_preferences->getBool(preference_wifi_power_save_disabled, false) != (wifiPowerSaveDisabled == "1"))
        {
            _preferences->putBool(preference_wifi_power_save_disabled, (wifiPowerSaveDisabled == "1"));
        }
    }
    if(request->hasParam("WIFIBSSID"))
    {
        String wifiBssid = request->getParam("WIFIBSSID")->value();
        if(_preferences->getString(preference_wifi_preferred_bssid, "") != wifiBssid)
        {
            _preferences->putString(preference_wifi_preferred_bssid, wifiBssid);
        }
    }

    ssid.trim();
    pass.trim();

    if (ssid.length() > 0 && pass.length() > 0)
    {
        const IPConfiguration* _ipConfiguration = new IPConfiguration(_preferences);

        if(!_ipConfiguration->dhcpEnabled())
        {
            WiFi.config(_ipConfiguration->ipAddress(), _ipConfiguration->dnsServer(), _ipConfiguration->defaultGateway(), _ipConfiguration->subnet());
        }

        WiFi.begin(ssid, pass);

        int loop = 0;
        while(!_network->isConnected() && loop < 150)
        {
            espDelayAck(100);
            loop++;
        }

        if (!_network->isConnected())
        {
            message = "Failed to connect to the given SSID, settings not saved<br/>";
            return res;
        }
        else
        {
            message = "Connection successful. Rebooting Nuki Hub.<br/>Please connect this device to the wireless network with the SSID "
                      + ssid + " or a wired/wireless connection that has access to the network of the selected SSID now<br/>and navigate to Nuki Hub at ";

            if (_isSSL)
            {
                if (_preferences->getString(preference_https_fqdn, "") != "")
                {
                    message += "<a href=\"https://" + _preferences->getString(preference_https_fqdn, "") + "\">https://" + _preferences->getString(preference_https_fqdn, "") + "</a>";
                }
                else
                {
                    message += "<a href=\"https://" + _network->localIP() + "\">https://" + _network->localIP() + "</a>";
                }
            }
            else
            {
                message += "<a href=\"http://" + _network->localIP() + "\">http://" + _network->localIP() + "</a>";
            }
            _preferences->putString(preference_wifi_ssid, ssid);
            _preferences->putString(preference_wifi_pass, pass);
            res = true;
        }
    }
    else
    {
        message = "No SSID or secret key entered, credentials not saved<br/>";
        return res;
    }

    return res;
}

#endif
#ifndef NUKI_HUB_UPDATER
#ifndef CONFIG_IDF_TARGET_ESP32H2
esp_err_t WebCfgServer::buildConfigureWifiHtml(PsychicRequest *request, PsychicResponse* resp)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form method=\"get\" action=\"get\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"wifimanager\">");
    response.print("<h3>Wi-Fi</h3>");
    response.print("Click confirm to remove saved WiFi settings and restart ESP into Wi-Fi configuration mode. After restart, connect to ESP access point to reconfigure Wi-Fi.<br><br><br>");
    response.print("<input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\" /><input type=\"submit\" value=\"Reboot\" /></form>");
    response.print("</form>");
    response.print("</body></html>");
    return response.endSend();
}

#endif
#endif
