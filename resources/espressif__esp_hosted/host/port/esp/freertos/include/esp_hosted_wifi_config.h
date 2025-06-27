/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __ESP_HOSTED_WIFI_CONFIG_H__
#define __ESP_HOSTED_WIFI_CONFIG_H__

#include "esp_idf_version.h"
#include "esp_hosted_config.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
  /* dual band API support available */
  #define H_WIFI_DUALBAND_SUPPORT 1
#else
  #define H_WIFI_DUALBAND_SUPPORT 0
#endif

/* ESP-IDF 5.5.0 breaking change: reserved/he_reserved renamed to reserved1/reserved2 */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  #define H_WIFI_NEW_RESERVED_FIELD_NAMES 1
  #define H_WIFI_VHT_FIELDS_AVAILABLE 1
#else
  #define H_WIFI_NEW_RESERVED_FIELD_NAMES 0
  #define H_WIFI_VHT_FIELDS_AVAILABLE 0
#endif

/* User-controllable reserved field decoding - works regardless of IDF version */
#ifdef CONFIG_ESP_HOSTED_DECODE_WIFI_RESERVED_FIELD
  #define H_DECODE_WIFI_RESERVED_FIELD 1
#else
  #define H_DECODE_WIFI_RESERVED_FIELD 0
#endif

#endif /* __ESP_HOSTED_WIFI_CONFIG_H__ */
