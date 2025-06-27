/*
* SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#ifndef __ESP_HOSTED_H__
#define __ESP_HOSTED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_hosted_config.h"
#include "esp_hosted_bt_config.h"
#include "esp_hosted_transport_config.h"
#include "esp_hosted_api_types.h"
#include "esp_hosted_ota.h"

typedef struct esp_hosted_transport_config esp_hosted_config_t;

/* --------- Hosted Minimal APIs --------- */
int esp_hosted_init(void);
int esp_hosted_deinit(void);

int esp_hosted_connect_to_slave(void);
int esp_hosted_get_coprocessor_fwversion(esp_hosted_coprocessor_fwver_t *ver_info);

/* --------- Exhaustive API list --------- */
/*
 * 1. All Wi-Fi supported APIs
 *    File: host/api/src/esp_wifi_weak.c
 *
 * 2. Communication Bus APIs (Set and get transport config)
 *    File : host/api/include/esp_hosted_transport_config.h
 *
 * 3. Co-Processor OTA API
 *    File : host/api/include/esp_hosted_ota.h
 */

#ifdef __cplusplus
}
#endif

#endif /* __ESP_HOSTED_H__ */
