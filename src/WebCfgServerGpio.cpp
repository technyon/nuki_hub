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
#if SOC_RMT_SUPPORTED
    printInputField(&response, "LEDBRI", "WS2812 status LED brightness (1-100 %)", _preferences->getInt(preference_status_led_brightness, 20), 3, "");
    std::vector<std::pair<String, String>> ledOrderOptions =
    {
        { "0", "GRB (most boards)" },
        { "1", "RGB" },
        { "2", "BRG" },
        { "3", "RBG" },
        { "4", "GBR" },
        { "5", "BGR" }
    };
    printDropDown(&response, "LEDORD", "WS2812 status LED color order", String(_preferences->getInt(preference_status_led_order, 0)), ledOrderOptions, "");
    printCheckBox(&response, "LEDQUIET", "WS2812 status LED off while everything is OK", _preferences->getBool(preference_status_led_quiet, false), "");
#endif

    const auto& availablePins = _gpio->availablePins();
    const auto& disabledPins = _gpio->getDisabledPins();

    for(const auto& pin : availablePins)
    {
        String pinStr = String(pin);
        String pinLabel = "Gpio " + pinStr;
#if SOC_RMT_SUPPORTED && defined(PIN_RGB_LED)
        if(pin == PIN_RGB_LED)
        {
            pinLabel += " (RGB LED on many dev boards)";
        }
#endif
        bool disabled = std::find(disabledPins.begin(), disabledPins.end(), pin) != disabledPins.end();
        response.print("<tr><td>" + pinLabel + "</td><td><select class=\"gpioselect\" name=\"" + pinStr + "\" data-value=\"" + (disabled ? String("21") : getPreselectionForGpio(pin)) + "\"></select></td></tr>");
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
        // The checkbox posts a hidden "0" before the checked "1"
        const bool retain = isParameterTrue(request, "RETGPIO");

        if(_preferences->getBool(preference_retain_gpio, false) != retain)
        {
            _preferences->putBool(preference_retain_gpio, retain);
        }
    }

#if SOC_RMT_SUPPORTED
    if(request->hasParam("LEDBRI"))
    {
        const PsychicWebParameter* p = request->getParam("LEDBRI");

        if(p->value().length() > 0)
        {
            const int brightness = constrain(p->value().toInt(), 1, 100);

            if(_preferences->getInt(preference_status_led_brightness, 20) != brightness)
            {
                _preferences->putInt(preference_status_led_brightness, brightness);
            }
        }
    }

    if(request->hasParam("LEDORD"))
    {
        const PsychicWebParameter* p = request->getParam("LEDORD");
        int order = p->value().toInt();

        if(order < 0 || order > 5)
        {
            order = 0;
        }
        if(_preferences->getInt(preference_status_led_order, 0) != order)
        {
            _preferences->putInt(preference_status_led_order, order);
        }
    }

    if(request->hasParam("LEDQUIET"))
    {
        const bool quiet = isParameterTrue(request, "LEDQUIET");

        if(_preferences->getBool(preference_status_led_quiet, false) != quiet)
        {
            _preferences->putBool(preference_status_led_quiet, quiet);
        }
    }
#endif

    bool statusLedAssigned = false;

    for (int i=0; i < 100; i++)
    {
        String gpio = String(i);
        if (!request->hasParam(gpio.c_str())) continue;
        const PsychicWebParameter* p = request->getParam(gpio.c_str());

        PinRole role = (PinRole)p->value().toInt();
        if(role == PinRole::OutputWs2812StatusLed)
        {
            if(statusLedAssigned)
            {
                Log->print("Only one WS2812 status LED is supported, ignoring GPIO ");
                Log->println(i);
                continue;
            }
            statusLedAssigned = true;
        }
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
