#include "Arduino.h"
#include "hardware/W5500EthServer.h"
#include "hardware/WifiEthServer.h"
#include "esp_crt_bundle.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#ifndef NUKI_HUB_UPDATER
#include "NukiWrapper.h"
#include "NukiNetworkLock.h"
#include "PresenceDetection.h"
#include "NukiOpenerWrapper.h"
#include "Gpio.h"
#include "CharBuffer.h"
#include "NukiDeviceId.h"
#include "WebCfgServer.h"
#include "Logger.h"
#include "PreferencesKeys.h"
#include "Config.h"
#include "RestartReason.h"

NukiNetworkLock* networkLock = nullptr;
NukiNetworkOpener* networkOpener = nullptr;
BleScanner::Scanner* bleScanner = nullptr;
NukiWrapper* nuki = nullptr;
NukiOpenerWrapper* nukiOpener = nullptr;
PresenceDetection* presenceDetection = nullptr;
NukiDeviceId* deviceIdLock = nullptr;
NukiDeviceId* deviceIdOpener = nullptr;
Gpio* gpio = nullptr;

bool lockEnabled = false;
bool openerEnabled = false;

TaskHandle_t nukiTaskHandle = nullptr;
TaskHandle_t presenceDetectionTaskHandle = nullptr;

unsigned long restartTs = (2^32) - 5 * 60000;

#else
#include "../../src/WebCfgServer.h"
#include "../../src/Logger.h"
#include "../../src/PreferencesKeys.h"
#include "../../src/Config.h"
#include "../../src/RestartReason.h"
#include "../../src/NukiNetwork.h"

unsigned long restartTs = 10 * 60000;

#endif

NukiNetwork* network = nullptr;
WebCfgServer* webCfgServer = nullptr;
Preferences* preferences = nullptr;
EthServer* ethServer = nullptr;

RTC_NOINIT_ATTR int restartReason;
RTC_NOINIT_ATTR uint64_t restartReasonValidDetect;
RTC_NOINIT_ATTR bool rebuildGpioRequested;
bool restartReason_isValid;
RestartReason currentRestartReason = RestartReason::NotApplicable;

TaskHandle_t otaTaskHandle = nullptr;
TaskHandle_t networkTaskHandle = nullptr;

void networkTask(void *pvParameters)
{
    while(true)
    {
        bool connected = network->update();

        #ifndef NUKI_HUB_UPDATER
        if(connected && openerEnabled)
        {
            networkOpener->update();
        }

        if(preferences->getBool(preference_webserver_enabled, true))
        {
            webCfgServer->update();
        }
        #else
        webCfgServer->update();
        #endif

        // millis() is about to overflow. Restart device to prevent problems with overflow
        if(millis() > restartTs)
        {
            Log->println(F("Restart timer expired, restarting device."));
            delay(200);
            restartEsp(RestartReason::RestartTimer);
        }

        delay(100);
    }
}

#ifndef NUKI_HUB_UPDATER
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
#endif

uint8_t checkPartition()
{
    const esp_partition_t* running_partition = esp_ota_get_running_partition();
    Log->print(F("Partition size: "));
    Log->println(running_partition->size);
    Log->print(F("Partition subtype: "));
    Log->println(running_partition->subtype);

    if(running_partition->size == 1966080) return 0; //OLD PARTITION TABLE
    else if(running_partition->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0) return 1; //NEW PARTITION TABLE, RUNNING MAIN APP
    else return 2; //NEW PARTITION TABLE, RUNNING UPDATER APP
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            Log->println("HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            Log->println("HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            Log->println("HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            Log->println("HTTP_EVENT_ON_HEADER");
            break;
        case HTTP_EVENT_ON_DATA:
            Log->println("HTTP_EVENT_ON_DATA");
            break;
        case HTTP_EVENT_ON_FINISH:
            Log->println("HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            Log->println("HTTP_EVENT_DISCONNECTED");
            break;
        #if (ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 0, 0))
        case HTTP_EVENT_REDIRECT:
            Log->println("HTTP_EVENT_REDIRECT");
            break;
        #endif        
    }
    return ESP_OK;
}

#if (ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 0, 0))
void otaTask(void *pvParameter)
{
    uint8_t partitionType = checkPartition();
    String updateUrl;

    if(partitionType==1)
    {
        updateUrl = preferences->getString(preference_ota_updater_url);
        preferences->putString(preference_ota_updater_url, "");
    }
    else
    {
        updateUrl = preferences->getString(preference_ota_main_url);
        preferences->putString(preference_ota_main_url, "");
    }
    Log->print(F("URL: "));
    Log->println(updateUrl.c_str());
    Log->println("Starting OTA task");
    esp_http_client_config_t config = {
        .url = updateUrl.c_str(),
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    Log->print(F("Attempting to download update from "));
    Log->println(config.url);
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        Log->println("OTA Succeeded, Rebooting...");
        esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL));
        restartEsp(RestartReason::OTACompleted);
    } else {
        Log->println("Firmware upgrade failed");
        restartEsp(RestartReason::OTAAborted);
    }
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
#endif

void setupTasks(bool ota)
{
    // configMAX_PRIORITIES is 25

    if(ota) 
    {
        #if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0))
        xTaskCreatePinnedToCore(networkTask, "ntw", preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE), NULL, 3, &networkTaskHandle, 1);
        #ifndef NUKI_HUB_UPDATER
        xTaskCreatePinnedToCore(nukiTask, "nuki", preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE), NULL, 2, &nukiTaskHandle, 1);
        #endif
        #else
        xTaskCreatePinnedToCore(otaTask, "ota", 8192, NULL, 2, &otaTaskHandle, 1);
        #endif
    }
    else
    {
        xTaskCreatePinnedToCore(networkTask, "ntw", preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE), NULL, 3, &networkTaskHandle, 1);
        #ifndef NUKI_HUB_UPDATER
        xTaskCreatePinnedToCore(nukiTask, "nuki", preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE), NULL, 2, &nukiTaskHandle, 1);
        #endif
    }
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

    #ifndef NUKI_HUB_UPDATER
    bool firstStart = !preferences->getBool(preference_started_before);

    if(firstStart)
    {
        preferences->putBool(preference_started_before, true);
        preferences->putBool(preference_lock_enabled, true);
        uint32_t aclPrefs[17] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        preferences->putBytes(preference_acl, (byte*)(&aclPrefs), sizeof(aclPrefs));
        uint32_t basicLockConfigAclPrefs[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        preferences->putBytes(preference_conf_lock_basic_acl, (byte*)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
        uint32_t basicOpenerConfigAclPrefs[14] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        preferences->putBytes(preference_conf_opener_basic_acl, (byte*)(&basicOpenerConfigAclPrefs), sizeof(basicOpenerConfigAclPrefs));
        uint32_t advancedLockConfigAclPrefs[22] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
        uint32_t advancedOpenerConfigAclPrefs[20] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));
    }
    else
    {
        int configVer = preferences->getInt(preference_config_version);

        if(configVer < (atof(NUKI_HUB_VERSION) * 100))
        {
            if (configVer < 834)
            {
                if(preferences->getInt(preference_keypad_control_enabled))
                {
                    preferences->putBool(preference_keypad_info_enabled, true);
                }
                else
                {
                    preferences->putBool(preference_keypad_info_enabled, false);
                }

                switch(preferences->getInt(preference_access_level))
                {
                    case 0:
                        {
                            preferences->putBool(preference_keypad_control_enabled, true);
                            uint32_t aclPrefs[17] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
                            preferences->putBytes(preference_acl, (byte*)(&aclPrefs), sizeof(aclPrefs));
                            uint32_t basicLockConfigAclPrefs[16] = {0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0};
                            preferences->putBytes(preference_conf_lock_basic_acl, (byte*)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
                            uint32_t basicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
                            preferences->putBytes(preference_conf_opener_basic_acl, (byte*)(&basicOpenerConfigAclPrefs), sizeof(basicOpenerConfigAclPrefs));
                            uint32_t advancedLockConfigAclPrefs[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0};
                            preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
                            uint32_t advancedOpenerConfigAclPrefs[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0};
                            preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));
                            break;
                        }
                    case 1:
                        {
                            preferences->putBool(preference_keypad_control_enabled, false);
                            uint32_t aclPrefs[17] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0};
                            preferences->putBytes(preference_acl, (byte*)(&aclPrefs), sizeof(aclPrefs));
                            uint32_t basicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                            preferences->putBytes(preference_conf_lock_basic_acl, (byte*)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
                            uint32_t basicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                            preferences->putBytes(preference_conf_opener_basic_acl, (byte*)(&basicOpenerConfigAclPrefs), sizeof(basicOpenerConfigAclPrefs));
                            uint32_t advancedLockConfigAclPrefs[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                            preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
                            uint32_t advancedOpenerConfigAclPrefs[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                            preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));
                            break;
                        }
                    case 2:
                        {
                            preferences->putBool(preference_keypad_control_enabled, false);
                            uint32_t aclPrefs[17] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                            preferences->putBytes(preference_acl, (byte*)(&aclPrefs), sizeof(aclPrefs));
                            uint32_t basicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                            preferences->putBytes(preference_conf_lock_basic_acl, (byte*)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
                            uint32_t basicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                            preferences->putBytes(preference_conf_opener_basic_acl, (byte*)(&basicOpenerConfigAclPrefs), sizeof(basicOpenerConfigAclPrefs));
                            uint32_t advancedLockConfigAclPrefs[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                            preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
                            uint32_t advancedOpenerConfigAclPrefs[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                            preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));
                            break;
                        }
                    case 3:
                        {
                            preferences->putBool(preference_keypad_control_enabled, false);
                            uint32_t aclPrefs[17] = {1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0};
                            preferences->putBytes(preference_acl, (byte*)(&aclPrefs), sizeof(aclPrefs));
                            uint32_t basicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                            preferences->putBytes(preference_conf_lock_basic_acl, (byte*)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
                            uint32_t basicOpenerConfigAclPrefs[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                            preferences->putBytes(preference_conf_opener_basic_acl, (byte*)(&basicOpenerConfigAclPrefs), sizeof(basicOpenerConfigAclPrefs));
                            uint32_t advancedLockConfigAclPrefs[22] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                            preferences->putBytes(preference_conf_lock_advanced_acl, (byte*)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));
                            uint32_t advancedOpenerConfigAclPrefs[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                            preferences->putBytes(preference_conf_opener_advanced_acl, (byte*)(&advancedOpenerConfigAclPrefs), sizeof(advancedOpenerConfigAclPrefs));
                            break;
                        }
                }
            }

            preferences->putInt(preference_config_version, atof(NUKI_HUB_VERSION) * 100);
        }
    }

    return firstStart;
    #else
    return false;
    #endif
}

void setup()
{
    Serial.begin(115200);
    Log = &Serial;

    bool firstStart = initPreferences();
    uint8_t partitionType = checkPartition();

    initializeRestartReason();

    #ifdef NUKI_HUB_UPDATER
    Log->print(F("Nuki Hub OTA version ")); Log->println(NUKI_HUB_VERSION);
    Log->print(F("Nuki Hub OTA build ")); Log->println(NUKI_HUB_BUILD);

    network = new NukiNetwork(preferences);
    network->initialize();
    initEthServer(network->networkDeviceType());
    webCfgServer = new WebCfgServer(network, ethServer, preferences, network->networkDeviceType() == NetworkDeviceType::WiFi, partitionType);
    webCfgServer->initialize();
    #else
    Log->print(F("Nuki Hub version ")); Log->println(NUKI_HUB_VERSION);
    Log->print(F("Nuki Hub build ")); Log->println(NUKI_HUB_BUILD);

    if(preferences->getBool(preference_enable_bootloop_reset, false) &&
    (esp_reset_reason() == esp_reset_reason_t::ESP_RST_PANIC ||
    esp_reset_reason() == esp_reset_reason_t::ESP_RST_INT_WDT ||
    esp_reset_reason() == esp_reset_reason_t::ESP_RST_TASK_WDT ||
    esp_reset_reason() == esp_reset_reason_t::ESP_RST_WDT))
    {
        preferences->putInt(preference_bootloop_counter, preferences->getInt(preference_bootloop_counter, 0) + 1);
        Log->println(F("Bootloop counter incremented"));

        if(preferences->getInt(preference_bootloop_counter) == 10)
        {
            preferences->putInt(preference_buffer_size, CHAR_BUFFER_SIZE);
            preferences->putInt(preference_task_size_network, NETWORK_TASK_SIZE);
            preferences->putInt(preference_task_size_nuki, NUKI_TASK_SIZE);
            preferences->putInt(preference_authlog_max_entries, MAX_AUTHLOG);
            preferences->putInt(preference_keypad_max_entries, MAX_KEYPAD);
            preferences->putInt(preference_timecontrol_max_entries, MAX_TIMECONTROL);
            preferences->putInt(preference_bootloop_counter, 0);
        }
    }

    uint32_t devIdOpener = preferences->getUInt(preference_device_id_opener);

    deviceIdLock = new NukiDeviceId(preferences, preference_device_id_lock);
    deviceIdOpener = new NukiDeviceId(preferences, preference_device_id_opener);

    if(deviceIdLock->get() != 0 && devIdOpener == 0)
    {
        deviceIdOpener->assignId(deviceIdLock->get());
    }

    char16_t buffer_size = preferences->getInt(preference_buffer_size, 4096);

    CharBuffer::initialize(buffer_size);

    if(preferences->getInt(preference_restart_timer) != 0)
    {
        preferences->remove(preference_restart_timer);
    }

    gpio = new Gpio(preferences);
    String gpioDesc;
    gpio->getConfigurationText(gpioDesc, gpio->pinConfiguration(), "\n\r");
    Serial.print(gpioDesc.c_str());

    bleScanner = new BleScanner::Scanner();
    bleScanner->initialize("NukiHub");
    bleScanner->setScanDuration(10*1000);

    if(preferences->getInt(preference_presence_detection_timeout) >= 0)
    {
        presenceDetection = new PresenceDetection(preferences, bleScanner, CharBuffer::get(), buffer_size);
        presenceDetection->initialize();
    }

    lockEnabled = preferences->getBool(preference_lock_enabled);
    openerEnabled = preferences->getBool(preference_opener_enabled);

    const String mqttLockPath = preferences->getString(preference_mqtt_lock_path);

    network = new NukiNetwork(preferences, presenceDetection, gpio, mqttLockPath, CharBuffer::get(), buffer_size);
    network->initialize();

    networkLock = new NukiNetworkLock(network, preferences, CharBuffer::get(), buffer_size);
    networkLock->initialize();

    if(openerEnabled)
    {
        networkOpener = new NukiNetworkOpener(network, preferences, CharBuffer::get(), buffer_size);
        networkOpener->initialize();
    }

    initEthServer(network->networkDeviceType());

    Log->println(lockEnabled ? F("Nuki Lock enabled") : F("Nuki Lock disabled"));
    if(lockEnabled)
    {
        nuki = new NukiWrapper("NukiHub", deviceIdLock, bleScanner, networkLock, gpio, preferences);
        nuki->initialize(firstStart);
    }

    Log->println(openerEnabled ? F("Nuki Opener enabled") : F("Nuki Opener disabled"));
    if(openerEnabled)
    {
        nukiOpener = new NukiOpenerWrapper("NukiHub", deviceIdOpener, bleScanner, networkOpener, gpio, preferences);
        nukiOpener->initialize();
    }

    if(preferences->getBool(preference_webserver_enabled, true))
    {
        webCfgServer = new WebCfgServer(nuki, nukiOpener, network, gpio, ethServer, preferences, network->networkDeviceType() == NetworkDeviceType::WiFi, partitionType);
        webCfgServer->initialize();
    }
    #endif

    if((partitionType==1 && preferences->getString(preference_ota_updater_url).length() > 0) || (partitionType==2 && preferences->getString(preference_ota_main_url).length() > 0)) setupTasks(true);
    else setupTasks(false);
}

void loop()
{
    vTaskDelete(NULL);
}

void printBeforeSetupInfo()
{
}

void printAfterSetupInfo()
{
}