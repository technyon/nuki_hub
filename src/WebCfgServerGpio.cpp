#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"

#ifndef NUKI_HUB_UPDATER
esp_err_t WebCfgServer::buildGpioConfigHtml(PsychicRequest *request, PsychicResponse* resp)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savegpiocfg\">");
    response.print("<h3>GPIO Configuration</h3>");
    response.print("<table>");
    printCheckBox(&response, "RETGPIO", "Retain Input GPIO MQTT state", _preferences->getBool(preference_retain_gpio, false), "");

    const auto& availablePins = _gpio->availablePins();
    const auto& disabledPins = _gpio->getDisabledPins();

    for(const auto& pin : availablePins)
    {
        String pinStr = String(pin);
        bool disabled = std::find(disabledPins.begin(), disabledPins.end(), pin) != disabledPins.end();
        response.print("<tr><td>Gpio " + pinStr + "</td><td><select class=\"gpioselect\" name=\"" + pinStr + "\" data-value=\"" + (disabled ? String("21") : getPreselectionForGpio(pin)) + "\"></select></td></tr>");
    }

    response.print("</table>");
    response.print("<div class=\"actions\"><input type=\"submit\" name=\"submit\" value=\"Save\"></div>");
    response.print("</form>");

    response.print("<template id=\"gpiooptions\">");

    for(const auto& option : getGpioOptions())
    {
        response.print("<option value=\"");
        response.print(htmlEscape(option.first.c_str()));
        response.print("\">");
        response.print(htmlEscape(option.second.c_str()));
        response.print("</option>");
    }

    response.print("</template>");
    buildHtmlFooter(&response);
    return response.endSend();
}

void WebCfgServer::processGpioArgs(PsychicRequest *request, PsychicResponse* resp)
{
    std::vector<PinEntry> pinConfiguration;

    if(request->hasParam("RETGPIO"))
    {
        const PsychicWebParameter* p = request->getParam("RETGPIO");

        if(_preferences->getBool(preference_retain_gpio, false) != (p->value() == "1"))
        {
            _preferences->putBool(preference_retain_gpio, (p->value() == "1"));
        }
    }

    for (int i=0; i < 100; i++)
    {
        String gpio = String(i);
        if (!request->hasParam(gpio.c_str())) continue;
        const PsychicWebParameter* p = request->getParam(gpio.c_str());

        PinRole role = (PinRole)p->value().toInt();
        if(role != PinRole::Disabled)
        {
            PinEntry entry;
            entry.pin = p->name().toInt();
            entry.role = role;
            pinConfiguration.push_back(entry);
        }
    }

    _gpio->savePinConfiguration(pinConfiguration);
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

const String WebCfgServer::getPreselectionForGpio(const uint8_t &pin) const
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

#endif
