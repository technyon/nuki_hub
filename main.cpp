#include "Arduino.h"
#include "Network.h"
#include "Nuki.h"
#include <FreeRTOS.h>


#define ESP32

Network* network;
Nuki* nuki;

void networkTask(void *pvParameters)
{
    while(true)
    {
        network->update();
    }
}

void nukiTask(void *pvParameters)
{
    while(true)
    {
        nuki->update();
    }
}

void setupTasks()
{
    xTaskCreate(networkTask, "ntw", 2048, NULL, 1, NULL);
    xTaskCreate(nukiTask, "nuki", 4096, NULL, 1, NULL);
}

void setup()
{
    network = new Network();
    nuki = new Nuki("Main Door", 2020001, network);

    network->initialize();
    nuki->initialize();
    setupTasks();
}

void loop()
{}