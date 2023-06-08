#include "Gpio2Go.h"


void Gpio2Go::configurePin(int pin, PinMode pin_Mode, InterruptMode interrupt_Mode, uint16_t timeoutAfterTriggerMS)
{
    timeoutDurations[pin - GPIO2GO_NR_FIRST_PIN] = timeoutAfterTriggerMS;

    switch(pin_Mode)
    {
        case PinMode::InputPullup:
            pinMode(pin, INPUT_PULLUP);
            attachIsr(pin, interrupt_Mode);
            break;
        case PinMode::InputPullDown:
            pinMode(pin, INPUT_PULLDOWN);
            attachIsr(pin, interrupt_Mode);
            break;
        case PinMode::Output:
            pinMode(pin, OUTPUT);
            break;
    }
}

void Gpio2Go::subscribe(std::function<void(const int &)> callback)
{
    subscriptions.push_back(callback);
}

unsigned long Gpio2Go::getLastTriggeredMillis(const int &pin)
{
    if(pin >= GPIO2GO_NR_FIRST_PIN && pin <= (GPIO2GO_NR_OF_PINS + GPIO2GO_NR_FIRST_PIN))
    {
        return lastTriggeredTimestamps[pin - GPIO2GO_NR_FIRST_PIN];
    }
    return -1;
}

void Gpio2Go::attachIsr(int pin, InterruptMode interruptMode)
{
    switch(pin)
    {
        case 2:
            attachInterrupt(2, isrGpio2, resolveInterruptMode(interruptMode));
            break;
        case 4:
            attachInterrupt(4, isrGpio4, resolveInterruptMode(interruptMode));
            break;
        case 5:
            attachInterrupt(5, isrGpio5, resolveInterruptMode(interruptMode));
            break;
        case 13:
            attachInterrupt(13, isrGpio13, resolveInterruptMode(interruptMode));
            break;
        case 14:
            attachInterrupt(14, isrGpio14, resolveInterruptMode(interruptMode));
            break;
        case 15:
            attachInterrupt(15, isrGpio15, resolveInterruptMode(interruptMode));
            break;
        case 16:
            attachInterrupt(16, isrGpio16, resolveInterruptMode(interruptMode));
            break;
        case 17:
            attachInterrupt(17, isrGpio17, resolveInterruptMode(interruptMode));
            break;
        case 18:
            attachInterrupt(18, isrGpio18, resolveInterruptMode(interruptMode));
            break;
        case 19:
            attachInterrupt(19, isrGpio19, resolveInterruptMode(interruptMode));
            break;
        case 20:
            attachInterrupt(20, isrGpio20, resolveInterruptMode(interruptMode));
            break;
        case 21:
            attachInterrupt(21, isrGpio21, resolveInterruptMode(interruptMode));
            break;
        case 22:
            attachInterrupt(22, isrGpio22, resolveInterruptMode(interruptMode));
            break;
        case 23:
            attachInterrupt(23, isrGpio23, resolveInterruptMode(interruptMode));
            break;
        case 24:
            attachInterrupt(24, isrGpio24, resolveInterruptMode(interruptMode));
            break;
        case 25:
            attachInterrupt(25, isrGpio25, resolveInterruptMode(interruptMode));
            break;
        case 26:
            attachInterrupt(26, isrGpio26, resolveInterruptMode(interruptMode));
            break;
        case 27:
            attachInterrupt(27, isrGpio27, resolveInterruptMode(interruptMode));
            break;
        case 32:
            attachInterrupt(32, isrGpio32, resolveInterruptMode(interruptMode));
            break;
        case 33:
            attachInterrupt(33, isrGpio33, resolveInterruptMode(interruptMode));
            break;
        default:
            throw std::runtime_error("Gpio2Go: Unsupported pin.");
    }
}

int Gpio2Go::resolveInterruptMode(InterruptMode interruptMode)
{
    switch(interruptMode)
    {
        case InterruptMode::Rising:
            return RISING;
        case InterruptMode::Falling:
            return FALLING;
        case InterruptMode::Change:
            return CHANGE;
        case InterruptMode::OnLow:
            return ONLOW;
        case InterruptMode::OnHigh:
            return ONHIGH;
        default:
            throw std::runtime_error("Gpio2Go: Unsupported interrupt mode.");
    }
}

void Gpio2Go::isrHandler(int pin)
{
    unsigned long timeout = lastTriggeredTimestamps[pin - GPIO2GO_NR_FIRST_PIN];
    if(timeoutDurations[pin - GPIO2GO_NR_FIRST_PIN] != 0 && (millis() - timeout) < timeoutDurations[pin - GPIO2GO_NR_FIRST_PIN]) return;
    lastTriggeredTimestamps[pin - GPIO2GO_NR_FIRST_PIN] = millis();

    bool state = digitalRead(pin) == HIGH;

    for(const auto& callback : subscriptions)
    {
        callback(pin);
    }
}

void Gpio2Go::isrGpio2()
{
    isrHandler(2);
}

void Gpio2Go::isrGpio4()
{
    isrHandler(4);
}

void Gpio2Go::isrGpio5()
{
    isrHandler(5);
}

void Gpio2Go::isrGpio13()
{
    isrHandler(13);
}

void Gpio2Go::isrGpio14()
{
    isrHandler(14);
}

void Gpio2Go::isrGpio15()
{
    isrHandler(15);
}

void Gpio2Go::isrGpio16()
{
    isrHandler(16);
}

void Gpio2Go::isrGpio17()
{
    isrHandler(17);
}

void Gpio2Go::isrGpio18()
{
    isrHandler(18);
}

void Gpio2Go::isrGpio19()
{
    isrHandler(19);
}

void Gpio2Go::isrGpio20()
{
    isrHandler(20);
}

void Gpio2Go::isrGpio21()
{
    isrHandler(21);
}

void Gpio2Go::isrGpio22()
{
    isrHandler(22);
}

void Gpio2Go::isrGpio23()
{
    isrHandler(23);
}

void Gpio2Go::isrGpio24()
{
    isrHandler(24);
}

void Gpio2Go::isrGpio25()
{
    isrHandler(25);
}

void Gpio2Go::isrGpio26()
{
    isrHandler(26);
}

void Gpio2Go::isrGpio27()
{
    isrHandler(27);
}

void Gpio2Go::isrGpio32()
{
    isrHandler(32);
}

void Gpio2Go::isrGpio33()
{
    isrHandler(33);
}

unsigned long Gpio2Go::lastTriggeredTimestamps[] = {0};
uint16_t Gpio2Go::timeoutDurations[] = {0};
std::vector<std::function<void(const int&)>> Gpio2Go::subscriptions;