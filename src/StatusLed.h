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

    enum class State : uint8_t
    {
        Off,
        SelfTest,
        Updating,
        AccessPoint,
        NetworkDown,
        PairingRequired,
        Starting,
        MqttDown,
        DeviceUnreachable,
        BleActivity,
        Ok
    };

    // Called once while setting up the GPIOs (loop task)
    void begin(uint8_t pin, Preferences* preferences, bool selfTest);

    void reportNetwork(bool apOpen, bool connected, bool mqttConnected, bool mqttExpected); // network task
    void reportPaired(Device device, bool paired); // nuki task
    void reportReachable(Device device, bool reachable); // nuki task
    void reportBleActivity(bool active); // nuki task
    void setUpdating(); // loop task, before the OTA task starts

    bool configured() const;
    bool running() const;
    uint8_t pin() const;
    uint8_t brightness() const;
    bool quiet() const;
    const char* orderName() const;
    const char* stateName() const;
    uint32_t writeErrors() const;

private:
    // Set by begin() before the timer starts, read-only afterwards
    bool _configured = false;
    uint8_t _pin = 0;
    uint8_t _brightness = 20;
    uint8_t _order = 0;
    bool _quiet = false;
    std::atomic<bool> _running{false};

#if SOC_RMT_SUPPORTED
    struct Rgb
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    static void onTimer(TimerHandle_t timer);
    static void onShutdown();
    void render();
    State computeState(uint32_t now);
    Rgb colorFor(State state, uint32_t phase) const;
    void encode(const Rgb& rgb, rmt_data_t* symbols) const;

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

    // Renderer state, only touched by the timer service task
    State _current = State::Off;
    uint32_t _phaseStart = 0;
    bool _selfTestPending = false;
    bool _selfTestStarted = false;
    uint32_t _selfTestStart = 0;
    bool _startingDone = false;
    bool _networkDownTracked = false;
    uint32_t _networkDownSince = 0;
    bool _mqttDownTracked = false;
    uint32_t _mqttDownSince = 0;
    Rgb _last = { 0, 0, 0 };
    uint32_t _lastWrite = 0;
    bool _everWritten = false;
    uint8_t _buffer = 0;
    rmt_data_t _symbols[2][24] = {};
    TimerHandle_t _timer = nullptr;

    // Written by the renderer, read by the web server
    std::atomic<State> _shown{State::Off};
    std::atomic<uint32_t> _writeErrors{0};

    // Hand-over between the renderer and the shutdown handler
    std::atomic<bool> _shutdown{false};
    std::atomic<bool> _writing{false};
#endif
};
