#pragma once

#include <atomic>
#include <cstdint>
#include <Preferences.h>
#include "sdkconfig.h"
#include "soc/soc_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#if SOC_RMT_SUPPORTED
#include "esp32-hal-rmt.h"
#endif

// Drives one WS2812 / SK6812 RGB LED (for example the LED built into many ESP32-S3 dev boards) as a status light.
// What the LED shows is a list of rules: the first rule whose trigger applies decides the effect and color, and
// "otherwise" applies when no rule does.
// The LED is rendered by a FreeRTOS software timer, so the producers below only store flags: they never touch
// RMT, never log and never block. Each producer is called from exactly one task.
class StatusLed
{
public:
    enum class Device : uint8_t
    {
        Lock = 0,
        Opener = 1
    };

    // The order is the order of the web page's list. Rules are stored by id, so new triggers can go anywhere.
    enum class Trigger : uint8_t
    {
        FirmwareUpdate,
        AccessPoint,
        NetworkDown,
        NotPaired,
        Starting,
        MqttDown,
        NotResponding,
        Bluetooth,
        LockLocked,
        LockUnlocked,
        LockMotorRunning,
        LockMotorBlocked,
        DoorOpen,
        LockBatteryCritical,
        AccessoryBatteryCritical,
        OpenerRingToOpen,
        OpenerContinuousMode,
        OpenerOpening,
        OpenerRing,
        OpenerBatteryCritical,
        Count
    };

    enum class Timing : uint8_t
    {
        While, // while the trigger applies
        After, // once the trigger has applied for the given seconds, for as long as it keeps applying
        For,   // for the given seconds after the trigger starts to apply
        Count
    };

    enum class Effect : uint8_t
    {
        Off,
        Solid,
        Pulse,
        BlinkSlow,
        BlinkFast,
        Flash1,
        Flash2,
        Flash3,
        Count
    };

    static constexpr uint8_t kMaxRules = 16;

    struct Rule
    {
        Trigger trigger = Trigger::LockLocked;
        Timing timing = Timing::While;
        Effect effect = Effect::Off;
        uint16_t seconds = 0;
        uint32_t color = 0; // 0xRRGGBB
    };

    struct RuleSet
    {
        uint8_t count = 0;
        Rule rules[kMaxRules];
        Effect otherwiseEffect = Effect::Off;
        uint32_t otherwiseColor = 0;
    };

    // Set from the lock's key turner state, the same state the GPIO outputs follow
    struct LockStatus
    {
        bool known = false; // false while the lock state is undefined
        bool locked = false;
        bool unlocked = false; // unlocked, unlatched or Lock 'n' Go
        bool motorRunning = false; // locking, unlocking or unlatching
        bool motorBlocked = false;
        bool doorOpen = false;
        bool batteryCritical = false;
        bool accessoryBatteryCritical = false; // keypad or door sensor
    };

    struct OpenerStatus
    {
        bool known = false;
        bool ringToOpen = false;
        bool continuousMode = false;
        bool opening = false;
        bool batteryCritical = false;
        bool accessoryBatteryCritical = false; // keypad
    };

    // Rules as text, for the preferences, the JSON export and the web page: entries
    // "trigger,timing,seconds,effect,rrggbb" separated by ";", with trigger "*" for "otherwise".
    // Entries with unknown names or bad values are skipped. Returns false if no entry was usable.
    static bool parseRules(const char* text, RuleSet& rules);
    static String formatRules(const RuleSet& rules);
    static const char* defaultRules(); // what the LED showed before rules existed
    static const char* quietRules(); // only states that need attention, dark otherwise

    static const char* triggerId(Trigger trigger);
    static const char* triggerName(Trigger trigger);
    static bool triggerIsEvent(Trigger trigger); // only works with Timing::For
    static const char* triggerApplies(Trigger trigger); // when it applies, in a sentence for the web page
    static const char* timingId(Timing timing);
    static const char* effectId(Effect effect);
    static const char* effectName(Effect effect);
    static const char* effectRhythm(Effect effect); // empty for Solid
    static uint8_t previewSeconds(Effect effect); // how long Test shows it
    static bool effectFromId(const char* id, Effect& effect);

    // Called once while setting up the GPIOs (loop task)
    void begin(uint8_t pin, Preferences* preferences, bool selfTest);

    // Web server task: take effect immediately, without a restart
    void applySettings(uint8_t brightness, uint8_t order, const RuleSet& rules);
    // Shows the effect for previewSeconds(), with this brightness and color order, which may not be saved yet
    void preview(Effect effect, uint32_t color, uint8_t brightness, uint8_t order);
    RuleSet rules() const;

    void reportNetwork(bool apOpen, bool connected, bool mqttConnected, bool mqttExpected); // network task
    void reportPaired(Device device, bool paired); // nuki task
    void reportReachable(Device device, bool reachable); // nuki task
    void reportBleActivity(bool active); // nuki task
    void reportLockStatus(const LockStatus& status); // nuki task
    void reportOpenerStatus(const OpenerStatus& status); // nuki task
    void reportRing(); // nuki task
    void setUpdating(); // loop task, before the OTA task starts

    bool configured() const;
    bool running() const;
    uint8_t pin() const;
    uint8_t brightness() const;
    uint8_t order() const;
    const char* orderName() const;
    String shownText(bool withRuleNumber = true) const; // what the LED shows right now, for the web pages
    int shownRule() const; // the rule it shows: 1 is the first rule, 0 is Otherwise, -1 is neither (starting or a test)
    uint32_t writeErrors() const;

private:
    // Set by begin() before the timer starts, read-only afterwards
    bool _configured = false;
    uint8_t _pin = 0;
    std::atomic<bool> _running{false};

    // Written by begin() and the web server, read by the renderer
    std::atomic<uint8_t> _brightness{20};
    std::atomic<uint8_t> _order{0};
    mutable portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
    RuleSet _rules; // guarded by _mux
    std::atomic<uint32_t> _rulesVersion{0};

#if SOC_RMT_SUPPORTED
    struct Rgb
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    static constexpr uint8_t kTriggerCount = (uint8_t)Trigger::Count;

    static void onTimer(TimerHandle_t timer);
    static void onShutdown();
    void render();
    void updateTriggers(uint32_t now);
    bool ruleApplies(const Rule& rule, uint32_t now) const;
    Rgb selfTestColor(uint32_t phase) const;
    static void encode(const Rgb& rgb, uint8_t order, rmt_data_t* symbols);

    // Inputs, each written by a single producer task and read by the renderer
    std::atomic<bool> _updating{false};
    std::atomic<bool> _apOpen{false};
    std::atomic<bool> _networkConnected{false};
    std::atomic<bool> _mqttConnected{false};
    std::atomic<bool> _mqttExpected{false};
    std::atomic<bool> _notPaired[2] = { {false}, {false} };
    std::atomic<bool> _unreachable[2] = { {false}, {false} };
    std::atomic<bool> _bleBusy{false};
    std::atomic<bool> _bleSeen{false};
    std::atomic<uint32_t> _bleStartMs{0};
    std::atomic<uint16_t> _lockFlags{0};
    std::atomic<uint16_t> _openerFlags{0};
    std::atomic<uint32_t> _ringCount{0};

    // Test from the web page, guarded by _mux
    Effect _previewEffect = Effect::Off;
    uint32_t _previewColor = 0;
    uint8_t _previewBrightness = 20;
    uint8_t _previewOrder = 0;
    std::atomic<uint32_t> _previewSeq{0};

    // Renderer state, only touched by the timer service task
    RuleSet _active;
    uint32_t _activeVersion = 0;
    bool _triggerActive[kTriggerCount] = {};
    bool _triggerPrimed[kTriggerCount] = {};
    bool _triggerRisen[kTriggerCount] = {};
    uint32_t _triggerSince[kTriggerCount] = {};
    uint32_t _triggerRise[kTriggerCount] = {};
    uint32_t _ringSeen = 0;
    uint32_t _previewSeen = 0;
    bool _previewing = false;
    uint32_t _previewStart = 0;
    Effect _previewShown = Effect::Off;
    uint32_t _previewShownColor = 0;
    uint8_t _previewShownBrightness = 20;
    uint8_t _previewShownOrder = 0;
    uint32_t _currentKey = 0xFFFFFFFF;
    uint32_t _phaseStart = 0;
    bool _selfTestPending = false;
    bool _selfTestStarted = false;
    uint32_t _selfTestStart = 0;
    bool _startingDone = false;
    Rgb _last = { 0, 0, 0 };
    uint8_t _lastOrder = 0;
    uint32_t _lastWrite = 0;
    bool _everWritten = false;
    uint8_t _buffer = 0;
    rmt_data_t _symbols[2][24] = {};
    TimerHandle_t _timer = nullptr;

    // Written by the renderer, read by the web server: shown index (low byte, see kShown*), trigger, effect
    std::atomic<uint32_t> _shown{0xFFFFFFFF};
    std::atomic<uint32_t> _writeErrors{0};

    // Hand-over between the renderer and the shutdown handler
    std::atomic<bool> _shutdown{false};
    std::atomic<bool> _writing{false};
#endif
};
