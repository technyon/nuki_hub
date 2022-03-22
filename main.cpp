#include "Arduino.h"
#include "Network.h"
#include "Nuki.h"
#include <FreeRTOS.h>


#define ESP32

Network network;
Nuki nuki("door", 0);

void networkTask(void *pvParameters)
{
    network.update();
}

void nukiTask(void *pvParameters)
{
//    nuki.update();
}

void setupTasks()
{
    xTaskCreate(networkTask, "ntw", 1024, NULL, 1, NULL);
    xTaskCreate(nukiTask, "nuki", 1024, NULL, 1, NULL);
}

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
    setupTasks();
}

void loop()
{
//    nuki.update();
}