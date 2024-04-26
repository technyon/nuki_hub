#pragma once

#include "sdkconfig.h"

#define NUKI_HUB_VERSION "8.34-pre-4"

#define GITHUB_LATEST_RELEASE_URL "https://github.com/technyon/nuki_hub/releases/latest"
#define GITHUB_LATEST_RELEASE_API_URL "https://api.github.com/repos/technyon/nuki_hub/releases/latest"

#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define GITHUB_LATEST_RELEASE_BINARY_URL "https://github.com/technyon/nuki_hub/raw/master/ota/nuki_hub_esp32c3.bin"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define GITHUB_LATEST_RELEASE_BINARY_URL "https://github.com/technyon/nuki_hub/raw/master/ota/nuki_hub_esp32s3.bin"
#else
#if defined(FRAMEWORK_ARDUINO_SOLO1)
#define GITHUB_LATEST_RELEASE_BINARY_URL "https://github.com/technyon/nuki_hub/raw/master/ota/nuki_hub_esp32solo1.bin"
#else    
#define GITHUB_LATEST_RELEASE_BINARY_URL "https://github.com/technyon/nuki_hub/raw/master/ota/nuki_hub_esp32.bin"
#endif
#endif

#define MQTT_QOS_LEVEL 1
#define MQTT_CLEAN_SESSIONS false

#define GPIO_DEBOUNCE_TIME 200
