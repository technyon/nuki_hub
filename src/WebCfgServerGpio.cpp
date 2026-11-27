#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"

#ifndef NUKI_HUB_UPDATER
#if SOC_RMT_SUPPORTED
namespace
{
// GPIOs with the RGB LED of common boards, as in the table in the README, the one of the chip's own dev boards first
const std::vector<uint8_t>& boardLedPins()
{
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    static const std::vector<uint8_t> pins = { 48, 38, 47, 21, 35 };
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
    static const std::vector<uint8_t> pins = { 8 };
#elif defined(CONFIG_IDF_TARGET_ESP32C5)
    static const std::vector<uint8_t> pins = { 27 };
#elif defined(CONFIG_IDF_TARGET_ESP32)
    // The M5Stack Atom Lite. The ESP32's own dev boards have no RGB LED.
    static const std::vector<uint8_t> pins = { 27 };
#else
    static const std::vector<uint8_t> pins;
#endif
    return pins;
}

const char* boardLedLabel(uint8_t pin)
{
    const auto& pins = boardLedPins();
    const auto it = std::find(pins.begin(), pins.end(), pin);
    if(it == pins.end())
    {
        return "";
    }
#if defined(CONFIG_IDF_TARGET_ESP32)
    return " (RGB LED on some boards)";
#else
    return it == pins.begin() ? " (RGB LED on many dev boards)" : " (RGB LED on some boards)";
#endif
}
}
#endif

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
        String pinLabel = "Gpio " + pinStr;
#if SOC_RMT_SUPPORTED
        pinLabel += boardLedLabel(pin);
#endif
        bool disabled = std::find(disabledPins.begin(), disabledPins.end(), pin) != disabledPins.end();
        response.print("<tr><td>" + pinLabel + "</td><td><select class=\"gpioselect\" name=\"" + pinStr + "\" data-value=\"" + (disabled ? String("21") : getPreselectionForGpio(pin)) + "\"></select></td></tr>");
    }

    response.print("</table>");
#if SOC_RMT_SUPPORTED
    response.print("<p>Brightness, color order and what a WS2812 status LED shows are set on the <a href=\"/get?page=ledcfg\">Status LED</a> page.</p>");
#endif
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

#if SOC_RMT_SUPPORTED
namespace
{
// The page shows and edits what is saved, which is also what the LED runs once it is set up
StatusLed::RuleSet savedStatusLedRules(Preferences* preferences)
{
    StatusLed::RuleSet rules;
    if(preferences->isKey(preference_status_led_rules) &&
            StatusLed::parseRules(preferences->getString(preference_status_led_rules, "").c_str(), rules))
    {
        return rules;
    }
    StatusLed::parseRules(StatusLed::defaultRules(), rules);
    return rules;
}

bool parseColor(const String& text, uint32_t& color)
{
    if(text.length() != 6)
    {
        return false;
    }
    for(size_t i = 0; i < 6; i++)
    {
        if(!isxdigit((unsigned char)text[i]))
        {
            return false;
        }
    }
    color = (uint32_t)strtoul(text.c_str(), nullptr, 16);
    return true;
}
}

esp_err_t WebCfgServer::buildStatusLedConfigHtml(PsychicRequest *request, PsychicResponse* resp)
{
    const StatusLed& statusLed = _gpio->statusLed();
    const StatusLed::RuleSet rules = savedStatusLedRules(_preferences);

    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response, "", statusLed.running() ? "data-status=\"/get?page=ledstatus\"" : "", "led");
    response.print("<form method=\"post\" action=\"post\" id=\"ledform\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"saveledcfg\">");
    response.print("<h3>Status LED</h3>");

    if(!statusLed.configured())
    {
        response.print("<div class=\"banner warn\">No GPIO has the role \"Output: WS2812 RGB status LED\" yet. Set one on the <a href=\"/get?page=gpiocfg\">GPIO Configuration</a> page; the settings below are kept until then.</div>");
    }
    else if(!statusLed.running())
    {
        response.print("<div class=\"banner danger\">The status LED on GPIO ");
        response.print(statusLed.pin());
        response.print(" failed to start. Restart Nuki Hub. If it fails again, choose another GPIO on the <a href=\"/get?page=gpiocfg\">GPIO Configuration</a> page.</div>");
    }

    response.print("<table>");
    if(statusLed.running())
    {
        response.print("<tr><td>Now showing</td><td><span id=\"ledshowndot\" class=\"leddot\" hidden></span><span id=\"ledshown\">");
        response.print(htmlEscape(statusLed.shownText(false).c_str()));
        response.print("</span><span id=\"ledshowncolor\"></span><span id=\"ledshownwhy\" hidden> (no rule applies right now)</span><span id=\"ledstale\" class=\"ledstale\" hidden> (not updating, Nuki Hub doesn't answer)</span></td></tr>");
    }
    printInputField(&response, "LEDBRI", "Brightness (1-100 %)", _preferences->getInt(preference_status_led_brightness, 20), 3, "inputmode=\"numeric\" pattern=\"[1-9][0-9]?|100\" required title=\"A whole number from 1 to 100\" aria-label=\"Brightness (1-100 %)\"");
    const std::vector<std::pair<int, const char*>> ledOrderOptions =
    {
        { 0, "GRB (most boards)" },
        { 1, "RGB" },
        { 2, "BRG" },
        { 3, "RBG" },
        { 4, "GBR" },
        { 5, "BGR" }
    };
    const int savedOrder = _preferences->getInt(preference_status_led_order, 0);
    response.print("<tr><td id=\"ledorderlabel\">Color order</td><td><div class=\"ledorder\"><select name=\"LEDORD\" aria-labelledby=\"ledorderlabel\"");
    response.print(statusLed.running() ? " aria-describedby=\"ledorderhint ledcheckmsg\">" : ">");
    for(const auto& option : ledOrderOptions)
    {
        response.print(option.first == savedOrder ? "<option selected value=\"" : "<option value=\"");
        response.print(option.first);
        response.print("\">");
        response.print(option.second);
        response.print("</option>");
    }
    response.print("</select>");
    if(statusLed.running())
    {
        response.print("<button type=\"button\" id=\"ledcheck\" title=\"Shows red, green and blue on the LED, one after the other\">Check colors</button>");
    }
    response.print("</div>");
    if(statusLed.running())
    {
        // A wrong order shows green as red, and nothing else says that it is worth checking
        response.print("<div id=\"ledorderhint\" class=\"ledorderhint\">Boards differ in the order of red, green and blue: check it once.</div>");
    }
    response.print("</td></tr>");
    if(statusLed.running())
    {
        // Where else the LED can be, for when it stays dark during the check
        const auto& availablePins = _gpio->availablePins();
        const auto disabledPins = _gpio->getDisabledPins();
        String otherPins;
        for(const uint8_t pin : boardLedPins())
        {
            if(pin != statusLed.pin() && std::find(availablePins.begin(), availablePins.end(), pin) != availablePins.end() &&
                    std::find(disabledPins.begin(), disabledPins.end(), pin) == disabledPins.end())
            {
                if(otherPins.length() > 0)
                {
                    otherPins += ",";
                }
                otherPins += String(pin);
            }
        }
        response.print("<tr id=\"ledcheckrow\" hidden data-pins=\"");
        response.print(otherPins);
        response.print("\"><td colspan=\"2\"><div id=\"ledcheckmsg\" class=\"ledmsg\"></div></td></tr>");
    }
    response.print("</table>");

    response.print("<div class=\"ledruleshead\"><h3>Rules</h3><span class=\"ledadd\"><button type=\"button\" id=\"ledadd\">Add rule</button><span id=\"ledcount\" class=\"ledcount\"></span></span>");
    response.print("<details class=\"ledpresetbox\"><summary>Start from a preset</summary><div class=\"ledpresets\"><span class=\"ledpresetslabel\">Replace all rules with:</span>");
    response.print("<span class=\"ledpreset\"><button type=\"button\" data-led-preset=\"default\" aria-describedby=\"ledpresetdefault\">Default rules</button><span id=\"ledpresetdefault\">The hub's state, green when the hub is fine.</span></span>");
    response.print("<span class=\"ledpreset\"><button type=\"button\" data-led-preset=\"quiet\" aria-describedby=\"ledpresetquiet\">Problems only</button><span id=\"ledpresetquiet\">Dark unless the hub, lock or opener has a problem.</span></span>");
    response.print("</div></details></div>");
    response.print("<div id=\"ledrules\" class=\"ledrules\" data-default=\"");
    response.print(htmlEscape(StatusLed::defaultRules()));
    response.print("\" data-quiet=\"");
    response.print(htmlEscape(StatusLed::quietRules()));
    response.print("\" data-max=\"");
    response.print(StatusLed::kMaxRules);
    response.print("\" data-test=\"");
    response.print(statusLed.running() ? "1" : "0");
    response.print("\"><div class=\"ledhint\"><p>Rules apply from top to bottom: the first that applies is shown, or Otherwise if none does.</p>");
    response.print("<details><summary>More about how rules work</summary><ul>");
    response.print("<li><b>after</b> shows a rule once its state has lasted that long, so short drops don't show. <b>for the first</b> shows a rule only right after the state starts, for example for the doorbell.</li>");
    response.print("<li>A <b>Dark</b> rule still counts: the LED stays dark and the rules below it wait. To stop using a rule, remove it.</li>");
    if(statusLed.running())
    {
        response.print("<li>Changing <b>Show</b> or <b>Color</b> shows the rule on the LED for a few seconds, without saving. <b>Test</b> shows it again. Colors look different there than on screen.</li>");
    }
    response.print("</ul></details></div></div>");
    response.print("<input type=\"hidden\" name=\"LEDRULES\" id=\"ledrulesvalue\" autocomplete=\"off\" value=\"");
    response.print(htmlEscape(StatusLed::formatRules(rules).c_str()));
    response.print("\">");
    response.print("<div class=\"actions ledsave\" id=\"ledsave\"><input type=\"submit\" name=\"submit\" value=\"Save\"><span id=\"ledunsaved\" class=\"ledunsaved\" role=\"status\"></span><button type=\"button\" id=\"leddiscard\" hidden>Discard</button></div>");
    response.print("</form>");

    // Grouped by the part of the name before the colon. A group for a device that isn't enabled stays selectable,
    // because the rules can come from an import or from before the device was disabled.
    const bool lockEnabled = _preferences->getBool(preference_lock_enabled, true);
    const bool openerEnabled = _preferences->getBool(preference_opener_enabled, false);
    String group;
    response.print("<template id=\"ledtriggers\">");
    for(uint8_t i = 0; i < (uint8_t)StatusLed::Trigger::Count; i++)
    {
        const StatusLed::Trigger trigger = (StatusLed::Trigger)i;
        const String name = StatusLed::triggerName(trigger);
        const String prefix = name.substring(0, name.indexOf(':'));
        const char* off = nullptr;
        if(prefix == "Lock" && !lockEnabled)
        {
            off = "no lock is enabled";
        }
        else if(prefix == "Opener" && !openerEnabled)
        {
            off = "no opener is enabled";
        }
        else if(prefix == "Keypad or sensor" && !lockEnabled && !openerEnabled)
        {
            off = "no lock or opener is enabled";
        }
        if(prefix != group)
        {
            response.print(group.length() > 0 ? "</optgroup><optgroup label=\"" : "<optgroup label=\"");
            response.print(prefix);
            if(off != nullptr)
            {
                response.print(" (not enabled)");
            }
            response.print("\">");
            group = prefix;
        }
        response.print("<option value=\"");
        response.print(StatusLed::triggerId(trigger));
        response.print("\" data-applies=\"");
        response.print(htmlEscape(StatusLed::triggerApplies(trigger)));
        response.print("\"");
        if(StatusLed::triggerIsEvent(trigger))
        {
            response.print(" data-event=\"1\"");
        }
        if(off != nullptr)
        {
            response.print(" data-off=\"");
            response.print(off);
            response.print("\"");
        }
        response.print(">");
        response.print(htmlEscape(name.c_str()));
        response.print("</option>");
    }
    response.print("</optgroup></template>");

    // In three kinds, so that the eight effects read at a glance
    response.print("<template id=\"ledeffects\">");
    const char* kind = nullptr;
    for(uint8_t i = 0; i < (uint8_t)StatusLed::Effect::Count; i++)
    {
        const StatusLed::Effect effect = (StatusLed::Effect)i;
        const char* effectKind = effect <= StatusLed::Effect::Solid ? "Steady" : effect <= StatusLed::Effect::BlinkFast ? "Pulse and blink" : "Flashes";
        if(effectKind != kind)
        {
            response.print(kind != nullptr ? "</optgroup><optgroup label=\"" : "<optgroup label=\"");
            response.print(effectKind);
            response.print("\">");
            kind = effectKind;
        }
        response.print("<option value=\"");
        response.print(StatusLed::effectId(effect));
        response.print("\" data-seconds=\"");
        response.print(StatusLed::previewSeconds(effect));
        response.print("\">");
        response.print(htmlEscape(StatusLed::effectName(effect)));
        if(StatusLed::effectRhythm(effect)[0] != '\0')
        {
            response.print(" (");
            response.print(htmlEscape(StatusLed::effectRhythm(effect)));
            response.print(")");
        }
        response.print("</option>");
    }
    response.print("</optgroup></template>");

    buildHtmlFooter(&response);
    return response.endSend();
}

void WebCfgServer::processStatusLedArgs(PsychicRequest *request)
{
    int brightness = constrain(_preferences->getInt(preference_status_led_brightness, 20), 1, 100);
    if(request->hasParam("LEDBRI"))
    {
        const PsychicWebParameter* p = request->getParam("LEDBRI");

        if(p->value().length() > 0)
        {
            brightness = constrain(p->value().toInt(), 1, 100);

            if(_preferences->getInt(preference_status_led_brightness, 20) != brightness)
            {
                _preferences->putInt(preference_status_led_brightness, brightness);
            }
        }
    }

    int order = _preferences->getInt(preference_status_led_order, 0);
    if(order < 0 || order > 5)
    {
        order = 0;
    }
    if(request->hasParam("LEDORD"))
    {
        const PsychicWebParameter* p = request->getParam("LEDORD");
        order = p->value().toInt();

        if(order < 0 || order > 5)
        {
            order = 0;
        }
        if(_preferences->getInt(preference_status_led_order, 0) != order)
        {
            _preferences->putInt(preference_status_led_order, order);
        }
    }

    StatusLed::RuleSet rules = savedStatusLedRules(_preferences);
    if(request->hasParam("LEDRULES"))
    {
        const PsychicWebParameter* p = request->getParam("LEDRULES");
        StatusLed::RuleSet submitted;

        if(StatusLed::parseRules(p->value().c_str(), submitted))
        {
            rules = submitted;
            const String text = StatusLed::formatRules(rules);
            StatusLed::RuleSet defaults;
            StatusLed::parseRules(StatusLed::defaultRules(), defaults);

            // The default rules aren't stored, so they follow a later firmware that changes them
            if(text == StatusLed::formatRules(defaults))
            {
                if(_preferences->isKey(preference_status_led_rules))
                {
                    _preferences->remove(preference_status_led_rules);
                }
            }
            else if(!_preferences->isKey(preference_status_led_rules) || _preferences->getString(preference_status_led_rules, "") != text)
            {
                _preferences->putString(preference_status_led_rules, text);
            }
        }
        else
        {
            Log->println("Status LED: no usable rule submitted, keeping the saved rules");
        }
    }

    // Takes effect right away, no restart needed
    _gpio->statusLed().applySettings((uint8_t)brightness, (uint8_t)order, rules);
}

esp_err_t WebCfgServer::processStatusLedTest(PsychicRequest *request, PsychicResponse* resp)
{
    StatusLed& statusLed = _gpio->statusLed();
    StatusLed::Effect effect = StatusLed::Effect::Off;
    uint32_t color = 0;
    const bool valid = request->hasParam("effect") && request->hasParam("color") &&
                       StatusLed::effectFromId(request->getParam("effect")->value().c_str(), effect) &&
                       parseColor(request->getParam("color")->value(), color);

    // The brightness and color order on the page, read the way saving would read them, so they can be tried first
    int brightness = statusLed.brightness();
    if(request->hasParam("brightness") && request->getParam("brightness")->value().length() > 0)
    {
        brightness = constrain(request->getParam("brightness")->value().toInt(), 1, 100);
    }
    int order = statusLed.order();
    if(request->hasParam("order") && request->getParam("order")->value().length() > 0)
    {
        order = request->getParam("order")->value().toInt();

        if(order < 0 || order > 5)
        {
            order = 0;
        }
    }

    resp->addHeader("Cache-Control", "no-cache");
    resp->setContentType("text/plain");
    if(!valid)
    {
        resp->setCode(400);
        resp->setContent("Invalid effect or color");
        return resp->send();
    }
    if(!statusLed.running())
    {
        resp->setCode(409);
        resp->setContent("The status LED is not running");
        return resp->send();
    }

    statusLed.preview(effect, color, (uint8_t)brightness, (uint8_t)order);
    resp->setCode(200);
    resp->setContent("OK");
    return resp->send();
}

esp_err_t WebCfgServer::sendStatusLedState(PsychicRequest *request, PsychicResponse* resp)
{
    JsonDocument json;
    String jsonStr;
    json["ledshown"] = _gpio->statusLed().shownText(false);
    json["ledshownrule"] = _gpio->statusLed().shownRule();
    serializeJson(json, jsonStr);
    resp->addHeader("Cache-Control", "no-cache");
    resp->setCode(200);
    resp->setContentType("application/json");
    resp->setContent(jsonStr.c_str());
    return resp->send();
}
#endif

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
