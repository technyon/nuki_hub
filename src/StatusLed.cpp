#include "StatusLed.h"
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

#if SOC_RMT_SUPPORTED
// For each color order: which of {r, g, b} goes on the wire first, second and third
const uint8_t kOrder[6][3] = { {1, 0, 2}, {0, 1, 2}, {2, 0, 1}, {0, 2, 1}, {1, 2, 0}, {2, 1, 0} };

constexpr uint32_t kTickMs = 50;
constexpr uint32_t kSelfTestStepMs = 400;
constexpr uint32_t kSelfTestMs = 3 * kSelfTestStepMs;
constexpr uint32_t kStartingMaxMs = 60000;
constexpr uint32_t kDownDelayMs = 5000;
constexpr uint32_t kBleMinMs = 250;
constexpr uint32_t kRefreshMs = 5000;
constexpr size_t kSymbols = 24;

StatusLed* s_instance = nullptr;

bool slowBlinkOn(uint32_t phase)
{
    return (phase % 2000) < 1000;
}

// n flashes of 150 ms on / 350 ms off, then 1500 ms off
bool flashOn(uint32_t phase, uint32_t n)
{
    const uint32_t t = phase % (n * 500 + 1500);
    return t < n * 500 && (t % 500) < 150;
}

uint8_t scale(uint8_t c, uint8_t brightness)
{
    if(c == 0)
    {
        return 0;
    }
    const uint8_t v = (uint8_t)(((uint32_t)c * brightness + 50) / 100);
    return v ? v : 1;
}

// True once the condition has held for at least duration ms without interruption
bool heldFor(bool condition, bool& tracked, uint32_t& since, uint32_t now, uint32_t duration)
{
    if(!condition)
    {
        tracked = false;
        return false;
    }
    if(!tracked)
    {
        tracked = true;
        since = now;
    }
    return now - since >= duration;
}
#endif
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
    return _brightness;
}

bool StatusLed::quiet() const
{
    return _quiet;
}

const char* StatusLed::orderName() const
{
    return kOrderNames[_order < 6 ? _order : 0];
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

    const int brightness = preferences->getInt(preference_status_led_brightness, 20);
    _brightness = (uint8_t)(brightness < 1 ? 1 : (brightness > 100 ? 100 : brightness));
    const int order = preferences->getInt(preference_status_led_order, 0);
    _order = (uint8_t)(order >= 0 && order <= 5 ? order : 0);
    _quiet = preferences->getBool(preference_status_led_quiet, false);
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
    Log->print(_brightness);
    Log->println(" %)");
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

const char* StatusLed::stateName() const
{
    switch(_shown.load())
    {
        case State::SelfTest:
            return "self-test";
        case State::Updating:
            return "updating";
        case State::AccessPoint:
            return "Wi-Fi setup";
        case State::NetworkDown:
            return "network down";
        case State::PairingRequired:
            return "pairing required";
        case State::Starting:
            return "starting";
        case State::MqttDown:
            return "MQTT down";
        case State::DeviceUnreachable:
            return "lock/opener unreachable";
        case State::BleActivity:
            return "Bluetooth activity";
        case State::Ok:
            return "OK";
        case State::Off:
        default:
            return "off";
    }
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
    const State state = computeState(now);
    if(state != _current)
    {
        _current = state;
        _phaseStart = now;
        _shown.store(state);
    }

    const Rgb rgb = colorFor(state, now - _phaseStart);
    const bool changed = rgb.r != _last.r || rgb.g != _last.g || rgb.b != _last.b;

    // Rewrite periodically even without a change, which repairs a frame that was corrupted on the wire
    if(!_everWritten || changed || now - _lastWrite >= kRefreshMs)
    {
        rmt_data_t* symbols = _symbols[_buffer];
        encode(rgb, symbols);

        // rmtWriteAsync() logs an error when the pin is not attached to RMT, so check first
        if(perimanGetPinBusType(_pin) == ESP32_BUS_TYPE_RMT_TX && rmtWriteAsync(_pin, symbols, kSymbols))
        {
            _last = rgb;
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

StatusLed::State StatusLed::computeState(uint32_t now)
{
    const bool networkConnected = _networkConnected.load();
    const bool apOpen = _apOpen.load();
    const bool mqttConnected = _mqttConnected.load();
    const bool mqttExpected = _mqttExpected.load();

    // Track these on every tick so the 5 s delay starts when the problem starts, whatever is shown meanwhile
    const bool networkDown = heldFor(!networkConnected, _networkDownTracked, _networkDownSince, now, kDownDelayMs);
    const bool mqttDown = heldFor(networkConnected && mqttExpected && !mqttConnected, _mqttDownTracked, _mqttDownSince, now, kDownDelayMs);

    // Starting ends for good at the first time everything is ready, or after 60 s
    if(!_startingDone && ((networkConnected && (mqttConnected || !mqttExpected)) || now >= kStartingMaxMs))
    {
        _startingDone = true;
    }

    if(_selfTestPending)
    {
        if(!_selfTestStarted)
        {
            _selfTestStarted = true;
            _selfTestStart = now;
        }
        if(now - _selfTestStart < kSelfTestMs)
        {
            return State::SelfTest;
        }
        _selfTestPending = false;
    }

    if(_updating.load())
    {
        return State::Updating;
    }
    if(apOpen)
    {
        return State::AccessPoint;
    }
    if(_startingDone && networkDown)
    {
        return State::NetworkDown;
    }
    if(_notPaired[0].load() || _notPaired[1].load())
    {
        return State::PairingRequired;
    }
    if(!_startingDone)
    {
        return State::Starting;
    }
    if(mqttDown)
    {
        return State::MqttDown;
    }
    if(_unreachable[0].load() || _unreachable[1].load())
    {
        return State::DeviceUnreachable;
    }
    if(_bleBusy.load() || (_bleSeen.load() && now - _bleStartMs.load() < kBleMinMs))
    {
        return State::BleActivity;
    }
    return State::Ok;
}

StatusLed::Rgb StatusLed::colorFor(State state, uint32_t phase) const
{
    Rgb rgb = { 0, 0, 0 };
    bool on = true;

    switch(state)
    {
        case State::SelfTest:
            if(phase < kSelfTestStepMs)
            {
                rgb = { 255, 0, 0 };
            }
            else if(phase < 2 * kSelfTestStepMs)
            {
                rgb = { 0, 255, 0 };
            }
            else
            {
                rgb = { 0, 0, 255 };
            }
            break;
        case State::Updating:
        case State::Starting:
            rgb = { 255, 255, 255 };
            on = !_quiet;
            break;
        case State::AccessPoint:
            rgb = { 0, 0, 255 };
            on = slowBlinkOn(phase);
            break;
        case State::NetworkDown:
            rgb = { 255, 0, 0 };
            on = flashOn(phase, 1);
            break;
        case State::PairingRequired:
            rgb = { 255, 0, 255 };
            on = slowBlinkOn(phase);
            break;
        case State::MqttDown:
            rgb = { 255, 100, 0 };
            on = flashOn(phase, 2);
            break;
        case State::DeviceUnreachable:
            rgb = { 255, 0, 255 };
            on = flashOn(phase, 3);
            break;
        case State::BleActivity:
            rgb = { 0, 255, 255 };
            on = !_quiet;
            break;
        case State::Ok:
            rgb = { 0, 255, 0 };
            on = !_quiet;
            break;
        case State::Off:
        default:
            on = false;
            break;
    }

    if(!on)
    {
        return { 0, 0, 0 };
    }
    return { scale(rgb.r, _brightness), scale(rgb.g, _brightness), scale(rgb.b, _brightness) };
}

// WS2812 timing at 10 MHz (100 ns per tick): 1 = 800 ns high + 400 ns low, 0 = 400 ns high + 800 ns low, MSB first
void StatusLed::encode(const Rgb& rgb, rmt_data_t* symbols) const
{
    const uint8_t channels[3] = { rgb.r, rgb.g, rgb.b };
    size_t i = 0;

    for(uint8_t position = 0; position < 3; position++)
    {
        const uint8_t value = channels[kOrder[_order][position]];
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
    self->encode({ 0, 0, 0 }, off);
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

void StatusLed::setUpdating()
{
}

uint32_t StatusLed::writeErrors() const
{
    return 0;
}

const char* StatusLed::stateName() const
{
    return "not supported";
}

#endif
