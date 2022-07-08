#include "Arduino.h"
#include "Pins.h"
#include "NukiWrapper.h"
#include "NetworkLock.h"
#include "WebCfgServer.h"
#include <RTOS.h>
#include "PreferencesKeys.h"
#include "PresenceDetection.h"
#include "hardware/W5500EthServer.h"
#include "hardware/WifiEthServer.h"
#include "NukiOpenerWrapper.h"
#include "Gpio.h"

Network* network = nullptr;
NetworkLock* networkLock = nullptr;
NetworkOpener* networkOpener = nullptr;
WebCfgServer* webCfgServer = nullptr;
BleScanner::Scanner* bleScanner = nullptr;
NukiWrapper* nuki = nullptr;
NukiOpenerWrapper* nukiOpener = nullptr;
PresenceDetection* presenceDetection = nullptr;
Preferences* preferences = nullptr;
EthServer* ethServer = nullptr;

bool lockEnabled = false;
bool openerEnabled = false;
unsigned long restartTs = (2^32) - 5 * 60000;

void networkTask(void *pvParameters)
{
    while(true)
    {
        bool r = network->update();

        switch(r)
        {
            // Network Device and MQTT is connected. Process all updates.
            case 0:
                network->update();
                webCfgServer->update();
                break;
            case 1:
                // Network Device is connected, but MQTT isn't. Call network->update() to allow MQTT reconnect and
                // keep Webserver alive to allow user to reconfigure network settings
                network->update();
                webCfgServer->update();
                break;
                // Neither Network Devicce or MQTT is connected
            default:
                network->update();
                break;
        }

        delay(200);
    }
}

void nukiTask(void *pvParameters)
{
    while(true)
    {
        bleScanner->update();
        delay(20);

        bool needsPairing = (lockEnabled && !nuki->isPaired()) || (openerEnabled && !nukiOpener->isPaired());

        if (needsPairing)
        {
            delay(5000);
        }

        if(lockEnabled)
        {
            nuki->update();
        }
        if(openerEnabled)
        {
            nukiOpener->update();
        }
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
        delay(30000);

        // millis() is about to overflow. Restart device to prevent problems with overflow
        if(millis() > restartTs)
        {
            Serial.println(F("Restart timer expired, restarting device."));
            delay(2000);
            ESP.restart();
        }
    }
}


void setupTasks()
{
    // configMAX_PRIORITIES is 25

    xTaskCreatePinnedToCore(networkTask, "ntw", 8192, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(nukiTask, "nuki", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(presenceDetectionTask, "prdet", 768, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(checkMillisTask, "mlchk", 640, NULL, 1, NULL, 1);
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

void initPreferences()
{
    preferences = new Preferences();
    preferences->begin("nukihub", false);

    if(!preferences->getBool(preference_started_befores))
    {
        preferences->putBool(preference_started_befores, true);
        preferences->putBool(preference_lock_enabled, true);
    }

    if(preferences->getInt(preference_restart_timer) == 0)
    {
        preferences->putInt(preference_restart_timer, -1);
    }
}

void setup()
{
    pinMode(NETWORK_SELECT, INPUT_PULLUP);

    Serial.begin(115200);

    initPreferences();

    if(preferences->getInt(preference_restart_timer) > 0)
    {
        restartTs = preferences->getInt(preference_restart_timer) * 60 * 1000;
    }

    const NetworkDeviceType networkDevice = NetworkDeviceType::WiFi;
//    const NetworkDeviceType networkDevice = digitalRead(NETWORK_SELECT) == HIGH ? NetworkDeviceType::WiFi : NetworkDeviceType::W5500;

    network = new Network(networkDevice, preferences);
    network->initialize();
    networkLock = new NetworkLock(network, preferences);
    networkLock->initialize();
    networkOpener = new NetworkOpener(network, preferences);
    networkOpener->initialize();

    uint32_t deviceId = preferences->getUInt(preference_deviceId);
    if(deviceId == 0)
    {
        deviceId = getRandomId();
        preferences->putUInt(preference_deviceId, deviceId);
    }

    initEthServer(networkDevice);

    bleScanner = new BleScanner::Scanner();
    bleScanner->initialize("NukiHub");
    bleScanner->setScanDuration(10);

    lockEnabled = preferences->getBool(preference_lock_enabled);
    Serial.println(lockEnabled ? F("NUKI Lock enabled") : F("NUKI Lock disabled"));
    if(lockEnabled)
    {
        nuki = new NukiWrapper("NukiHub", deviceId, bleScanner, networkLock, preferences);
        nuki->initialize();

        if(preferences->getBool(preference_gpio_locking_enabled))
        {
            Gpio::init(nuki);
        }
    }

    openerEnabled = preferences->getBool(preference_opener_enabled);
    Serial.println(openerEnabled ? F("NUKI Opener enabled") : F("NUKI Opener disabled"));
    if(openerEnabled)
    {
        nukiOpener = new NukiOpenerWrapper("NukiHub", deviceId, bleScanner, networkOpener, preferences);
        nukiOpener->initialize();
    }

    webCfgServer = new WebCfgServer(nuki, nukiOpener, network, ethServer, preferences, networkDevice == NetworkDeviceType::WiFi);
    webCfgServer->initialize();

    presenceDetection = new PresenceDetection(preferences, bleScanner, network);
    presenceDetection->initialize();

    setupTasks();
}

void loop()
{}