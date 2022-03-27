#include "Arduino.h"
#include "Network.h"
#include "Nuki.h"
#include "WebCfgServer.h"
#include <FreeRTOS.h>

#define ESP32

Network* network;
WebCfgServer* webCfgServer;
Nuki* nuki;
Preferences* preferences;

void networkTask(void *pvParameters)
{
    while(true)
    {
        network->update();
        webCfgServer->update();
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
    xTaskCreate(nukiTask, "nuki", 16384, NULL, 1, NULL);
}

void setup()
{
    preferences = new Preferences();
    preferences->begin("nukihub", false);
    network = new Network(preferences);
    webCfgServer = new WebCfgServer(preferences);
    nuki = new Nuki("Main Door", 2020001, network);

    network->initialize();
    webCfgServer->initialize();

    nuki->initialize();
    setupTasks();

//    Serial.println(byte_array_dec[0]);
}

void loop()
{}