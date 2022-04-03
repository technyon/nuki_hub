#include "Arduino.h"
#include "NukiWrapper.h"
#include "Network.h"
#include "WebCfgServer.h"
#include <FreeRTOS.h>
#include "PreferencesKeys.h"

#define ESP32

Network* network;
WebCfgServer* webCfgServer;
NukiWrapper* nuki;
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
    xTaskCreate(networkTask, "ntw", 8192, NULL, 1, NULL);
    xTaskCreate(nukiTask, "nuki", 16384, NULL, 1, NULL);
}

uint32_t getRandomId()
{
    uint8_t rnd[4];
    for(int i=0; i<4; i++)
    {
        rnd[i] = random(255);
    }
    uint32_t deviceId;
    memcpy(&deviceId, &rnd, sizeof(deviceId));
    return deviceId;
}

void setup()
{
    Serial.begin(115200);

    preferences = new Preferences();
    preferences->begin("nukihub", false);
    network = new Network(preferences);
    network->initialize();

    uint32_t deviceId = preferences->getUInt(preference_deviceId);
    if(deviceId == 0)
    {
        deviceId = getRandomId();
        preferences->putUInt(preference_deviceId, deviceId);
    }

    nuki = new NukiWrapper("ESP", deviceId, network, preferences);
    webCfgServer = new WebCfgServer(nuki, network, preferences);
    webCfgServer->initialize();

    nuki->initialize();
    setupTasks();
}

void loop()
{}