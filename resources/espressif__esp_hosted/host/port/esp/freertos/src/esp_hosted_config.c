/*
* SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#include "esp_hosted_config.h"
#include "esp_hosted_transport_config.h"

esp_err_t esp_hosted_set_default_config(void) {
  return esp_hosted_transport_set_default_config();
}

bool esp_hosted_is_config_valid(void) {
  return esp_hosted_transport_is_config_valid();
}
