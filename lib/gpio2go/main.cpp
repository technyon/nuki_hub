#include "Arduino.h"
#include "Gpio2Go.h"

#define INPUT_PIN 21

bool hasMessage = false;
String message;

void inputCb(const int & pin)
{
    message = "";
    message.concat("Input, Pin ");
    message.concat(pin);
    message.concat(" ");
    message.concat(", state ");
    message.concat(digitalRead(INPUT_PIN) ? "High" : "Low");
    hasMessage = true;
}

void setup()
{
    Serial.begin(115200);

    delay(1100);
    Serial.println(F("Started"));
    Gpio2Go::configurePin(INPUT_PIN, PinMode::InputPullup, InterruptMode::Change, 200);
    Gpio2Go::subscribe(inputCb);
}

void loop()
{
    delay(100);
    if(hasMessage)
    {
        hasMessage = false;
        Serial.println(message);
    }
}