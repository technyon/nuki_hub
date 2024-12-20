#pragma once

#include "sdkconfig.h"

#define NUKI_HUB_VERSION "9.05"
#define NUKI_HUB_VERSION_INT (uint32_t)905
#define NUKI_HUB_BUILD "unknownbuildnr"
#define NUKI_HUB_DATE "2024-12-20"

#define GITHUB_LATEST_RELEASE_URL (char*)"https://github.com/technyon/nuki_hub/releases/latest"
#define GITHUB_OTA_MANIFEST_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/manifest.json"

#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define GITHUB_LATEST_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/nuki_hub_esp32c3.bin"
#define GITHUB_LATEST_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/nuki_hub_updater_esp32c3.bin"
#define GITHUB_BETA_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/beta/nuki_hub_esp32c3.bin"
#define GITHUB_BETA_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/beta/nuki_hub_updater_esp32c3.bin"
#define GITHUB_MASTER_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/master/nuki_hub_esp32c3.bin"
#define GITHUB_MASTER_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/master/nuki_hub_updater_esp32c3.bin"
#define GITHUB_LATEST_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/nuki_hub_esp32c3.bin"
#define GITHUB_LATEST_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/nuki_hub_updater_esp32c3.bin"
#define GITHUB_BETA_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/beta/nuki_hub_esp32c3.bin"
#define GITHUB_BETA_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/beta/nuki_hub_updater_esp32c3.bin"
#define GITHUB_MASTER_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/master/nuki_hub_esp32c3.bin"
#define GITHUB_MASTER_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/master/nuki_hub_updater_esp32c3.bin"
#define NUKI_HUB_HW (char*)"ESP32-C3"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#if defined(CONFIG_SPIRAM_MODE_OCT)
#define GITHUB_LATEST_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/nuki_hub_esp32s3oct.bin"
#define GITHUB_LATEST_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/nuki_hub_updater_esp32s3oct.bin"
#define GITHUB_BETA_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/beta/nuki_hub_esp32s3oct.bin"
#define GITHUB_BETA_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/beta/nuki_hub_updater_esp32s3oct.bin"
#define GITHUB_MASTER_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/master/nuki_hub_esp32s3oct.bin"
#define GITHUB_MASTER_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/master/nuki_hub_updater_esp32s3oct.bin"
#define GITHUB_LATEST_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/nuki_hub_esp32s3oct.bin"
#define GITHUB_LATEST_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/nuki_hub_updater_esp32s3oct.bin"
#define GITHUB_BETA_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/beta/nuki_hub_esp32s3oct.bin"
#define GITHUB_BETA_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/beta/nuki_hub_updater_esp32s3oct.bin"
#define GITHUB_MASTER_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/master/nuki_hub_esp32s3oct.bin"
#define GITHUB_MASTER_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/master/nuki_hub_updater_esp32s3oct.bin"
#define NUKI_HUB_HW (char*)"ESP32-S3 (Octal PSRAM)"
#else
#define GITHUB_LATEST_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/nuki_hub_esp32s3.bin"
#define GITHUB_LATEST_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/nuki_hub_updater_esp32s3.bin"
#define GITHUB_BETA_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/beta/nuki_hub_esp32s3.bin"
#define GITHUB_BETA_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/beta/nuki_hub_updater_esp32s3.bin"
#define GITHUB_MASTER_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/master/nuki_hub_esp32s3.bin"
#define GITHUB_MASTER_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/master/nuki_hub_updater_esp32s3.bin"
#define GITHUB_LATEST_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/nuki_hub_esp32s3.bin"
#define GITHUB_LATEST_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/nuki_hub_updater_esp32s3.bin"
#define GITHUB_BETA_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/beta/nuki_hub_esp32s3.bin"
#define GITHUB_BETA_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/beta/nuki_hub_updater_esp32s3.bin"
#define GITHUB_MASTER_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/master/nuki_hub_esp32s3.bin"
#define GITHUB_MASTER_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/master/nuki_hub_updater_esp32s3.bin"
#define NUKI_HUB_HW (char*)"ESP32-S3"
#endif
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
#define GITHUB_LATEST_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/nuki_hub_esp32c6.bin"
#define GITHUB_LATEST_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/nuki_hub_updater_esp32c6.bin"
#define GITHUB_BETA_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/beta/nuki_hub_esp32c6.bin"
#define GITHUB_BETA_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/beta/nuki_hub_updater_esp32c6.bin"
#define GITHUB_MASTER_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/master/nuki_hub_esp32c6.bin"
#define GITHUB_MASTER_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/master/nuki_hub_updater_esp32c6.bin"
#define GITHUB_LATEST_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/nuki_hub_esp32c6.bin"
#define GITHUB_LATEST_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/nuki_hub_updater_esp32c6.bin"
#define GITHUB_BETA_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/beta/nuki_hub_esp32c6.bin"
#define GITHUB_BETA_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/beta/nuki_hub_updater_esp32c6.bin"
#define GITHUB_MASTER_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/master/nuki_hub_esp32c6.bin"
#define GITHUB_MASTER_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/master/nuki_hub_updater_esp32c6.bin"
#define NUKI_HUB_HW (char*)"ESP32-C6"
#elif defined(CONFIG_IDF_TARGET_ESP32H2)
#define GITHUB_LATEST_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/nuki_hub_esp32h2.bin"
#define GITHUB_LATEST_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/nuki_hub_updater_esp32h2.bin"
#define GITHUB_BETA_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/beta/nuki_hub_esp32h2.bin"
#define GITHUB_BETA_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/beta/nuki_hub_updater_esp32h2.bin"
#define GITHUB_MASTER_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/master/nuki_hub_esp32h2.bin"
#define GITHUB_MASTER_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/master/nuki_hub_updater_esp32h2.bin"
#define GITHUB_LATEST_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/nuki_hub_esp32h2.bin"
#define GITHUB_LATEST_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/nuki_hub_updater_esp32h2.bin"
#define GITHUB_BETA_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/beta/nuki_hub_esp32h2.bin"
#define GITHUB_BETA_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/beta/nuki_hub_updater_esp32h2.bin"
#define GITHUB_MASTER_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/master/nuki_hub_esp32h2.bin"
#define GITHUB_MASTER_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/master/nuki_hub_updater_esp32h2.bin"
#define NUKI_HUB_HW (char*)"ESP32-H2"
#else
#if defined(CONFIG_FREERTOS_UNICORE)
#define GITHUB_LATEST_RELEASE_BINARY_URL "https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/nuki_hub_esp32-solo1.bin"
#define GITHUB_LATEST_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/nuki_hub_updater_esp32-solo1.bin"
#define GITHUB_BETA_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/beta/nuki_hub_esp32-solo1.bin"
#define GITHUB_BETA_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/beta/nuki_hub_updater_esp32-solo1.bin"
#define GITHUB_MASTER_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/master/nuki_hub_esp32-solo1.bin"
#define GITHUB_MASTER_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/master/nuki_hub_updater_esp32-solo1.bin"
#define GITHUB_LATEST_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/nuki_hub_esp32-solo1.bin"
#define GITHUB_LATEST_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/nuki_hub_updater_esp32-solo1.bin"
#define GITHUB_BETA_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/beta/nuki_hub_esp32-solo1.bin"
#define GITHUB_BETA_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/beta/nuki_hub_updater_esp32-solo1.bin"
#define GITHUB_MASTER_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/master/nuki_hub_esp32-solo1.bin"
#define GITHUB_MASTER_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/master/nuki_hub_updater_esp32-solo1.bin"
#define NUKI_HUB_HW (char*)"ESP32-SOLO1"
#else
#define GITHUB_LATEST_RELEASE_BINARY_URL "https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/nuki_hub_esp32.bin"
#define GITHUB_LATEST_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/nuki_hub_updater_esp32.bin"
#define GITHUB_BETA_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/beta/nuki_hub_esp32.bin"
#define GITHUB_BETA_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/beta/nuki_hub_updater_esp32.bin"
#define GITHUB_MASTER_RELEASE_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/master/nuki_hub_esp32.bin"
#define GITHUB_MASTER_UPDATER_BINARY_URL (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/master/nuki_hub_updater_esp32.bin"
#define GITHUB_LATEST_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/nuki_hub_esp32.bin"
#define GITHUB_LATEST_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/nuki_hub_updater_esp32.bin"
#define GITHUB_BETA_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/beta/nuki_hub_esp32.bin"
#define GITHUB_BETA_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/beta/nuki_hub_updater_esp32.bin"
#define GITHUB_MASTER_RELEASE_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/master/nuki_hub_esp32.bin"
#define GITHUB_MASTER_UPDATER_BINARY_URL_DBG (char*)"https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/debug/master/nuki_hub_updater_esp32.bin"
#define NUKI_HUB_HW (char*)"ESP32"
#endif
#endif

#ifndef NUKI_HUB_UPDATER
#define MQTT_QOS_LEVEL 1
#define MQTT_CLEAN_SESSIONS false
#define MQTT_KEEP_ALIVE 60
#define GPIO_DEBOUNCE_TIME 200
#define CHAR_BUFFER_SIZE 4096
#define NUKI_TASK_SIZE 8192
#define MAX_AUTHLOG 5
#define MAX_KEYPAD 10
#define MAX_TIMECONTROL 10
#define MAX_AUTH 10
#endif

#define NETWORK_TASK_SIZE 12288
#define HTTPD_TASK_SIZE 8192
