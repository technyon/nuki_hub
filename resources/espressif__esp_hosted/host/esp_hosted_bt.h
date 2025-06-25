// Copyright 2015-2024 Espressif Systems (Shanghai) PTE LTD
/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */

#ifndef __ESP_HOSTED_BT_H
#define __ESP_HOSTED_BT_H

#include "esp_hosted_bt_config.h"

#if H_BT_HOST_ESP_BLUEDROID
#include "esp_bluedroid_hci.h"

// BT Bluedroid interface for Host
void hosted_hci_bluedroid_open(void);
void hosted_hci_bluedroid_close(void);
void hosted_hci_bluedroid_send(uint8_t *data, uint16_t len);
bool hosted_hci_bluedroid_check_send_available(void);
esp_err_t hosted_hci_bluedroid_register_host_callback(const esp_bluedroid_hci_driver_callbacks_t *callback);

#endif // H_BT_HOST_ESP_BLUEDROID

#endif // __ESP_HOSTED_BT_H
