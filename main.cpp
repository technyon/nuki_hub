#include "Arduino.h"
#include "Pins.h"
#include "NukiWrapper.h"
#include "Network.h"
#include "WebCfgServer.h"
#include <FreeRTOS.h>
#include "PreferencesKeys.h"
#include "PresenceDetection.h"
#include "hardware/W5500EthServer.h"
#include "hardware/WifiEthServer.h"
#include "NukiOpenerWrapper.h"

Network* network = nullptr;
WebCfgServer* webCfgServer = nullptr;
NukiWrapper* nuki = nullptr;
NukiOpenerWrapper* nukiOpener = nullptr;
PresenceDetection* presenceDetection = nullptr;
Preferences* preferences = nullptr;
EthServer* ethServer = nullptr;

void networkTask(void *pvParameters)
{
    while(true)
    {
        network->update();
        webCfgServer->update();
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

void nukiTask(void *pvParameters)
{
    while(true)
    {
        nuki->update();
    }
}

void presenceDetectionTask(void *pvParameters)
{
    while(true)
    {
        presenceDetection->update();
    }
}

void checkMillisTask(void *pvParameters)
{
    while(true)
    {
        vTaskDelay( 60000 / portTICK_PERIOD_MS);
        // millis() is about to overflow. Restart device to prevent problems with overflow
        if(millis() > (2^32) - 5 * 60000)
        {
            Serial.println(F("millis() is about to overflow. Restarting device."));
            vTaskDelay( 2000 / portTICK_PERIOD_MS);
            ESP.restart();
        }
    }
}


void setupTasks()
{
    // configMAX_PRIORITIES is 25

    xTaskCreate(networkTask, "ntw", 32768, NULL, 3, NULL);
    xTaskCreate(nukiTask, "nuki", 8192, NULL, 2, NULL);
    xTaskCreate(presenceDetectionTask, "prdet", 1024, NULL, 5, NULL);
    xTaskCreate(checkMillisTask, "mlchk", 512, NULL, 1, NULL);
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

void initEthServer(const NetworkDeviceType device)
{
    switch (device)
    {
        case NetworkDeviceType::W5500:
            ethServer = new W5500EthServer(80);
            break;
        case NetworkDeviceType::WiFi:
            ethServer = new WifiEthServer(80);
            break;
        default:
            ethServer = new WifiEthServer(80);
            break;
    }
}

void setup()
{
    pinMode(NETWORK_SELECT, INPUT_PULLUP);

    Serial.begin(115200);

//    const NetworkDeviceType networkDevice = NetworkDeviceType::WiFi;
    const NetworkDeviceType networkDevice = digitalRead(NETWORK_SELECT) == HIGH ? NetworkDeviceType::WiFi : NetworkDeviceType::W5500;

    preferences = new Preferences();
    preferences->begin("nukihub", false);
    network = new Network(networkDevice, preferences);
    network->initialize();

    uint32_t deviceId = preferences->getUInt(preference_deviceId);
    if(deviceId == 0)
    {
        deviceId = getRandomId();
        preferences->putUInt(preference_deviceId, deviceId);
    }

    initEthServer(networkDevice);

    nuki = new NukiWrapper("NukiHub", deviceId, network, preferences);
//    nukiOpener = new NukiOpenerWrapper("NukiHub", deviceId, network, preferences);
    webCfgServer = new WebCfgServer(nuki, network, ethServer, preferences, networkDevice == NetworkDeviceType::WiFi);
    webCfgServer->initialize();
    nuki->initialize();

    presenceDetection = new PresenceDetection(preferences, nuki->bleScanner(), network);
    presenceDetection->initialize();

    setupTasks();
}

void loop()
{}