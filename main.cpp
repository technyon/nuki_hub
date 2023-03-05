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
#include "Logger.h"
#include "Config.h"
#include "RestartReason.h"

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

RTC_NOINIT_ATTR int restartReason;
RTC_NOINIT_ATTR uint64_t restartReasonValid;

TaskHandle_t networkTaskHandle = nullptr;
TaskHandle_t nukiTaskHandle = nullptr;
TaskHandle_t presenceDetectionTaskHandle = nullptr;

void networkTask(void *pvParameters)
{
    while(true)
    {
        bool connected = network->update();
        if(connected && openerEnabled)
        {
            networkOpener->update();
        }
        webCfgServer->update();

        // millis() is about to overflow. Restart device to prevent problems with overflow
        if(millis() > restartTs)
        {
            Log->println(F("Restart timer expired, restarting device."));
            delay(200);
            restartEsp(RestartReason::RestartTimer);
        }

        delay(100);

//        if(wmts < millis())
//        {
//            Serial.print("# ");
//            Serial.println(uxTaskGetStackHighWaterMark(NULL));
//            wmts = millis() + 60000;
//        }
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


void setupTasks()
{
    // configMAX_PRIORITIES is 25

    xTaskCreatePinnedToCore(networkTask, "ntw", 8192, NULL, 3, &networkTaskHandle, 1);
    xTaskCreatePinnedToCore(nukiTask, "nuki", 3328, NULL, 2, &nukiTaskHandle, 1);
    xTaskCreatePinnedToCore(presenceDetectionTask, "prdet", 896, NULL, 5, &presenceDetectionTaskHandle, 1);
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

bool initPreferences()
{
    preferences = new Preferences();
    preferences->begin("nukihub", false);

    bool firstStart = !preferences->getBool(preference_started_before);

    if(firstStart)
    {
        preferences->putBool(preference_started_before, true);
        preferences->putBool(preference_lock_enabled, true);
    }

    if(preferences->getInt(preference_restart_timer) == 0)
    {
        preferences->putInt(preference_restart_timer, -1);
    }

    return firstStart;
}

void setup()
{
    Serial.begin(115200);
    Log = &Serial;

    Log->print(F("NUKI Hub version ")); Log->println(NUKI_HUB_VERSION);

    bool firstStart = initPreferences();

    if(preferences->getInt(preference_restart_timer) > 0)
    {
        restartTs = preferences->getInt(preference_restart_timer) * 60 * 1000;
    }

    lockEnabled = preferences->getBool(preference_lock_enabled);
    openerEnabled = preferences->getBool(preference_opener_enabled);

    const String mqttLockPath = preferences->getString(preference_mqtt_lock_path);
    network = new Network(preferences, mqttLockPath);
    network->initialize();

    networkLock = new NetworkLock(network, preferences);
    networkLock->initialize();

    if(openerEnabled)
    {
        networkOpener = new NetworkOpener(network, preferences);
        networkOpener->initialize();
    }

    uint32_t deviceId = preferences->getUInt(preference_deviceId);
    if(deviceId == 0)
    {
        deviceId = getRandomId();
        preferences->putUInt(preference_deviceId, deviceId);
    }

    initEthServer(network->networkDeviceType());

    bleScanner = new BleScanner::Scanner();
    bleScanner->initialize("NukiHub");
    bleScanner->setScanDuration(10);

    Log->println(lockEnabled ? F("NUKI Lock enabled") : F("NUKI Lock disabled"));
    if(lockEnabled)
    {
        nuki = new NukiWrapper("NukiHub", deviceId, bleScanner, networkLock, preferences);
        nuki->initialize(firstStart);

        if(preferences->getBool(preference_gpio_locking_enabled))
        {
            Gpio::init(nuki);
        }
    }

    Log->println(openerEnabled ? F("NUKI Opener enabled") : F("NUKI Opener disabled"));
    if(openerEnabled)
    {
        nukiOpener = new NukiOpenerWrapper("NukiHub", deviceId, bleScanner, networkOpener, preferences);
        nukiOpener->initialize();
    }

    webCfgServer = new WebCfgServer(nuki, nukiOpener, network, ethServer, preferences, network->networkDeviceType() == NetworkDeviceType::WiFi);
    webCfgServer->initialize();

    presenceDetection = new PresenceDetection(preferences, bleScanner, network);
    presenceDetection->initialize();

    setupTasks();
}

void loop()
{}