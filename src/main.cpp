#define IS_VALID_DETECT 0xa00ab00bc00bd00d;

#include "Arduino.h"
#include "esp_crt_bundle.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include <esp_task_wdt.h>
#include "Config.h"

#ifndef NUKI_HUB_UPDATER
#include "NukiWrapper.h"
#include "NukiNetworkLock.h"
#include "NukiOpenerWrapper.h"
#include "Gpio.h"
#include "CharBuffer.h"
#include "NukiDeviceId.h"
#include "WebCfgServer.h"
#include "Logger.h"
#include "PreferencesKeys.h"
#include "RestartReason.h"
/*
#ifdef DEBUG_NUKIHUB
#include <WString.h>
#include <MycilaWebSerial.h>
#endif
*/

char log_print_buffer[1024];

NukiNetworkLock* networkLock = nullptr;
NukiNetworkOpener* networkOpener = nullptr;
BleScanner::Scanner* bleScanner = nullptr;
NukiWrapper* nuki = nullptr;
NukiOpenerWrapper* nukiOpener = nullptr;
NukiDeviceId* deviceIdLock = nullptr;
NukiDeviceId* deviceIdOpener = nullptr;
Gpio* gpio = nullptr;

bool lockEnabled = false;
bool openerEnabled = false;

TaskHandle_t nukiTaskHandle = nullptr;

int64_t restartTs = ((2^64) - (5 * 1000 * 60000)) / 1000;

#else
#include "../../src/WebCfgServer.h"
#include "../../src/Logger.h"
#include "../../src/PreferencesKeys.h"
#include "../../src/RestartReason.h"
#include "../../src/NukiNetwork.h"

int64_t restartTs = 10 * 1000 * 60000;

#endif

PsychicHttpServer* psychicServer = nullptr;
NukiNetwork* network = nullptr;
WebCfgServer* webCfgServer = nullptr;
Preferences* preferences = nullptr;

RTC_NOINIT_ATTR int restartReason;
RTC_NOINIT_ATTR uint64_t restartReasonValidDetect;
RTC_NOINIT_ATTR bool rebuildGpioRequested;
RTC_NOINIT_ATTR uint64_t bootloopValidDetect;
RTC_NOINIT_ATTR int8_t bootloopCounter;
RTC_NOINIT_ATTR bool forceEnableWebServer;

bool restartReason_isValid;
RestartReason currentRestartReason = RestartReason::NotApplicable;

TaskHandle_t otaTaskHandle = nullptr;
TaskHandle_t networkTaskHandle = nullptr;

#ifndef NUKI_HUB_UPDATER
ssize_t write_fn(void* cookie, const char* buf, ssize_t size)
{
  Log->write((uint8_t *)buf, (size_t)size);

  return size;
}

void ets_putc_handler(char c)
{
    static char buf[1024];
    static size_t buf_pos = 0;
    buf[buf_pos] = c;
    buf_pos++;
    if (c == '\n' || buf_pos == sizeof(buf)) {
        write_fn(NULL, buf, buf_pos);
        buf_pos = 0;
    }
}

int _log_vprintf(const char *fmt, va_list args) {
    int ret = vsnprintf(log_print_buffer, sizeof(log_print_buffer), fmt, args);
    if (ret >= 0){
        Log->write((uint8_t *)log_print_buffer, (size_t)ret);
    }
    return 0; //return vprintf(fmt, args);
}

void setReroute(){
    esp_log_set_vprintf(_log_vprintf);
    if(preferences->getBool(preference_mqtt_log_enabled)) esp_log_level_set("*", ESP_LOG_INFO);
    else
    {
        esp_log_level_set("*", ESP_LOG_DEBUG);
        esp_log_level_set("nvs", ESP_LOG_INFO);
        esp_log_level_set("wifi", ESP_LOG_INFO);
    }
}
#endif

void networkTask(void *pvParameters)
{
    int64_t networkLoopTs = 0;
    bool reroute = true;
    if(preferences->getBool(preference_show_secrets, false))
    {
        preferences->putBool(preference_show_secrets, false);
    }
    while(true)
    {
        int64_t ts = (esp_timer_get_time() / 1000);
        if(ts > 120000 && ts < 125000)
        {
            if(bootloopCounter > 0)
            {
                bootloopCounter = (int8_t)0;
                Log->println(F("Bootloop counter reset"));
            }
        }

        bool connected = network->update();

        #ifndef NUKI_HUB_UPDATER
        #ifdef DEBUG_NUKIHUB
        if(connected && reroute)
        {
            reroute = false;
            setReroute();
        }
        #endif
        if(connected && openerEnabled) networkOpener->update();
        #endif

        if((esp_timer_get_time() / 1000) - networkLoopTs > 120000)
        {
            Log->println("networkTask is running");
            networkLoopTs = esp_timer_get_time() / 1000;
        }

        esp_task_wdt_reset();
        delay(100);
    }
}

#ifndef NUKI_HUB_UPDATER
void nukiTask(void *pvParameters)
{
    int64_t nukiLoopTs = 0;
    bool whiteListed = false;

    while(true)
    {
        bleScanner->update();
        delay(20);

        bool needsPairing = (lockEnabled && !nuki->isPaired()) || (openerEnabled && !nukiOpener->isPaired());

        if (needsPairing)
        {
            delay(5000);
        }
        else if (!whiteListed)
        {
            whiteListed = true;
            if(lockEnabled)
            {
                bleScanner->whitelist(nuki->getBleAddress());
            }
            if(openerEnabled)
            {
                bleScanner->whitelist(nukiOpener->getBleAddress());
            }
        }

        if(lockEnabled)
        {
            nuki->update();
        }
        if(openerEnabled)
        {
            nukiOpener->update();
        }

        if((esp_timer_get_time() / 1000) - nukiLoopTs > 120000)
        {
            Log->println("nukiTask is running");
            nukiLoopTs = esp_timer_get_time() / 1000;
        }

        esp_task_wdt_reset();
    }
}

void bootloopDetection()
{
    uint64_t cmp = IS_VALID_DETECT;
    bool bootloopIsValid = (bootloopValidDetect == cmp);
    Log->println(bootloopIsValid);

    if(!bootloopIsValid)
    {
        bootloopCounter = (int8_t)0;
        bootloopValidDetect = IS_VALID_DETECT;
        return;
    }

    if(esp_reset_reason() == esp_reset_reason_t::ESP_RST_PANIC ||
        esp_reset_reason() == esp_reset_reason_t::ESP_RST_INT_WDT ||
        esp_reset_reason() == esp_reset_reason_t::ESP_RST_TASK_WDT ||
        true ||
        esp_reset_reason() == esp_reset_reason_t::ESP_RST_WDT)
    {
        bootloopCounter++;
        Log->print(F("Bootloop counter incremented: "));
        Log->println(bootloopCounter);

        if(bootloopCounter == 10)
        {
            Log->print(F("Bootloop detected."));

            preferences->putInt(preference_buffer_size, CHAR_BUFFER_SIZE);
            preferences->putInt(preference_task_size_network, NETWORK_TASK_SIZE);
            preferences->putInt(preference_task_size_nuki, NUKI_TASK_SIZE);
            preferences->putInt(preference_authlog_max_entries, MAX_AUTHLOG);
            preferences->putInt(preference_keypad_max_entries, MAX_KEYPAD);
            preferences->putInt(preference_timecontrol_max_entries, MAX_TIMECONTROL);
            preferences->putInt(preference_auth_max_entries, MAX_AUTH);
            bootloopCounter = 0;
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
        case HTTP_EVENT_REDIRECT:
            Log->println("HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

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

    int retryMax = 3;
    int retryCount = 0;

    while (retryCount <= retryMax)
    {
        esp_err_t ret = esp_https_ota(&ota_config);
        if (ret == ESP_OK) {
            Log->println("OTA Succeeded, Rebooting...");
            esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL));
            restartEsp(RestartReason::OTACompleted);
            break;
        } else {
            Log->println("Firmware upgrade failed, retrying in 5 seconds");
            retryCount++;
            esp_task_wdt_reset();
            delay(5000);
            continue;
        }
        while (1) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    Log->println("Firmware upgrade failed, restarting");
    esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL));
    restartEsp(RestartReason::OTAAborted);
}

void setupTasks(bool ota)
{
    // configMAX_PRIORITIES is 25
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 300000,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    esp_task_wdt_reconfigure(&twdt_config);

    if(ota)
    {
        xTaskCreatePinnedToCore(otaTask, "ota", 8192, NULL, 2, &otaTaskHandle, 1);
        esp_task_wdt_add(otaTaskHandle);
    }
    else
    {
        xTaskCreatePinnedToCore(networkTask, "ntw", preferences->getInt(preference_task_size_network, NETWORK_TASK_SIZE), NULL, 3, &networkTaskHandle, 1);
        esp_task_wdt_add(networkTaskHandle);
        #ifndef NUKI_HUB_UPDATER
        xTaskCreatePinnedToCore(nukiTask, "nuki", preferences->getInt(preference_task_size_nuki, NUKI_TASK_SIZE), NULL, 2, &nukiTaskHandle, 0);
        esp_task_wdt_add(nukiTaskHandle);
        #endif
    }
}

void setup()
{
    esp_log_level_set("*", ESP_LOG_ERROR);
    Serial.begin(115200);
    Log = &Serial;

    #ifndef NUKI_HUB_UPDATER
    stdout = funopen(NULL, NULL, &write_fn, NULL, NULL);
    static char linebuf[1024];
    setvbuf(stdout, linebuf, _IOLBF, sizeof(linebuf));
    esp_rom_install_channel_putc(1, &ets_putc_handler);
    //ets_install_putc1(&ets_putc_handler);
    #endif

    preferences = new Preferences();
    preferences->begin("nukihub", false);
    bool firstStart = initPreferences(preferences);
    bool doOta = false;
    uint8_t partitionType = checkPartition();

    initializeRestartReason();

    if((partitionType==1 && preferences->getString(preference_ota_updater_url, "").length() > 0) || (partitionType==2 && preferences->getString(preference_ota_main_url, "").length() > 0)) doOta = true;

    #ifndef NUKI_HUB_UPDATER
    if(preferences->getBool(preference_enable_bootloop_reset, false))
    {
        bootloopDetection();
    }
    #endif

    #ifdef NUKI_HUB_UPDATER
    Log->print(F("Nuki Hub OTA version "));
    Log->println(NUKI_HUB_VERSION);
    Log->print(F("Nuki Hub OTA build "));
    Log->println();

    if(preferences->getString(preference_updater_version, "") != NUKI_HUB_VERSION) preferences->putString(preference_updater_version, NUKI_HUB_VERSION);
    if(preferences->getString(preference_updater_build, "") != NUKI_HUB_BUILD) preferences->putString(preference_updater_build, NUKI_HUB_BUILD);
    if(preferences->getString(preference_updater_date, "") != NUKI_HUB_DATE) preferences->putString(preference_updater_date, NUKI_HUB_DATE);

    network = new NukiNetwork(preferences);
    network->initialize();

    if(!doOta)
    {
        psychicServer = new PsychicHttpServer;
        webCfgServer = new WebCfgServer(network, preferences, network->networkDeviceType() == NetworkDeviceType::WiFi, partitionType, psychicServer);
        webCfgServer->initialize();
        psychicServer->onNotFound([](PsychicRequest* request) { return request->redirect("/"); });
        psychicServer->listen(80);
    }
    #else
    Log->print(F("Nuki Hub version "));
    Log->println(NUKI_HUB_VERSION);
    Log->print(F("Nuki Hub build "));
    Log->println(NUKI_HUB_BUILD);

    uint32_t devIdOpener = preferences->getUInt(preference_device_id_opener);

    deviceIdLock = new NukiDeviceId(preferences, preference_device_id_lock);
    deviceIdOpener = new NukiDeviceId(preferences, preference_device_id_opener);

    if(deviceIdLock->get() != 0 && devIdOpener == 0)
    {
        deviceIdOpener->assignId(deviceIdLock->get());
    }

    char16_t buffer_size = preferences->getInt(preference_buffer_size, 4096);

    CharBuffer::initialize(buffer_size);

    gpio = new Gpio(preferences);
    String gpioDesc;
    gpio->getConfigurationText(gpioDesc, gpio->pinConfiguration(), "\n\r");
    Log->print(gpioDesc.c_str());

    bleScanner = new BleScanner::Scanner();
    // Scan interval and window according to Nuki recommendations:
    // https://developer.nuki.io/t/bluetooth-specification-questions/1109/27
    bleScanner->initialize("NukiHub", true, 40, 40);
    bleScanner->setScanDuration(0);

    lockEnabled = preferences->getBool(preference_lock_enabled);
    openerEnabled = preferences->getBool(preference_opener_enabled);

    const String mqttLockPath = preferences->getString(preference_mqtt_lock_path);

    network = new NukiNetwork(preferences, gpio, mqttLockPath, CharBuffer::get(), buffer_size);
    network->initialize();

    networkLock = new NukiNetworkLock(network, preferences, CharBuffer::get(), buffer_size);
    networkLock->initialize();

    if(openerEnabled)
    {
        networkOpener = new NukiNetworkOpener(network, preferences, CharBuffer::get(), buffer_size);
        networkOpener->initialize();
    }

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

    if(forceEnableWebServer || preferences->getBool(preference_webserver_enabled, true) || preferences->getBool(preference_webserial_enabled, false))
    {
        if(!doOta)
        {
            psychicServer = new PsychicHttpServer;
            psychicServer->config.max_uri_handlers = 40;
            psychicServer->config.stack_size = 8192;
            psychicServer->listen(80);

            if(forceEnableWebServer || preferences->getBool(preference_webserver_enabled, true))
            {
                webCfgServer = new WebCfgServer(nuki, nukiOpener, network, gpio, preferences, network->networkDeviceType() == NetworkDeviceType::WiFi, partitionType, psychicServer);
                webCfgServer->initialize();
                psychicServer->onNotFound([](PsychicRequest* request) { return request->redirect("/"); });
            }
            /*
            #ifdef DEBUG_NUKIHUB
            else psychicServer->onNotFound([](PsychicRequest* request) { return request->redirect("/webserial"); });

            if(preferences->getBool(preference_webserial_enabled, false))
            {
              WebSerial.setAuthentication(preferences->getString(preference_cred_user), preferences->getString(preference_cred_password));
              WebSerial.begin(asyncServer);
              WebSerial.setBuffer(1024);
            }
            #endif
            */
        }
    }
    #endif

    if(doOta) setupTasks(true);
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