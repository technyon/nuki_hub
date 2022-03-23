#include "Arduino.h"
#include "Network.h"
#include "Nuki.h"
#include <FreeRTOS.h>


#define ESP32

Network network;
Nuki nuki("door", 0);

void networkTask(void *pvParameters)
{
    while(true)
    {
        network.update();
    }
}

void nukiTask(void *pvParameters)
{
    while(true)
    {
        nuki.update();
    }
}

void setupTasks()
{
    xTaskCreate(networkTask, "ntw", 2048, NULL, 1, NULL);
//    xTaskCreate(nukiTask, "nuki", 1024, NULL, 1, NULL);
}

void setup()
{
    network.initialize();
//    nuki.initialize();
    setupTasks();
}

void loop()
{}