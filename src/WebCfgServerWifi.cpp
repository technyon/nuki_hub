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
        response.print("<tr class=\"trssid\" data-ssid=\"" + htmlEscape(_ssidList[i].c_str()) + "\"><td>" + htmlEscape(_ssidList[i].c_str()) + " (" + String(_rssiList[i]) + " %)</td></tr>");
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
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<div class=\"wizard\" data-wizard>");
    response.print("<ol class=\"steps\"><li>Start</li><li>Network</li><li>Connect</li></ol>");

    response.print("<section data-step=\"1\">");
    response.print("<h3>Welcome</h3>");
    response.print("<p>Nuki Hub connects your Nuki devices to your network. First it needs to join your Wi-Fi; everything else is configured afterwards from the network.</p>");
    response.print("<div class=\"card\"><h4>Set up a new hub</h4><p>Pick your Wi-Fi and enter its password. The hub reboots and is then reachable on your network.</p><div class=\"actions\"><button type=\"button\" class=\"primary\" data-next>Continue</button></div></div>");
#ifndef NUKI_HUB_UPDATER
    response.print("<div class=\"card\"><h4>Restore a backup</h4><p>Upload a settings export from another Nuki Hub. Wi-Fi settings and pairing data in the export are restored too.</p>");
    response.print("<form method=\"post\" action=\"post\"><table><tr><td>Configuration file</td><td><input type=\"file\" id=\"importfile\" accept=\".json,application/json\" data-fill=\"#importjson\"></td></tr></table>");
    response.print("<textarea id=\"importjson\" name=\"importjson\" hidden></textarea>");
    response.print("<input type=\"hidden\" name=\"page\" value=\"import\">");
    response.print("<div class=\"actions\"><input type=\"submit\" name=\"submit\" value=\"Restore\"></div></form></div>");
#endif
    response.print("</section>");

    response.print("<form method=\"post\" action=\"savewifi\">");
    response.print("<section data-step=\"2\" hidden>");
    response.print("<h3>Choose your network</h3>");
    response.print("<p>The list refreshes every few seconds. Click a network to select it, or type a hidden network's name.</p>");
    response.print("<table id=\"aplist\">");
    createSsidList();
    for (int i = 0; i < _ssidList.size(); i++)
    {
        response.print("<tr class=\"trssid\" data-ssid=\"" + htmlEscape(_ssidList[i].c_str()) + "\"><td>" + htmlEscape(_ssidList[i].c_str()) + " (" + String(_rssiList[i]) + " %)</td></tr>");
    }
    response.print("</table>");
    response.print("<table>");
    printInputField(&response, "WIFISSID", "Network name (SSID)", "", 32, "id=\"inputssid\"", false, true);
    printCheckBox(&response, "FINDBESTRSSI", "Connect to the access point with the strongest signal (disable for hidden networks)", _preferences->getBool(preference_find_best_rssi, true), "");
    printInputField(&response, "WIFIBSSID", "Preferred access point MAC address (optional, e.g. aa:bb:cc:dd:ee:ff)", _preferences->getString(preference_wifi_preferred_bssid).c_str(), 17, "");
    response.print("</table>");
    response.print("<div class=\"actions\"><button type=\"button\" data-back>Back</button><button type=\"button\" class=\"primary\" data-next>Continue</button></div>");
    response.print("</section>");

    response.print("<section data-step=\"3\" hidden>");
    response.print("<h3>Connect</h3>");
    response.print("<table>");
    printInputField(&response, "WIFIPASS", "Wi-Fi password", "", 63, "id=\"inputpass\"", false, true);
    printCheckBox(&response, "WIFIPSDIS", "Disable Wi-Fi power save (improves weak connections)", _preferences->getBool(preference_wifi_power_save_disabled, false), "");
    response.print("</table>");
    response.print("<h3>IP address</h3>");
    response.print("<table>");
    printCheckBox(&response, "DHCPENA", "Get an IP address automatically (DHCP)", _preferences->getBool(preference_ip_dhcp_enabled), "");
    printInputField(&response, "IPADDR", "Static IP address", _preferences->getString(preference_ip_address).c_str(), 15, "");
    printInputField(&response, "IPSUB", "Subnet", _preferences->getString(preference_ip_subnet).c_str(), 15, "");
    printInputField(&response, "IPGTW", "Default gateway", _preferences->getString(preference_ip_gateway).c_str(), 15, "");
    printInputField(&response, "DNSSRV", "DNS Server", _preferences->getString(preference_ip_dns_server).c_str(), 15, "");
    response.print("</table>");
    response.print("<div class=\"actions\"><button type=\"button\" data-back>Back</button><input type=\"submit\" name=\"submit\" value=\"Connect\"></div>");
    response.print("</section>");
    response.print("</form>");

    response.print("<form action=\"/reboot\" method=\"get\" class=\"wizard-foot\">");
    response.print("<input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\"><button type=\"submit\" class=\"small\">Reboot the hub</button></form>");
    response.print("</div>");
    buildHtmlFooter(&response);
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
    response.print("<p>Click confirm to remove saved WiFi settings and restart ESP into Wi-Fi configuration mode. After restart, connect to ESP access point to reconfigure Wi-Fi.</p>");
    response.print("<input type=\"hidden\" name=\"CONFIRMTOKEN\" value=\"" + _confirmCode + "\"><div class=\"actions\"><input type=\"submit\" value=\"Reboot\"></div></form>");
    buildHtmlFooter(&response);
    return response.endSend();
}

#endif
#endif
