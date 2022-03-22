#include "Arduino.h"
#include "Network.h"
#include "Nuki.h"


#define ESP32

Network network;
Nuki nuki("door", 0);


void setup()
{
    network.initialize();
//    nuki.initialize();

    /*
    Serial.begin(115200);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.println("Connecting to WiFi..");
    }

    Serial.println("Connected to the WiFi network");
     */
}

void loop()
{
    network.update();
//    nuki.update();
}