// SPDX-License-Identifier: Apache-2.0
// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <stdint.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#if CONFIG_BT_ENABLED
#include "esp_bt.h"
#endif
#include "slave_bt.h"
#include "slave_bt_uart.h"
#include "interface.h"
#include "esp_hosted_transport.h"
#include "esp_hosted_transport_init.h"
#include "esp_hosted_interface.h"

#ifdef CONFIG_BT_ENABLED
#include "esp_log.h"
#include "esp_hosted_log.h"
#include "soc/lldesc.h"
#include "esp_mac.h"

static const char *TAG = "h_bt";

#if BLUETOOTH_HCI
/* ***** HCI specific part ***** */

#define VHCI_MAX_TIMEOUT_MS 	2000
static SemaphoreHandle_t vhci_send_sem;

static void controller_rcv_pkt_ready(void)
{
	if (vhci_send_sem)
		xSemaphoreGive(vhci_send_sem);
}

static int host_rcv_pkt(uint8_t *data, uint16_t len)
{
	interface_buffer_handle_t buf_handle;
	uint8_t *buf = NULL;

	buf = (uint8_t *) malloc(len);

	if (!buf) {
		ESP_LOGE(TAG, "HCI Send packet: memory allocation failed");
		return ESP_FAIL;
	}

	memcpy(buf, data, len);

	memset(&buf_handle, 0, sizeof(buf_handle));

	buf_handle.if_type = ESP_HCI_IF;
	buf_handle.if_num = 0;
	buf_handle.payload_len = len;
	buf_handle.payload = buf;
	buf_handle.wlan_buf_handle = buf;
	buf_handle.free_buf_handle = free;

	ESP_HEXLOGV("bt_tx new", data, len);

	if (send_to_host_queue(&buf_handle, PRIO_Q_BT)) {
		free(buf);
		return ESP_FAIL;
	}

	return 0;
}

static esp_vhci_host_callback_t vhci_host_cb = {
	.notify_host_send_available = controller_rcv_pkt_ready,
	.notify_host_recv = host_rcv_pkt
};

void process_hci_rx_pkt(uint8_t *payload, uint16_t payload_len)
{
	/* VHCI needs one extra byte at the start of payload */
	/* that is accomodated in esp_payload_header */
	ESP_HEXLOGV("bt_rx", payload, payload_len);

	payload--;
	payload_len++;

	if (!esp_vhci_host_check_send_available()) {
		ESP_LOGD(TAG, "VHCI not available");
	}

#if SOC_ESP_NIMBLE_CONTROLLER
	esp_vhci_host_send_packet(payload, payload_len);
#else
	if (vhci_send_sem) {
		if (xSemaphoreTake(vhci_send_sem, VHCI_MAX_TIMEOUT_MS) == pdTRUE) {
			esp_vhci_host_send_packet(payload, payload_len);
		} else {
			ESP_LOGI(TAG, "VHCI sem timeout");
		}
	}
#endif
}

#elif BLUETOOTH_UART
/* ***** UART specific part ***** */

#endif

#if BLUETOOTH_HCI
#if SOC_ESP_NIMBLE_CONTROLLER

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 0)
#include "nimble/ble_hci_trans.h"

typedef enum {
    DATA_TYPE_COMMAND = 1,
    DATA_TYPE_ACL     = 2,
    DATA_TYPE_SCO     = 3,
    DATA_TYPE_EVENT   = 4
} serial_data_type_t;

/* Host-to-controller command. */
#define BLE_HCI_TRANS_BUF_CMD       3

/* ACL_DATA_MBUF_LEADINGSPACE: The leadingspace in user info header for ACL data */
#define ACL_DATA_MBUF_LEADINGSPACE    4

void esp_vhci_host_send_packet(uint8_t *data, uint16_t len)
{
    if (*(data) == DATA_TYPE_COMMAND) {
        struct ble_hci_cmd *cmd = NULL;
        cmd = (struct ble_hci_cmd *) ble_hci_trans_buf_alloc(BLE_HCI_TRANS_BUF_CMD);
	if (!cmd) {
		ESP_LOGE(TAG, "Failed to allocate memory for HCI transport buffer");
		return;
	}

        memcpy((uint8_t *)cmd, data + 1, len - 1);
        ble_hci_trans_hs_cmd_tx((uint8_t *)cmd);
    }

    if (*(data) == DATA_TYPE_ACL) {
        struct os_mbuf *om = os_msys_get_pkthdr(len, ACL_DATA_MBUF_LEADINGSPACE);
        assert(om);
        os_mbuf_append(om, &data[1], len - 1);
        ble_hci_trans_hs_acl_tx(om);
    }

}

bool esp_vhci_host_check_send_available() {
    // not need to check in esp new controller
    return true;
}

int
ble_hs_hci_rx_evt(uint8_t *hci_ev, void *arg)
{
    uint16_t len = hci_ev[1] + 3;
    uint8_t *data = (uint8_t *)malloc(len);
    data[0] = 0x04;
    memcpy(&data[1], hci_ev, len - 1);
    ble_hci_trans_buf_free(hci_ev);
    vhci_host_cb.notify_host_recv(data, len);
    free(data);
    return 0;
}


int
ble_hs_rx_data(struct os_mbuf *om, void *arg)
{
    uint16_t len = om->om_len + 1;
    uint8_t *data = (uint8_t *)malloc(len);
    data[0] = 0x02;
    os_mbuf_copydata(om, 0, len - 1, &data[1]);
    vhci_host_cb.notify_host_recv(data, len);
    free(data);
    os_mbuf_free_chain(om);
    return 0;
}
#endif // ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 0)

#endif
#endif
#endif

esp_err_t initialise_bluetooth(void)
{
#if CONFIG_BT_ENABLED
	uint8_t mac[BSSID_BYTES_SIZE] = {0};
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

	ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_BT));
	ESP_LOGI(TAG, "ESP Bluetooth MAC addr: %02x:%02x:%02x:%02x:%02x:%02x",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

#ifdef BLUETOOTH_UART
	slave_bt_init_uart(&bt_cfg);
#endif

	ESP_ERROR_CHECK( esp_bt_controller_init(&bt_cfg) );
#if BLUETOOTH_BLE
	ESP_ERROR_CHECK( esp_bt_controller_enable(ESP_BT_MODE_BLE) );
#elif BLUETOOTH_BT
	ESP_ERROR_CHECK( esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) );
#elif BLUETOOTH_BT_BLE
	ESP_ERROR_CHECK( esp_bt_controller_enable(ESP_BT_MODE_BTDM) );
#endif

#if BLUETOOTH_HCI
	esp_err_t ret = ESP_OK;

#if SOC_ESP_NIMBLE_CONTROLLER && (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 0))
    ble_hci_trans_cfg_hs((ble_hci_trans_rx_cmd_fn *)ble_hs_hci_rx_evt,NULL,
                         (ble_hci_trans_rx_acl_fn *)ble_hs_rx_data,NULL);
#else
	ret = esp_vhci_host_register_callback(&vhci_host_cb);
#endif

	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to register VHCI callback");
		return ret;
	}

	vhci_send_sem = xSemaphoreCreateBinary();
	if (vhci_send_sem == NULL) {
		ESP_LOGE(TAG, "Failed to create VHCI send sem");
		return ESP_ERR_NO_MEM;
	}

	xSemaphoreGive(vhci_send_sem);
#endif
#endif

	return ESP_OK;
}

void deinitialize_bluetooth(void)
{
#ifdef CONFIG_BT_ENABLED
#if BLUETOOTH_HCI
	if (vhci_send_sem) {
		/* Dummy take and give sema before deleting it */
		xSemaphoreTake(vhci_send_sem, portMAX_DELAY);
		xSemaphoreGive(vhci_send_sem);
		vSemaphoreDelete(vhci_send_sem);
		vhci_send_sem = NULL;
	}
	esp_bt_controller_disable();
	esp_bt_controller_deinit();
#endif
#endif
}

uint8_t get_bluetooth_capabilities(void)
{
	uint8_t cap = 0;
#ifdef CONFIG_BT_ENABLED
	ESP_LOGI(TAG, "- BT/BLE");
#if BLUETOOTH_HCI

#if CONFIG_ESP_SPI_HOST_INTERFACE
	ESP_LOGI(TAG, "   - HCI Over SPI");
	cap |= ESP_BT_SPI_SUPPORT;
#elif CONFIG_ESP_SDIO_HOST_INTERFACE
	ESP_LOGI(TAG, "   - HCI Over SDIO");
	cap |= ESP_BT_SDIO_SUPPORT;
#endif

#elif BLUETOOTH_UART
	ESP_LOGI(TAG, "   - HCI Over UART");
	cap |= ESP_BT_UART_SUPPORT;
#endif

#if BLUETOOTH_BLE
	ESP_LOGI(TAG, "   - BLE only");
	cap |= ESP_BLE_ONLY_SUPPORT;
#elif BLUETOOTH_BT
	ESP_LOGI(TAG, "   - BR_EDR only");
	cap |= ESP_BR_EDR_ONLY_SUPPORT;
#elif BLUETOOTH_BT_BLE
	ESP_LOGI(TAG, "   - BT/BLE dual mode");
	cap |= ESP_BLE_ONLY_SUPPORT | ESP_BR_EDR_ONLY_SUPPORT;
#endif
#endif
	return cap;
}

uint32_t get_bluetooth_ext_capabilities(void)
{
	uint32_t ext_cap = 0;
#ifdef CONFIG_BT_ENABLED
	ESP_LOGI(TAG, "- BT/BLE (extended)");
#if BLUETOOTH_HCI
#if CONFIG_ESP_SPI_HD_HOST_INTERFACE
	ESP_LOGI(TAG, "   - HCI Over SPI HD");
	ext_cap |= ESP_BT_INTERFACE_SUPPORT;
#endif
#if CONFIG_ESP_UART_HOST_INTERFACE
	ESP_LOGI(TAG, "   - HCI Over UART (VHCI)");
	ext_cap |= ESP_BT_INTERFACE_SUPPORT;
#endif
#endif
#endif
	return ext_cap;
}
