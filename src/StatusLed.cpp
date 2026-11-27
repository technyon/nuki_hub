#include "StatusLed.h"
#include <cstring>
#include "Arduino.h"
#include "Logger.h"
#include "PreferencesKeys.h"
#include "esp_system.h"
#include "esp_rom_sys.h"
#include "freertos/task.h"
#if SOC_RMT_SUPPORTED
#include "esp32-hal-periman.h"
#endif

namespace
{
const char* const kOrderNames[6] = { "GRB", "RGB", "BRG", "RBG", "GBR", "BGR" };

struct TriggerInfo
{
    const char* id;
    const char* name;
    bool event;
    const char* applies; // for the web page, where the name alone doesn't say it all
};

// Indexed by StatusLed::Trigger. The ids are stored in the preferences, so never change them.
const TriggerInfo kTriggers[] =
{
    { "update", "Hub: firmware update", false, "While Nuki Hub downloads the updater, the first part of a firmware update." },
    { "ap", "Hub: Wi-Fi setup access point open", false, "While the access point for setting up Wi-Fi is open." },
    { "netdown", "Hub: no network", false, "While there is no Wi-Fi or Ethernet connection, once starting is over (at most 60 s after a restart)." },
    { "unpaired", "Hub: lock or opener not paired", false, "While an enabled lock or opener isn't paired." },
    { "starting", "Hub: starting", false, "After a restart, until the network and MQTT are connected (at most 60 s)." },
    { "mqttdown", "Hub: no MQTT connection", false, "While the network is connected but the MQTT broker isn't." },
    { "noresponse", "Hub: lock or opener not responding", false, "While the lock or opener doesn't respond, even after retrying." },
    { "ble", "Hub: Bluetooth communication", false, "During Bluetooth communication with the lock or opener, at least 0.25 s each time." },
    { "locked", "Lock: locked", false, "While the lock state is \"locked\"." },
    { "unlocked", "Lock: unlocked", false, "While the lock state is \"unlocked\", \"unlatched\" or \"unlocked (Lock 'n' Go)\"." },
    { "motor", "Lock: motor running", false, "While the lock state is \"locking\", \"unlocking\" or \"unlatching\"." },
    { "blocked", "Lock: motor blocked", false, "While the lock state is \"motor blocked\"." },
    { "dooropen", "Lock: door open", false, "While the door sensor reports the door as open." },
    { "lockbattery", "Lock: battery critical", false, "While the lock reports its battery as critical." },
    { "accbattery", "Keypad or sensor: battery critical", false, "While the lock or opener reports the battery of its keypad or door sensor as critical." },
    { "rto", "Opener: Ring to Open active", false, "While the opener state is \"Ring to Open active\"." },
    { "cm", "Opener: continuous mode", false, "While the opener is in continuous mode." },
    { "opening", "Opener: opening the door", false, "While the opener state is \"open\" or \"opening\"." },
    { "ring", "Opener: doorbell rang", true, "When the opener detects a ring. This is a moment, so it only works with \"for the first\"." },
    { "openerbattery", "Opener: battery critical", false, "While the opener reports its battery as critical." },
};
static_assert(sizeof(kTriggers) / sizeof(kTriggers[0]) == (size_t)StatusLed::Trigger::Count, "one entry per trigger");

// Indexed by StatusLed::Timing
const char* const kTimingIds[] = { "while", "after", "for" };
static_assert(sizeof(kTimingIds) / sizeof(kTimingIds[0]) == (size_t)StatusLed::Timing::Count, "one entry per timing");

struct EffectInfo
{
    const char* id;
    const char* name;
    const char* rhythm;
};

// Indexed by StatusLed::Effect
const EffectInfo kEffects[] =
{
    { "off", "Dark", "LED off" },
    { "solid", "Solid", "" },
    { "pulse", "Pulse", "fades every 3 s" },
    { "blink", "Blink slow", "1 s on/off" },
    { "fastblink", "Blink fast", "0.25 s on/off" },
    { "flash1", "Flash 1×", "every 2 s" },
    { "flash2", "Flash 2×", "every 2.5 s" },
    { "flash3", "Flash 3×", "every 3 s" },
};
static_assert(sizeof(kEffects) / sizeof(kEffects[0]) == (size_t)StatusLed::Effect::Count, "one entry per effect");

// The fixed states the LED showed before it had rules, in the same order and colors
const char* const kDefaultRules =
    "update,while,0,solid,ffffff;"
    "ap,while,0,blink,0000ff;"
    "netdown,after,5,flash1,ff0000;"
    "unpaired,while,0,blink,ff00ff;"
    "starting,while,0,solid,ffffff;"
    "mqttdown,after,5,flash2,ff6400;"
    "noresponse,while,0,flash3,ff00ff;"
    "ble,while,0,solid,00ffff;"
    "*,while,0,solid,00ff00";

// The same rules with the states that need no attention switched off, plus the problems of the lock, its keypad or
// door sensor and the opener. The "update" and "starting" rules stay, so they still hide the states below them, for
// example a slow MQTT connection while starting. The page leaves out the rules of a device that isn't enabled.
const char* const kQuietRules =
    "update,while,0,off,000000;"
    "ap,while,0,blink,0000ff;"
    "netdown,after,5,flash1,ff0000;"
    "unpaired,while,0,blink,ff00ff;"
    "starting,while,0,off,000000;"
    "mqttdown,after,5,flash2,ff6400;"
    "noresponse,while,0,flash3,ff00ff;"
    "blocked,while,0,fastblink,ff0000;"
    "lockbattery,while,0,flash1,ffcc00;"
    "accbattery,while,0,flash2,ffcc00;"
    "openerbattery,while,0,flash3,ffcc00;"
    "*,while,0,off,000000";

bool equalsId(const char* text, size_t length, const char* id)
{
    return strlen(id) == length && strncmp(text, id, length) == 0;
}

bool triggerFromId(const char* text, size_t length, StatusLed::Trigger& trigger)
{
    for(size_t i = 0; i < (size_t)StatusLed::Trigger::Count; i++)
    {
        if(equalsId(text, length, kTriggers[i].id))
        {
            trigger = (StatusLed::Trigger)i;
            return true;
        }
    }
    return false;
}

bool timingFromId(const char* text, size_t length, StatusLed::Timing& timing)
{
    for(size_t i = 0; i < (size_t)StatusLed::Timing::Count; i++)
    {
        if(equalsId(text, length, kTimingIds[i]))
        {
            timing = (StatusLed::Timing)i;
            return true;
        }
    }
    return false;
}

bool effectFromText(const char* text, size_t length, StatusLed::Effect& effect)
{
    for(size_t i = 0; i < (size_t)StatusLed::Effect::Count; i++)
    {
        if(equalsId(text, length, kEffects[i].id))
        {
            effect = (StatusLed::Effect)i;
            return true;
        }
    }
    return false;
}

bool secondsFromText(const char* text, size_t length, uint16_t& seconds)
{
    if(length == 0 || length > 5)
    {
        return false;
    }
    uint32_t value = 0;
    for(size_t i = 0; i < length; i++)
    {
        if(text[i] < '0' || text[i] > '9')
        {
            return false;
        }
        value = value * 10 + (uint32_t)(text[i] - '0');
    }
    if(value > 65535)
    {
        return false;
    }
    seconds = (uint16_t)value;
    return true;
}

bool colorFromText(const char* text, size_t length, uint32_t& color)
{
    if(length != 6)
    {
        return false;
    }
    uint32_t value = 0;
    for(size_t i = 0; i < length; i++)
    {
        const char c = text[i];
        uint32_t digit;
        if(c >= '0' && c <= '9')
        {
            digit = (uint32_t)(c - '0');
        }
        else if(c >= 'a' && c <= 'f')
        {
            digit = (uint32_t)(c - 'a' + 10);
        }
        else if(c >= 'A' && c <= 'F')
        {
            digit = (uint32_t)(c - 'A' + 10);
        }
        else
        {
            return false;
        }
        value = (value << 4) | digit;
    }
    color = value;
    return true;
}

#if SOC_RMT_SUPPORTED
// For each color order: which of {r, g, b} goes on the wire first, second and third
const uint8_t kOrder[6][3] = { {1, 0, 2}, {0, 1, 2}, {2, 0, 1}, {0, 2, 1}, {1, 2, 0}, {2, 1, 0} };

constexpr uint32_t kTickMs = 50;
constexpr uint32_t kSelfTestStepMs = 400;
constexpr uint32_t kSelfTestMs = 3 * kSelfTestStepMs;
constexpr uint32_t kStartingMaxMs = 60000;
constexpr uint32_t kBleMinMs = 250;
constexpr uint32_t kRefreshMs = 5000;
// Longer than any rule can ask for (65535 s), and far below the 49 days after which millis() wraps
constexpr uint32_t kMaxSpanMs = 65536000;
constexpr size_t kSymbols = 24;

// What the LED shows, besides a rule index
constexpr int8_t kShownOtherwise = -1;
constexpr int8_t kShownSelfTest = -2;
constexpr int8_t kShownTest = -3;

// Bits of the lock and opener flags
constexpr uint16_t kFlagKnown = 1 << 0;
constexpr uint16_t kFlagBatteryCritical = 1 << 1;
constexpr uint16_t kFlagAccessoryBatteryCritical = 1 << 2;
constexpr uint16_t kLockLocked = 1 << 3;
constexpr uint16_t kLockUnlocked = 1 << 4;
constexpr uint16_t kLockMotorRunning = 1 << 5;
constexpr uint16_t kLockMotorBlocked = 1 << 6;
constexpr uint16_t kLockDoorOpen = 1 << 7;
constexpr uint16_t kOpenerRingToOpen = 1 << 3;
constexpr uint16_t kOpenerContinuousMode = 1 << 4;
constexpr uint16_t kOpenerOpening = 1 << 5;

StatusLed* s_instance = nullptr;

// n flashes of 150 ms on / 350 ms off, then 1500 ms off
bool flashOn(uint32_t phase, uint32_t n)
{
    const uint32_t t = phase % (n * 500 + 1500);
    return t < n * 500 && (t % 500) < 150;
}

// 0 (dark) to 255 (full) for the given time into the effect
uint8_t effectLevel(StatusLed::Effect effect, uint32_t phase)
{
    switch(effect)
    {
        case StatusLed::Effect::Solid:
            return 255;
        case StatusLed::Effect::Pulse:
        {
            // Squaring the ramp makes the fade look even to the eye. It never goes fully dark, so it doesn't look
            // like a blink.
            const uint32_t t = phase % 3000;
            const uint32_t ramp = t < 1500 ? t : 3000 - t;
            return (uint8_t)(10 + (245 * ramp * ramp) / (1500 * 1500));
        }
        case StatusLed::Effect::BlinkSlow:
            return (phase % 2000) < 1000 ? 255 : 0;
        case StatusLed::Effect::BlinkFast:
            return (phase % 500) < 250 ? 255 : 0;
        case StatusLed::Effect::Flash1:
            return flashOn(phase, 1) ? 255 : 0;
        case StatusLed::Effect::Flash2:
            return flashOn(phase, 2) ? 255 : 0;
        case StatusLed::Effect::Flash3:
            return flashOn(phase, 3) ? 255 : 0;
        case StatusLed::Effect::Off:
        default:
            return 0;
    }
}

// Scales a color channel by the effect level (0-255) and the brightness (1-100 %), keeping a lit channel lit
uint8_t scaleChannel(uint8_t c, uint8_t level, uint8_t brightness)
{
    if(c == 0 || level == 0)
    {
        return 0;
    }
    const uint32_t v = ((uint32_t)c * level * brightness + 12750) / 25500;
    return (uint8_t)(v ? v : 1);
}
#endif
}

bool StatusLed::parseRules(const char* text, RuleSet& rules)
{
    RuleSet result;
    bool usable = false;
    const char* entry = text != nullptr ? text : "";

    while(*entry != '\0')
    {
        const char* separator = strchr(entry, ';');
        const size_t length = separator != nullptr ? (size_t)(separator - entry) : strlen(entry);

        // Split into exactly five fields: trigger, timing, seconds, effect, color
        const char* fields[5];
        size_t lengths[5];
        size_t count = 0;
        const char* field = entry;
        const char* end = entry + length;
        bool tooMany = false;
        while(true)
        {
            const char* comma = (const char*)memchr(field, ',', (size_t)(end - field));
            const char* fieldEnd = comma != nullptr ? comma : end;
            if(count == 5)
            {
                tooMany = true;
                break;
            }
            const char* first = field;
            const char* last = fieldEnd;
            while(first < last && *first == ' ')
            {
                first++;
            }
            while(last > first && last[-1] == ' ')
            {
                last--;
            }
            fields[count] = first;
            lengths[count] = (size_t)(last - first);
            count++;
            if(comma == nullptr)
            {
                break;
            }
            field = comma + 1;
        }

        Effect effect;
        uint32_t color;
        if(!tooMany && count == 5 && effectFromText(fields[3], lengths[3], effect) && colorFromText(fields[4], lengths[4], color))
        {
            Trigger trigger;
            Timing timing;
            uint16_t seconds;
            if(equalsId(fields[0], lengths[0], "*"))
            {
                result.otherwiseEffect = effect;
                result.otherwiseColor = color;
                usable = true;
            }
            else if(result.count < kMaxRules &&
                    triggerFromId(fields[0], lengths[0], trigger) &&
                    timingFromId(fields[1], lengths[1], timing) &&
                    secondsFromText(fields[2], lengths[2], seconds))
            {
                if(triggerIsEvent(trigger))
                {
                    timing = Timing::For;
                    if(seconds == 0)
                    {
                        seconds = 10;
                    }
                }
                else if(timing != Timing::While && seconds == 0)
                {
                    timing = Timing::While;
                }
                if(timing == Timing::While)
                {
                    seconds = 0;
                }

                Rule& rule = result.rules[result.count++];
                rule.trigger = trigger;
                rule.timing = timing;
                rule.seconds = seconds;
                rule.effect = effect;
                rule.color = color;
                usable = true;
            }
        }

        entry = separator != nullptr ? separator + 1 : end;
    }

    if(usable)
    {
        rules = result;
    }
    return usable;
}

String StatusLed::formatRules(const RuleSet& rules)
{
    String text;
    char entry[64];
    text.reserve(rules.count * 40 + 32);

    for(uint8_t i = 0; i < rules.count && i < kMaxRules; i++)
    {
        const Rule& rule = rules.rules[i];
        snprintf(entry, sizeof(entry), "%s,%s,%u,%s,%06lx;", triggerId(rule.trigger), timingId(rule.timing), (unsigned)rule.seconds,
                 effectId(rule.effect), (unsigned long)(rule.color & 0xFFFFFF));
        text += entry;
    }
    snprintf(entry, sizeof(entry), "*,while,0,%s,%06lx", effectId(rules.otherwiseEffect), (unsigned long)(rules.otherwiseColor & 0xFFFFFF));
    text += entry;
    return text;
}

const char* StatusLed::defaultRules()
{
    return kDefaultRules;
}

const char* StatusLed::quietRules()
{
    return kQuietRules;
}

const char* StatusLed::triggerId(Trigger trigger)
{
    return (size_t)trigger < (size_t)Trigger::Count ? kTriggers[(size_t)trigger].id : "";
}

const char* StatusLed::triggerName(Trigger trigger)
{
    return (size_t)trigger < (size_t)Trigger::Count ? kTriggers[(size_t)trigger].name : "";
}

bool StatusLed::triggerIsEvent(Trigger trigger)
{
    return (size_t)trigger < (size_t)Trigger::Count && kTriggers[(size_t)trigger].event;
}

const char* StatusLed::triggerApplies(Trigger trigger)
{
    return (size_t)trigger < (size_t)Trigger::Count ? kTriggers[(size_t)trigger].applies : "";
}

const char* StatusLed::timingId(Timing timing)
{
    return (size_t)timing < (size_t)Timing::Count ? kTimingIds[(size_t)timing] : "";
}

const char* StatusLed::effectId(Effect effect)
{
    return (size_t)effect < (size_t)Effect::Count ? kEffects[(size_t)effect].id : "";
}

const char* StatusLed::effectName(Effect effect)
{
    return (size_t)effect < (size_t)Effect::Count ? kEffects[(size_t)effect].name : "";
}

const char* StatusLed::effectRhythm(Effect effect)
{
    return (size_t)effect < (size_t)Effect::Count ? kEffects[(size_t)effect].rhythm : "";
}

uint8_t StatusLed::previewSeconds(Effect effect)
{
    // A flash effect needs two full rounds before its number of flashes can be told apart
    switch(effect)
    {
        case Effect::Flash1:
            return 4;
        case Effect::Flash2:
            return 5;
        case Effect::Flash3:
            return 6;
        default:
            return 3;
    }
}

bool StatusLed::effectFromId(const char* id, Effect& effect)
{
    return id != nullptr && effectFromText(id, strlen(id), effect);
}

bool StatusLed::configured() const
{
    return _configured;
}

bool StatusLed::running() const
{
    return _running.load();
}

uint8_t StatusLed::pin() const
{
    return _pin;
}

uint8_t StatusLed::brightness() const
{
    return _brightness.load();
}

uint8_t StatusLed::order() const
{
    return _order.load();
}

const char* StatusLed::orderName() const
{
    const uint8_t order = _order.load();
    return kOrderNames[order < 6 ? order : 0];
}

void StatusLed::applySettings(uint8_t brightness, uint8_t order, const RuleSet& rules)
{
    _brightness.store(brightness < 1 ? 1 : (brightness > 100 ? 100 : brightness));
    _order.store(order <= 5 ? order : 0);
    portENTER_CRITICAL(&_mux);
    _rules = rules;
    portEXIT_CRITICAL(&_mux);
    _rulesVersion.fetch_add(1);
}

StatusLed::RuleSet StatusLed::rules() const
{
    RuleSet copy;
    portENTER_CRITICAL(&_mux);
    copy = _rules;
    portEXIT_CRITICAL(&_mux);
    return copy;
}

#if SOC_RMT_SUPPORTED

void StatusLed::begin(uint8_t pin, Preferences* preferences, bool selfTest)
{
    if(_configured)
    {
        return;
    }
    _configured = true;
    _pin = pin;

    RuleSet rules;
    bool rulesLoaded = false;
    if(preferences->isKey(preference_status_led_rules))
    {
        const String text = preferences->getString(preference_status_led_rules, "");
        rulesLoaded = parseRules(text.c_str(), rules);
        if(!rulesLoaded)
        {
            Log->println("WS2812 status LED: no usable rule saved, using the default rules");
        }
    }
    if(!rulesLoaded)
    {
        parseRules(kDefaultRules, rules);
    }
    const int savedBrightness = preferences->getInt(preference_status_led_brightness, 20);
    const int savedOrder = preferences->getInt(preference_status_led_order, 0);
    applySettings((uint8_t)(savedBrightness < 1 ? 1 : (savedBrightness > 100 ? 100 : savedBrightness)),
                  (uint8_t)(savedOrder >= 0 && savedOrder <= 5 ? savedOrder : 0), rules);
    _selfTestPending = selfTest;

    if(!rmtInit(pin, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000))
    {
        Log->print("WS2812 status LED: RMT setup failed on GPIO ");
        Log->println(pin);
        return;
    }

    _timer = xTimerCreate("statusLed", pdMS_TO_TICKS(kTickMs), pdTRUE, this, &StatusLed::onTimer);
    if(_timer == nullptr || xTimerStart(_timer, 0) != pdPASS)
    {
        if(_timer != nullptr)
        {
            xTimerDelete(_timer, 0);
            _timer = nullptr;
        }
        rmtDeinit(pin);
        Log->println("WS2812 status LED: could not start the timer");
        return;
    }

    s_instance = this;
    _running.store(true);

    if(esp_register_shutdown_handler(&StatusLed::onShutdown) != ESP_OK)
    {
        Log->println("WS2812 status LED: no free shutdown handler, the LED keeps its color during restarts");
    }

    Log->print("WS2812 status LED on GPIO ");
    Log->print(pin);
    Log->print(" (");
    Log->print(orderName());
    Log->print(", ");
    Log->print(brightness());
    Log->print(" %, ");
    Log->print(rules.count);
    Log->println(rules.count == 1 ? " rule)" : " rules)");
}

void StatusLed::preview(Effect effect, uint32_t color, uint8_t brightness, uint8_t order)
{
    if(!_running.load() || (size_t)effect >= (size_t)Effect::Count)
    {
        return;
    }
    portENTER_CRITICAL(&_mux);
    _previewEffect = effect;
    _previewColor = color & 0xFFFFFF;
    _previewBrightness = brightness < 1 ? 1 : (brightness > 100 ? 100 : brightness);
    _previewOrder = order <= 5 ? order : 0;
    portEXIT_CRITICAL(&_mux);
    _previewSeq.fetch_add(1);
}

void StatusLed::reportNetwork(bool apOpen, bool connected, bool mqttConnected, bool mqttExpected)
{
    if(!_running.load())
    {
        return;
    }
    // Stored last and loaded first by the renderer, so a renderer that sees the new network state also sees the
    // MQTT state of the same report
    _mqttExpected.store(mqttExpected);
    _mqttConnected.store(mqttConnected);
    _apOpen.store(apOpen);
    _networkConnected.store(connected);
}

void StatusLed::reportPaired(Device device, bool paired)
{
    if(!_running.load())
    {
        return;
    }
    _notPaired[(uint8_t)device & 1].store(!paired);
}

void StatusLed::reportReachable(Device device, bool reachable)
{
    if(!_running.load())
    {
        return;
    }
    _unreachable[(uint8_t)device & 1].store(!reachable);
}

void StatusLed::reportBleActivity(bool active)
{
    if(!_running.load())
    {
        return;
    }
    if(active)
    {
        // Order matters: the renderer reads _bleStartMs only after it saw _bleSeen
        _bleStartMs.store((uint32_t)millis());
        _bleSeen.store(true);
    }
    _bleBusy.store(active);
}

void StatusLed::reportLockStatus(const LockStatus& status)
{
    if(!_running.load())
    {
        return;
    }
    uint16_t flags = 0;
    if(status.known)
    {
        flags = kFlagKnown |
                (status.batteryCritical ? kFlagBatteryCritical : 0) |
                (status.accessoryBatteryCritical ? kFlagAccessoryBatteryCritical : 0) |
                (status.locked ? kLockLocked : 0) |
                (status.unlocked ? kLockUnlocked : 0) |
                (status.motorRunning ? kLockMotorRunning : 0) |
                (status.motorBlocked ? kLockMotorBlocked : 0) |
                (status.doorOpen ? kLockDoorOpen : 0);
    }
    _lockFlags.store(flags);
}

void StatusLed::reportOpenerStatus(const OpenerStatus& status)
{
    if(!_running.load())
    {
        return;
    }
    uint16_t flags = 0;
    if(status.known)
    {
        flags = kFlagKnown |
                (status.batteryCritical ? kFlagBatteryCritical : 0) |
                (status.accessoryBatteryCritical ? kFlagAccessoryBatteryCritical : 0) |
                (status.ringToOpen ? kOpenerRingToOpen : 0) |
                (status.continuousMode ? kOpenerContinuousMode : 0) |
                (status.opening ? kOpenerOpening : 0);
    }
    _openerFlags.store(flags);
}

void StatusLed::reportRing()
{
    if(!_running.load())
    {
        return;
    }
    _ringCount.fetch_add(1);
}

void StatusLed::setUpdating()
{
    if(!_running.load())
    {
        return;
    }
    _updating.store(true);
}

uint32_t StatusLed::writeErrors() const
{
    return _writeErrors.load();
}

String StatusLed::shownText(bool withRuleNumber) const
{
    if(!_configured)
    {
        return "not configured";
    }
    if(!_running.load())
    {
        return "failed to start";
    }
    const uint32_t shown = _shown.load();
    if(shown == 0xFFFFFFFF)
    {
        return "Starting";
    }
    const int8_t index = (int8_t)(shown & 0xFF);
    const Trigger trigger = (Trigger)((shown >> 8) & 0xFF);
    const Effect effect = (Effect)((shown >> 16) & 0xFF);

    if(index == kShownSelfTest)
    {
        return "Self-test";
    }
    if(index == kShownTest)
    {
        return String("Test, ") + effectName(effect);
    }
    if(index < 0)
    {
        return String("Otherwise, ") + effectName(effect);
    }
    String text = String(triggerName(trigger)) + ", " + effectName(effect);
    if(withRuleNumber)
    {
        text = String("Rule ") + String(index + 1) + ": " + text;
    }
    return text;
}

int StatusLed::shownRule() const
{
    const uint32_t shown = _shown.load();
    if(!_configured || !_running.load() || shown == 0xFFFFFFFF)
    {
        return -1;
    }
    const int8_t index = (int8_t)(shown & 0xFF);
    if(index == kShownOtherwise)
    {
        return 0;
    }
    return index < 0 ? -1 : index + 1;
}

void StatusLed::onTimer(TimerHandle_t timer)
{
    static_cast<StatusLed*>(pvTimerGetTimerID(timer))->render();
}

// Runs in the timer service task (priority 1, small stack): never log, block or delay here
void StatusLed::render()
{
    if(_shutdown.load())
    {
        return;
    }
    if(_writing.exchange(true))
    {
        return;
    }
    if(_shutdown.load())
    {
        _writing.store(false);
        return;
    }

    const uint32_t now = (uint32_t)millis();

    const uint32_t version = _rulesVersion.load();
    if(version != _activeVersion)
    {
        portENTER_CRITICAL(&_mux);
        _active = _rules;
        portEXIT_CRITICAL(&_mux);
        _activeVersion = version;
    }

    const uint32_t previewSeq = _previewSeq.load();
    if(previewSeq != _previewSeen)
    {
        portENTER_CRITICAL(&_mux);
        _previewShown = _previewEffect;
        _previewShownColor = _previewColor;
        _previewShownBrightness = _previewBrightness;
        _previewShownOrder = _previewOrder;
        portEXIT_CRITICAL(&_mux);
        _previewSeen = previewSeq;
        _previewing = true;
        _previewStart = now;
    }
    if(_previewing && now - _previewStart >= (uint32_t)previewSeconds(_previewShown) * 1000)
    {
        _previewing = false;
    }

    // Tracked on every tick, so a rule's delay starts when its trigger starts, whatever is shown meanwhile
    updateTriggers(now);

    if(_selfTestPending)
    {
        if(!_selfTestStarted)
        {
            _selfTestStarted = true;
            _selfTestStart = now;
        }
        if(now - _selfTestStart >= kSelfTestMs)
        {
            _selfTestPending = false;
        }
    }

    int8_t index = kShownOtherwise;
    Trigger trigger = Trigger::Count;
    Effect effect = _active.otherwiseEffect;
    uint32_t color = _active.otherwiseColor;

    if(_selfTestPending)
    {
        index = kShownSelfTest;
        effect = Effect::Solid;
    }
    else if(_previewing)
    {
        index = kShownTest;
        effect = _previewShown;
        color = _previewShownColor;
    }
    else
    {
        for(uint8_t i = 0; i < _active.count && i < kMaxRules; i++)
        {
            const Rule& rule = _active.rules[i];
            if(ruleApplies(rule, now))
            {
                index = (int8_t)i;
                trigger = rule.trigger;
                effect = rule.effect;
                color = rule.color;
                break;
            }
        }
    }

    // A different rule, effect or test starts its pattern from the beginning
    uint32_t key = (uint32_t)(uint8_t)index | ((uint32_t)(uint8_t)trigger << 8) | ((uint32_t)(uint8_t)effect << 16);
    if(index == kShownTest)
    {
        key |= (_previewSeen & 0xFF) << 24;
    }
    if(key != _currentKey)
    {
        _currentKey = key;
        _phaseStart = now;
        _shown.store(key & 0x00FFFFFF);
    }

    // A test uses the brightness and color order on the web page, so they can be tried before saving
    const bool testing = index == kShownTest;
    const uint8_t brightness = testing ? _previewShownBrightness : _brightness.load();
    const uint8_t order = testing ? _previewShownOrder : _order.load();

    Rgb rgb;
    if(index == kShownSelfTest)
    {
        rgb = selfTestColor(now - _selfTestStart);
        rgb = { scaleChannel(rgb.r, 255, brightness), scaleChannel(rgb.g, 255, brightness), scaleChannel(rgb.b, 255, brightness) };
    }
    else
    {
        const uint8_t level = effectLevel(effect, now - _phaseStart);
        rgb = { scaleChannel((uint8_t)(color >> 16), level, brightness),
                scaleChannel((uint8_t)(color >> 8), level, brightness),
                scaleChannel((uint8_t)color, level, brightness) };
    }

    // A new color order changes the frame without changing the color
    const bool changed = rgb.r != _last.r || rgb.g != _last.g || rgb.b != _last.b || order != _lastOrder;

    // Rewrite periodically even without a change, which repairs a frame that was corrupted on the wire
    if(!_everWritten || changed || now - _lastWrite >= kRefreshMs)
    {
        rmt_data_t* symbols = _symbols[_buffer];
        encode(rgb, order, symbols);

        // rmtWriteAsync() logs an error when the pin is not attached to RMT, so check first
        if(perimanGetPinBusType(_pin) == ESP32_BUS_TYPE_RMT_TX && rmtWriteAsync(_pin, symbols, kSymbols))
        {
            _last = rgb;
            _lastOrder = order;
            _lastWrite = now;
            _everWritten = true;
            _buffer ^= 1;
        }
        else
        {
            // Retried on the next tick because _last is unchanged
            _writeErrors.store(_writeErrors.load() + 1);
        }
    }

    _writing.store(false);
}

void StatusLed::updateTriggers(uint32_t now)
{
    const bool networkConnected = _networkConnected.load();
    const bool apOpen = _apOpen.load();
    const bool mqttConnected = _mqttConnected.load();
    const bool mqttExpected = _mqttExpected.load();

    // Starting ends for good at the first time everything is ready, or after 60 s
    if(!_startingDone && ((networkConnected && (mqttConnected || !mqttExpected)) || now >= kStartingMaxMs))
    {
        _startingDone = true;
    }

    // A device that isn't paired has no state; one that isn't enabled never reports one
    const bool lockPaired = !_notPaired[(uint8_t)Device::Lock].load();
    const bool openerPaired = !_notPaired[(uint8_t)Device::Opener].load();
    const uint16_t lock = lockPaired ? _lockFlags.load() : 0;
    const uint16_t opener = openerPaired ? _openerFlags.load() : 0;
    const bool lockKnown = (lock & kFlagKnown) != 0;
    const bool openerKnown = (opener & kFlagKnown) != 0;

    bool known[kTriggerCount] = {};
    bool active[kTriggerCount] = {};
    auto set = [&](Trigger trigger, bool isKnown, bool isActive)
    {
        known[(uint8_t)trigger] = isKnown;
        active[(uint8_t)trigger] = isKnown && isActive;
    };

    set(Trigger::FirmwareUpdate, true, _updating.load());
    set(Trigger::AccessPoint, true, apOpen);
    // Tracked from the moment the network is lost, but rules only see it once starting has finished, see ruleApplies()
    set(Trigger::NetworkDown, true, !networkConnected);
    set(Trigger::NotPaired, true, !lockPaired || !openerPaired);
    set(Trigger::Starting, true, !_startingDone);
    set(Trigger::MqttDown, true, networkConnected && mqttExpected && !mqttConnected);
    set(Trigger::NotResponding, true, _unreachable[0].load() || _unreachable[1].load());
    set(Trigger::Bluetooth, true, _bleBusy.load() || (_bleSeen.load() && now - _bleStartMs.load() < kBleMinMs));
    set(Trigger::LockLocked, lockKnown, (lock & kLockLocked) != 0);
    set(Trigger::LockUnlocked, lockKnown, (lock & kLockUnlocked) != 0);
    set(Trigger::LockMotorRunning, lockKnown, (lock & kLockMotorRunning) != 0);
    set(Trigger::LockMotorBlocked, lockKnown, (lock & kLockMotorBlocked) != 0);
    set(Trigger::DoorOpen, lockKnown, (lock & kLockDoorOpen) != 0);
    set(Trigger::LockBatteryCritical, lockKnown, (lock & kFlagBatteryCritical) != 0);
    set(Trigger::AccessoryBatteryCritical, lockKnown || openerKnown, ((lock | opener) & kFlagAccessoryBatteryCritical) != 0);
    set(Trigger::OpenerRingToOpen, openerKnown, (opener & kOpenerRingToOpen) != 0);
    set(Trigger::OpenerContinuousMode, openerKnown, (opener & kOpenerContinuousMode) != 0);
    set(Trigger::OpenerOpening, openerKnown, (opener & kOpenerOpening) != 0);
    set(Trigger::OpenerRing, true, false); // an event, see below
    set(Trigger::OpenerBatteryCritical, openerKnown, (opener & kFlagBatteryCritical) != 0);

    for(uint8_t t = 0; t < kTriggerCount; t++)
    {
        // Keep the times within what a rule can ask for, so an old start never looks recent after millis() wraps
        if(_triggerRisen[t] && now - _triggerRise[t] > kMaxSpanMs)
        {
            _triggerRisen[t] = false;
        }
        if(!known[t])
        {
            // The first state after the trigger becomes known again doesn't count as a change, so a "for" rule
            // doesn't fire because Nuki Hub started or the lock was paired
            _triggerActive[t] = false;
            _triggerPrimed[t] = false;
            continue;
        }
        if(active[t] && !_triggerActive[t])
        {
            _triggerSince[t] = now;
            if(_triggerPrimed[t])
            {
                _triggerRise[t] = now;
                _triggerRisen[t] = true;
            }
        }
        _triggerActive[t] = active[t];
        _triggerPrimed[t] = true;

        if(_triggerActive[t] && now - _triggerSince[t] > kMaxSpanMs)
        {
            _triggerSince[t] = now - kMaxSpanMs;
        }
    }

    const uint32_t rings = _ringCount.load();
    if(rings != _ringSeen)
    {
        _ringSeen = rings;
        _triggerRise[(uint8_t)Trigger::OpenerRing] = now;
        _triggerRisen[(uint8_t)Trigger::OpenerRing] = true;
    }
}

bool StatusLed::ruleApplies(const Rule& rule, uint32_t now) const
{
    const uint8_t t = (uint8_t)rule.trigger;
    if(t >= kTriggerCount)
    {
        return false;
    }
    // Like before rules existed, "no network" doesn't show while Nuki Hub is still connecting after a restart. An
    // "after" delay still counts from the moment the network was lost, so it shows as soon as starting ends.
    if(rule.trigger == Trigger::NetworkDown && !_startingDone)
    {
        return false;
    }
    const uint32_t span = (uint32_t)rule.seconds * 1000;
    switch(rule.timing)
    {
        case Timing::While:
            return _triggerActive[t];
        case Timing::After:
            return _triggerActive[t] && now - _triggerSince[t] >= span;
        case Timing::For:
            return _triggerRisen[t] && now - _triggerRise[t] < span;
        default:
            return false;
    }
}

StatusLed::Rgb StatusLed::selfTestColor(uint32_t phase) const
{
    if(phase < kSelfTestStepMs)
    {
        return { 255, 0, 0 };
    }
    if(phase < 2 * kSelfTestStepMs)
    {
        return { 0, 255, 0 };
    }
    return { 0, 0, 255 };
}

// WS2812 timing at 10 MHz (100 ns per tick): 1 = 800 ns high + 400 ns low, 0 = 400 ns high + 800 ns low, MSB first
void StatusLed::encode(const Rgb& rgb, uint8_t order, rmt_data_t* symbols)
{
    const uint8_t channels[3] = { rgb.r, rgb.g, rgb.b };
    const uint8_t* positions = kOrder[order < 6 ? order : 0];
    size_t i = 0;

    for(uint8_t position = 0; position < 3; position++)
    {
        const uint8_t value = channels[positions[position]];
        for(int bit = 7; bit >= 0; bit--)
        {
            const bool one = (value >> bit) & 1;
            symbols[i].level0 = 1;
            symbols[i].duration0 = one ? 8 : 4;
            symbols[i].level1 = 0;
            symbols[i].duration1 = one ? 4 : 8;
            i++;
        }
    }
}

// Runs inside esp_restart() in the restarting task, while the scheduler still runs. Panic, watchdog and brownout
// resets skip it. Switching the LED off here keeps it dark during the restart, because the LED latches its color.
void StatusLed::onShutdown()
{
    StatusLed* self = s_instance;
    if(self == nullptr || !self->_running.load())
    {
        return;
    }
    if(xPortInIsrContext() || xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
    {
        return;
    }

    self->_shutdown.store(true);

    // Wait for a frame the renderer is writing right now; afterwards it stops for good
    bool owned = false;
    for(int i = 0; i < 20 && !owned; i++)
    {
        owned = !self->_writing.exchange(true);
        if(!owned)
        {
            vTaskDelay(1);
        }
    }
    if(!owned)
    {
        return;
    }

    rmt_data_t off[kSymbols];
    encode({ 0, 0, 0 }, self->_order.load(), off);
    for(int i = 0; i < 3; i++)
    {
        if(perimanGetPinBusType(self->_pin) == ESP32_BUS_TYPE_RMT_TX && rmtWrite(self->_pin, off, kSymbols, 10))
        {
            // The LED latches the frame after 280 us of low level
            esp_rom_delay_us(300);
            break;
        }
        vTaskDelay(1);
    }
    // _writing stays taken: no further frames until the reset
}

#else

void StatusLed::begin(uint8_t pin, Preferences*, bool)
{
    if(_configured)
    {
        return;
    }
    _configured = true;
    _pin = pin;
    Log->println("WS2812 status LED is not supported on this chip");
}

void StatusLed::preview(Effect, uint32_t, uint8_t, uint8_t)
{
}

void StatusLed::reportNetwork(bool, bool, bool, bool)
{
}

void StatusLed::reportPaired(Device, bool)
{
}

void StatusLed::reportReachable(Device, bool)
{
}

void StatusLed::reportBleActivity(bool)
{
}

void StatusLed::reportLockStatus(const LockStatus&)
{
}

void StatusLed::reportOpenerStatus(const OpenerStatus&)
{
}

void StatusLed::reportRing()
{
}

void StatusLed::setUpdating()
{
}

uint32_t StatusLed::writeErrors() const
{
    return 0;
}

String StatusLed::shownText(bool) const
{
    return "not supported on this chip";
}

int StatusLed::shownRule() const
{
    return -1;
}

#endif
