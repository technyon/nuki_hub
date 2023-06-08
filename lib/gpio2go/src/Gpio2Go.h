#pragma once

#include <Arduino.h>
#include <vector>
#include "esp_attr.h"
#include "PinMode.h"
#include "InterruptMode.h"

#define GPIO2GO_NR_OF_PINS 31
#define GPIO2GO_NR_FIRST_PIN 2

class Gpio2Go
{
public:
    static void configurePin(int pin, PinMode pin_Mode, InterruptMode interrupt_Mode, uint16_t timeoutAfterTriggerMS);
    static void subscribe(std::function<void(const int&)> callback);

    unsigned long getLastTriggeredMillis(const int& pin);

private:
    static void attachIsr(int pin, InterruptMode interruptMode);
    static int resolveInterruptMode(InterruptMode interruptMode);

    static void IRAM_ATTR isrHandler(int pin);
    static void IRAM_ATTR isrGpio2();
    static void IRAM_ATTR isrGpio4();
    static void IRAM_ATTR isrGpio5();
    static void IRAM_ATTR isrGpio13();
    static void IRAM_ATTR isrGpio14();
    static void IRAM_ATTR isrGpio15();
    static void IRAM_ATTR isrGpio16();
    static void IRAM_ATTR isrGpio17();
    static void IRAM_ATTR isrGpio18();
    static void IRAM_ATTR isrGpio19();
    static void IRAM_ATTR isrGpio20();
    static void IRAM_ATTR isrGpio21();
    static void IRAM_ATTR isrGpio22();
    static void IRAM_ATTR isrGpio23();
    static void IRAM_ATTR isrGpio24();
    static void IRAM_ATTR isrGpio25();
    static void IRAM_ATTR isrGpio26();
    static void IRAM_ATTR isrGpio27();
    static void IRAM_ATTR isrGpio32();
    static void IRAM_ATTR isrGpio33();

    static unsigned long DRAM_ATTR lastTriggeredTimestamps[GPIO2GO_NR_OF_PINS];
    static uint16_t DRAM_ATTR timeoutDurations[GPIO2GO_NR_OF_PINS];
    static std::vector<std::function<void(const int&)>> DRAM_ATTR subscriptions;
};
