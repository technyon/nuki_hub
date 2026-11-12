/*
Copyright (c) 2022 Bert Melis. All rights reserved.

This work is licensed under the terms of the MIT license.  
For a copy, see <https://opensource.org/licenses/MIT> or
the LICENSE file.
*/

#pragma once

#if defined(ARDUINO_ARCH_ESP32)
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "RuntimeLog.h"
  // Logging is enabled at runtime through emcLogEnabled and delivered through emcLogHandler
    #define emc_log_i(...) do { if (emcLogEnabled) emcLogLine('I', __VA_ARGS__); } while (0)
    #define emc_log_e(...) do { if (emcLogEnabled) emcLogLine('E', __VA_ARGS__); } while (0)
    #define emc_log_w(...) do { if (emcLogEnabled) emcLogLine('W', __VA_ARGS__); } while (0)
    #define emc_log_t(...)
#elif defined(ARDUINO_ARCH_ESP8266)
  #if defined(DEBUG_ESP_PORT) && defined(DEBUG_ESP_MQTT_CLIENT)
    #include <Arduino.h>
    #define emc_log_i(...) DEBUG_ESP_PORT.printf(__VA_ARGS__); DEBUG_ESP_PORT.print("\n")
    #define emc_log_e(...) DEBUG_ESP_PORT.printf(__VA_ARGS__); DEBUG_ESP_PORT.print("\n")
    #define emc_log_w(...) DEBUG_ESP_PORT.printf(__VA_ARGS__); DEBUG_ESP_PORT.print("\n")
    #define emc_log_t(...) emc_log_i(__VA_ARGS__)
  #else
    #define emc_log_i(...)
    #define emc_log_e(...)
    #define emc_log_w(...)
    #define emc_log_t(...)
  #endif
#else
  // when building for PC, always show debug statements as part of testing suite
  #include <iostream>
  #define emc_log_i(...) std::cout << "[I] " << __FILE__ ":" << __LINE__ << ": "; printf(__VA_ARGS__); std::cout << std::endl
  #define emc_log_e(...) std::cout << "[E] " << __FILE__ ":" << __LINE__ << ": "; printf(__VA_ARGS__); std::cout << std::endl
  #define emc_log_w(...) std::cout << "[W] " << __FILE__ ":" << __LINE__ << ": "; printf(__VA_ARGS__); std::cout << std::endl
  #define emc_log_t(...) emc_log_i(__VA_ARGS__)
#endif
