#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
#include "util/CertUtil.h"
#include "FS.h"
#include "SPIFFS.h"
#ifdef NUKI_HUB_HTTPS_SERVER
#include "util/SSLCert.hpp"
#endif

#ifndef NUKI_HUB_UPDATER
esp_err_t WebCfgServer::buildNetworkConfigHtml(PsychicRequest *request, PsychicResponse* resp)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response.print("<h3>Network Configuration</h3>");
    response.print("<table>");
    printInputField(&response, "HOSTNAME", "Hostname (needs to be unique)", _preferences->getString(preference_hostname).c_str(), 100, "");
    printDropDown(&response, "NWHW", "Network hardware", String(_preferences->getInt(preference_network_hardware)), getNetworkDetectionOptions(), "");
    printInputField(&response, "HASSCUURL", "Home Assistant device configuration URL (empty to use http://LOCALIP; fill when using a reverse proxy for example)", _preferences->getString(preference_mqtt_hass_cu_url).c_str(), 261, "");
#ifndef CONFIG_IDF_TARGET_ESP32H2
    printInputField(&response, "RSSI", "RSSI Publish interval (seconds; -1 to disable)", _preferences->getInt(preference_rssi_publish_interval), 6, "");
#endif
    printCheckBox(&response, "RSTDISC", "Restart on disconnect", _preferences->getBool(preference_restart_on_disconnect), "");
    printCheckBox(&response, "CHECKUPDATE", "Check for Firmware Updates every 24h", _preferences->getBool(preference_check_updates), "");
    printCheckBox(&response, "FINDBESTRSSI", "Find WiFi AP with strongest signal", _preferences->getBool(preference_find_best_rssi, false), "");
    printCheckBox(&response, "WIFIPSDIS", "Disable Wi-Fi power save (improves weak connections)", _preferences->getBool(preference_wifi_power_save_disabled, false), "");
    printInputField(&response, "WIFIBSSID", "Preferred access point MAC address (BSSID, e.g. aa:bb:cc:dd:ee:ff, falls back to any access point after two failed attempts)", _preferences->getString(preference_wifi_preferred_bssid).c_str(), 17, "");
    if(nuki_hub_https_server_enabled)
    {
        response.print("<tr><td>Set HTTP SSL Certificate</td><td><button title=\"Set HTTP SSL Certificate\" onclick=\" window.open('/get?page=httpcrtconfig', '_self'); return false;\">Change</button></td></tr>");
        response.print("<tr><td>Set HTTP SSL Key</td><td><button title=\"Set HTTP SSL Key\" onclick=\" window.open('/get?page=httpkeyconfig', '_self'); return false;\">Change</button></td></tr>");
        response.print("<tr><td>Generate self-signed HTTP SSL Certificate and key</td><td><button title=\"Generate HTTP SSL Certificate and key\" onclick=\" window.open('/get?page=selfsignhttps', '_self'); return false;\">Generate</button></td></tr>");
        printInputField(&response, "HTTPSFQDN", "Nuki Hub FQDN for HTTP redirect", _preferences->getString(preference_https_fqdn, "").c_str(), 255, "");
    }
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
    response.print("</body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildCustomNetworkConfigHtml(PsychicRequest *request, PsychicResponse* resp)
{
    String header = "<script>window.onload=function(){var physelect=document.getElementsByName('NWCUSTPHY')[0];hideshowopt(physelect.value);physelect.addEventListener('change', function(event){var select=event.target;var selectedOption=select.options[select.selectedIndex];hideshowopt(selectedOption.getAttribute('value'));});};function hideshowopt(value){if(value>=1&&value<=3){hideopt('internalopt',true);hideopt('externalopt',false);}else if(value>=4&&value<=9){hideopt('internalopt', false);hideopt('externalopt', true);}else {hideopt('internalopt', true);hideopt('externalopt', true);}}function hideopt(opts,hide){var hideopts = document.getElementsByClassName(opts);for(var i=0;i<hideopts.length;i++){if(hide==true){hideopts[i].style.display='none';}else{hideopts[i].style.display='block';}}}</script>";
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response, header);
    response.print("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response.print("<h3>Custom Ethernet Configuration</h3>");
    response.print("<table>");
    printDropDown(&response, "NWCUSTPHY", "PHY", String(_preferences->getInt(preference_network_custom_phy)), getNetworkCustomPHYOptions(), "");
    printInputField(&response, "NWCUSTADDR", "ADDR", _preferences->getInt(preference_network_custom_addr, 1), 6, "");
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
    printDropDown(&response, "NWCUSTCLK", "CLK", String(_preferences->getInt(preference_network_custom_clk, 0)), getNetworkCustomCLKOptions(), "internalopt");
    printInputField(&response, "NWCUSTPWR", "PWR", _preferences->getInt(preference_network_custom_pwr, 12), 6, "class=\"internalopt\"");
    printInputField(&response, "NWCUSTMDIO", "MDIO", _preferences->getInt(preference_network_custom_mdio), 6, "class=\"internalopt\"");
    printInputField(&response, "NWCUSTMDC", "MDC", _preferences->getInt(preference_network_custom_mdc), 6, "class=\"internalopt\"");
#endif
    printInputField(&response, "NWCUSTIRQ", "IRQ", _preferences->getInt(preference_network_custom_irq, -1), 6, "class=\"externalopt\"");
    printInputField(&response, "NWCUSTRST", "RST", _preferences->getInt(preference_network_custom_rst, -1), 6, "class=\"externalopt\"");
    printInputField(&response, "NWCUSTCS", "CS", _preferences->getInt(preference_network_custom_cs, -1), 6, "class=\"externalopt\"");
    printInputField(&response, "NWCUSTSCK", "SCK", _preferences->getInt(preference_network_custom_sck, -1), 6, "class=\"externalopt\"");
    printInputField(&response, "NWCUSTMISO", "MISO", _preferences->getInt(preference_network_custom_miso, -1), 6, "class=\"externalopt\"");
    printInputField(&response, "NWCUSTMOSI", "MOSI", _preferences->getInt(preference_network_custom_mosi, -1), 6, "class=\"externalopt\"");
    response.print("</table>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form>");
    response.print("</body></html>");
    return response.endSend();
}

const std::vector<std::pair<String, String>> WebCfgServer::getNetworkDetectionOptions() const
{
    std::vector<std::pair<String, String>> options;

    options.push_back(std::make_pair("1", "Wi-Fi"));
    options.push_back(std::make_pair("2", "Generic W5500"));
    options.push_back(std::make_pair("3", "M5Stack Atom POE (W5500)"));
    options.push_back(std::make_pair("10", "M5Stack Atom POE S3 (W5500)"));
    options.push_back(std::make_pair("4", "Olimex ESP32-POE / ESP-POE-ISO"));
    options.push_back(std::make_pair("5", "WT32-ETH01"));
    options.push_back(std::make_pair("6", "M5STACK PoESP32 Unit"));
    options.push_back(std::make_pair("7", "LilyGO T-ETH-POE"));
    options.push_back(std::make_pair("12", "LilyGO T-ETH ELite"));
    options.push_back(std::make_pair("8", "GL-S10"));
    options.push_back(std::make_pair("9", "ETH01-Evo"));
    options.push_back(std::make_pair("13", "Waveshare ESP32-S3-ETH / ESP32-S3-ETH-POE"));
    options.push_back(std::make_pair("14", "LilyGO T-ETH-Lite-ESP32S3"));
    options.push_back(std::make_pair("15", "Waveshare ESP32-P4-NANO"));
    options.push_back(std::make_pair("16", "Waveshare ESP32-P4-Module-DEV-KIT"));
    options.push_back(std::make_pair("17", "ESP32-P4-Function-EV-Board"));
    options.push_back(std::make_pair("11", "Custom LAN module"));

    return options;
}

const std::vector<std::pair<String, String>> WebCfgServer::getNetworkCustomPHYOptions() const
{
    std::vector<std::pair<String, String>> options;
    options.push_back(std::make_pair("0", "Disabled"));
    options.push_back(std::make_pair("1", "W5500"));
    options.push_back(std::make_pair("2", "DN9051"));
    options.push_back(std::make_pair("3", "KSZ8851SNL"));
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
    options.push_back(std::make_pair("4", "LAN8720"));
    options.push_back(std::make_pair("5", "RTL8201"));
    options.push_back(std::make_pair("6", "TLK110/IP101"));
    options.push_back(std::make_pair("7", "DP83848"));
    options.push_back(std::make_pair("8", "KSZ8041"));
    options.push_back(std::make_pair("9", "KSZ8081"));
#endif

    return options;
}

#endif
#ifndef NUKI_HUB_UPDATER
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
const std::vector<std::pair<String, String>> WebCfgServer::getNetworkCustomCLKOptions() const
{
    std::vector<std::pair<String, String>> options;
    options.push_back(std::make_pair("0", "GPIO0 IN"));
    options.push_back(std::make_pair("1", "GPIO0 OUT"));
    options.push_back(std::make_pair("2", "GPIO16 OUT"));
    options.push_back(std::make_pair("3", "GPIO17 OUT"));
    return options;
}

#endif
#endif
#ifndef NUKI_HUB_UPDATER
esp_err_t WebCfgServer::buildHttpSSLConfigHtml(PsychicRequest *request, PsychicResponse* resp, int type)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form class=\"adapt\" method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response.print("<input type=\"hidden\" name=\"httpssl\" value=\"1\">");
    response.print("<h3>HTTP SSL Configuration</h3>");
    response.print("<table>");

    if (type == 1)
    {
        bool found = false;

        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
        }
        else
        {
            File file = SPIFFS.open("/http_ssl.crt");
            if (!file || file.isDirectory())
            {
                Log->println("http_ssl.crt not found");
            }
            else
            {
                Log->println("Reading http_ssl.crt");
                file.close();

                printTextarea(&response, "HTTPCRT", "HTTP SSL Certificate", "*", 4400, true, true);
                found = true;
            }
        }

        if (!found)
        {
            printTextarea(&response, "HTTPCRT", "HTTP SSL Certificate", "", 4400, true, true);
        }
    }
    else if (type == 2)
    {
        bool found = false;

        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
        }
        else
        {
            File file = SPIFFS.open("/http_ssl.key");
            if (!file || file.isDirectory())
            {
                Log->println("http_ssl.key not found");
            }
            else
            {
                Log->println("Reading http_ssl.key");
                file.close();

                printTextarea(&response, "HTTPKEY", "HTTP SSL Key", "*", certSize(), true, true);
                found = true;
            }
        }

        if (!found)
        {
            printTextarea(&response, "HTTPKEY", "HTTP SSL Key", "", certSize(), true, true);
        }
    }
    else
    {
        response.print("<input type=\"hidden\" name=\"HTTPGEN\" value=\"1\">");
        response.print("<tr><td>Click save to generate a HTTPS SSL Certificate and key</td></tr>");
    }
    response.print("</table>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form>");
    response.print("</body>");
    response.print("</html>");
    return response.endSend();
}

#endif
#ifndef NUKI_HUB_UPDATER
#ifdef NUKI_HUB_HTTPS_SERVER
void WebCfgServer::createSSLCertificate()
{
    SSLCert* cert;
    cert = new SSLCert();
    int createCertResult = createSelfSignedCert(
                               *cert,
                               KEYSIZE_2048,
                               "CN=nukihub.local,O=NukiHub,C=DE",
                               "20250101000000",
                               "20350101000000"
                           );

    if (createCertResult == 0)
    {
        if (!SPIFFS.begin(true))
        {
            Log->println("SPIFFS Mount Failed");
        }
        else
        {
            File file = SPIFFS.open("/http_ssl.crt", FILE_WRITE);
            if (!file)
            {
                Log->println("Failed to open /http_ssl.crt for writing");
            }
            else
            {
                if (!file.print(cert->getCertPEM()))
                {
                    Log->println("Failed to write /http_ssl.crt");
                }
                file.close();
            }

            File file2 = SPIFFS.open("/http_ssl.key", FILE_WRITE);
            if (!file2)
            {
                Log->println("Failed to open /http_ssl.key for writing");
            }
            else
            {
                if (!file2.print(cert->getKeyPEM()))
                {
                    Log->println("Failed to write /http_ssl.key");
                }
                file2.close();
            }
        }
    }
    else
    {
        Log->print("SSL Self sign failed: ");
        Log->println(createCertResult);
    }
}

#endif
void WebCfgServer::processNetworkArgs(PsychicRequest *request, ProcessArgsState& state, String& message)
{
    bool networkReconfigure = false;
    String value;

    if(request->hasParam("HTTPCRT") && nuki_hub_https_server_enabled)
    {
        value = request->getParam("HTTPCRT")->value();
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
                    File file = SPIFFS.open("/http_ssl.crt", FILE_WRITE);
                    if (!file)
                    {
                        Log->println("Failed to open /http_ssl.crt for writing");
                    }
                    else
                    {
                        if (!file.print(value))
                        {
                            Log->println("Failed to write /http_ssl.crt");
                        }
                        file.close();
                    }
                }
                else
                {
                    if (!SPIFFS.remove("/http_ssl.crt"))
                    {
                        Log->println("Failed to delete /http_ssl.crt");
                    }
                }
                state.restartServicesNoReconnect = true;
            }
        }
    }

    if(request->hasParam("HTTPKEY") && nuki_hub_https_server_enabled)
    {
        value = request->getParam("HTTPKEY")->value();
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
                    File file = SPIFFS.open("/http_ssl.key", FILE_WRITE);
                    if (!file)
                    {
                        Log->println("Failed to open /http_ssl.key for writing");
                    }
                    else
                    {
                        if (!file.print(value))
                        {
                            Log->println("Failed to write /http_ssl.key");
                        }
                        file.close();
                    }
                }
                else
                {
                    if (!SPIFFS.remove("/http_ssl.key"))
                    {
                        Log->println("Failed to delete /http_ssl.key");
                    }
                }
                state.restartServicesNoReconnect = true;
            }
        }
    }

    if(request->hasParam("HTTPGEN") && nuki_hub_https_server_enabled)
    {
#ifdef NUKI_HUB_HTTPS_SERVER
        createSSLCertificate();
#endif
        state.configChanged = true;
    }

    if(request->hasParam("NWHW"))
    {
        value = request->getParam("NWHW")->value();
        if(_preferences->getInt(preference_network_hardware, 0) != value.toInt())
        {
            if(value.toInt() > 1)
            {
                networkReconfigure = true;
                if(value.toInt() != 11)
                {
                    _preferences->putInt(preference_network_custom_phy, 0);
                }
            }
            _preferences->putInt(preference_network_hardware, value.toInt());

            state.configChanged = true;
        }
    }

    if(request->hasParam("NWCUSTPHY"))
    {
        value = request->getParam("NWCUSTPHY")->value();
        if(_preferences->getInt(preference_network_custom_phy, 0) != value.toInt())
        {
            networkReconfigure = true;
            _preferences->putInt(preference_network_custom_phy, value.toInt());

            state.configChanged = true;
        }
    }

    if(request->hasParam("NWCUSTADDR"))
    {
        value = request->getParam("NWCUSTADDR")->value();
        if(_preferences->getInt(preference_network_custom_addr, -1) != value.toInt())
        {
            networkReconfigure = true;
            _preferences->putInt(preference_network_custom_addr, value.toInt());

            state.configChanged = true;
        }
    }

    if(request->hasParam("NWCUSTIRQ"))
    {
        value = request->getParam("NWCUSTIRQ")->value();
        if(_preferences->getInt(preference_network_custom_irq, 0) != value.toInt())
        {
            networkReconfigure = true;
            _preferences->putInt(preference_network_custom_irq, value.toInt());

            state.configChanged = true;
        }
    }

    if(request->hasParam("NWCUSTRST"))
    {
        value = request->getParam("NWCUSTRST")->value();
        if(_preferences->getInt(preference_network_custom_rst, 0) != value.toInt())
        {
            networkReconfigure = true;
            _preferences->putInt(preference_network_custom_rst, value.toInt());

            state.configChanged = true;
        }
    }

    if(request->hasParam("NWCUSTCS"))
    {
        value = request->getParam("NWCUSTCS")->value();
        if(_preferences->getInt(preference_network_custom_cs, 0) != value.toInt())
        {
            networkReconfigure = true;
            _preferences->putInt(preference_network_custom_cs, value.toInt());

            state.configChanged = true;
        }
    }

    if(request->hasParam("NWCUSTSCK"))
    {
        value = request->getParam("NWCUSTSCK")->value();
        if(_preferences->getInt(preference_network_custom_sck, 0) != value.toInt())
        {
            networkReconfigure = true;
            _preferences->putInt(preference_network_custom_sck, value.toInt());

            state.configChanged = true;
        }
    }

    if(request->hasParam("NWCUSTMISO"))
    {
        value = request->getParam("NWCUSTMISO")->value();
        if(_preferences->getInt(preference_network_custom_miso, 0) != value.toInt())
        {
            networkReconfigure = true;
            _preferences->putInt(preference_network_custom_miso, value.toInt());

            state.configChanged = true;
        }
    }

    if(request->hasParam("NWCUSTMOSI"))
    {
        value = request->getParam("NWCUSTMOSI")->value();
        if(_preferences->getInt(preference_network_custom_mosi, 0) != value.toInt())
        {
            networkReconfigure = true;
            _preferences->putInt(preference_network_custom_mosi, value.toInt());

            state.configChanged = true;
        }
    }

    if(request->hasParam("NWCUSTPWR"))
    {
        value = request->getParam("NWCUSTPWR")->value();
        if(_preferences->getInt(preference_network_custom_pwr, 0) != value.toInt())
        {
            networkReconfigure = true;
            _preferences->putInt(preference_network_custom_pwr, value.toInt());

            state.configChanged = true;
        }
    }

    if(request->hasParam("NWCUSTMDIO"))
    {
        value = request->getParam("NWCUSTMDIO")->value();
        if(_preferences->getInt(preference_network_custom_mdio, 0) != value.toInt())
        {
            networkReconfigure = true;
            _preferences->putInt(preference_network_custom_mdio, value.toInt());

            state.configChanged = true;
        }
    }

    if(request->hasParam("NWCUSTMDC"))
    {
        value = request->getParam("NWCUSTMDC")->value();
        if(_preferences->getInt(preference_network_custom_mdc, 0) != value.toInt())
        {
            networkReconfigure = true;
            _preferences->putInt(preference_network_custom_mdc, value.toInt());

            state.configChanged = true;
        }
    }

    if(request->hasParam("NWCUSTCLK"))
    {
        value = request->getParam("NWCUSTCLK")->value();
        if(_preferences->getInt(preference_network_custom_clk, 0) != value.toInt())
        {
            networkReconfigure = true;
            _preferences->putInt(preference_network_custom_clk, value.toInt());

            state.configChanged = true;
        }
    }

    if(request->hasParam("RSSI"))
    {
        value = request->getParam("RSSI")->value();
        if(_preferences->getInt(preference_rssi_publish_interval, 60) != value.toInt())
        {
            _preferences->putInt(preference_rssi_publish_interval, value.toInt());

        }
    }

    if(request->hasParam("HTTPSFQDN"))
    {
        value = request->getParam("HTTPSFQDN")->value();
        if(_preferences->getString(preference_https_fqdn, "") != value)
        {
            _preferences->putString(preference_https_fqdn, value);

            state.restartServicesNoReconnect = true;
        }
    }

    if(request->hasParam("HASSCUURL"))
    {
        value = request->getParam("HASSCUURL")->value();
        if(_preferences->getString(preference_mqtt_hass_cu_url, "") != value)
        {
            _preferences->putString(preference_mqtt_hass_cu_url, value);

        }
    }

    if(request->hasParam("HOSTNAME"))
    {
        value = request->getParam("HOSTNAME")->value();
        if(_preferences->getString(preference_hostname, "") != value)
        {
            _preferences->putString(preference_hostname, value);

            state.configChanged = true;
        }
    }

    if(request->hasParam("FINDBESTRSSI"))
    {
        bool v = isParameterTrue(request, "FINDBESTRSSI");
        _preferences->putBool(preference_find_best_rssi, v);
    }

    if(request->hasParam("WIFIPSDIS"))
    {
        bool v = isParameterTrue(request, "WIFIPSDIS");
        _preferences->putBool(preference_wifi_power_save_disabled, v);
    }

    if(request->hasParam("WIFIBSSID"))
    {
        value = request->getParam("WIFIBSSID")->value();
        if(_preferences->getString(preference_wifi_preferred_bssid, "") != value)
        {
            _preferences->putString(preference_wifi_preferred_bssid, value);

            state.configChanged = true;
        }
    }

    if(request->hasParam("RSTDISC"))
    {
        bool v = isParameterTrue(request, "RSTDISC");
        _preferences->putBool(preference_restart_on_disconnect, v);
    }

    if(request->hasParam("CHECKUPDATE"))
    {
        bool v = isParameterTrue(request, "CHECKUPDATE");
        if(_preferences->getBool(preference_check_updates, false) != v)
        {
            _preferences->putBool(preference_check_updates, v);

            state.restartServicesReconnect = true;
        }
    }

    if(request->hasParam("DHCPENA"))
    {
        bool v = isParameterTrue(request, "DHCPENA");
        if(_preferences->getBool(preference_ip_dhcp_enabled, true) != v)
        {
            _preferences->putBool(preference_ip_dhcp_enabled, v);

            state.configChanged = true;
        }
    }

    if(request->hasParam("IPADDR"))
    {
        value = request->getParam("IPADDR")->value();
        if(_preferences->getString(preference_ip_address, "") != value)
        {
            _preferences->putString(preference_ip_address, value);

            state.configChanged = true;
        }
    }

    if(request->hasParam("IPSUB"))
    {
        value = request->getParam("IPSUB")->value();
        if(_preferences->getString(preference_ip_subnet, "") != value)
        {
            _preferences->putString(preference_ip_subnet, value);

            state.configChanged = true;
        }
    }

    if(request->hasParam("IPGTW"))
    {
        value = request->getParam("IPGTW")->value();
        if(_preferences->getString(preference_ip_gateway, "") != value)
        {
            _preferences->putString(preference_ip_gateway, value);

            state.configChanged = true;
        }
    }

    if(request->hasParam("DNSSRV"))
    {
        value = request->getParam("DNSSRV")->value();
        if(_preferences->getString(preference_ip_dns_server, "") != value)
        {
            _preferences->putString(preference_ip_dns_server, value);

            state.configChanged = true;
        }
    }

    if(networkReconfigure)
    {
        _preferences->putBool(preference_ntw_reconfigure, true);
    }
}

#endif
