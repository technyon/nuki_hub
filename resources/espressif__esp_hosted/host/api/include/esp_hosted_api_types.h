/*
* SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

/** prevent recursive inclusion **/
#ifndef __ESP_HOSTED_API_TYPES_H__
#define __ESP_HOSTED_API_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint32_t major1;
	uint32_t minor1;
	uint32_t patch1;
} esp_hosted_coprocessor_fwver_t;

#ifdef __cplusplus
}
#endif

#endif
