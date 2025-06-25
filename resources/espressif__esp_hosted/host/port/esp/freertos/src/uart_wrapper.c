// SPDX-License-Identifier: Apache-2.0
// Copyright 2015-2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"

#include "esp_hosted_config.h"
#include "os_wrapper.h"
#include "uart_wrapper.h"

#include "esp_log.h"
static const char TAG[] = "uart_wrapper";

#if H_UART_START_BITS != 1
#error "UART Start Bits must be 1 to communicate with ESP co-processor"
#endif

#if H_UART_FLOWCTRL
#error "UART Flow Control must be disabled to communicate with ESP co-processor"
#endif

#if CONFIG_ESP_CONSOLE_UART
#if CONFIG_ESP_CONSOLE_UART_NUM == H_UART_PORT
#error "ESP Console UART and Hosted UART are the same. Select another UART port."
#endif
#endif

#define UART_FAIL_IF_NULL_CTX(x) do { \
		if (!x) return ESP_FAIL;      \
	} while (0);

// these values should match ESP_HOSTED_UART_PARITY values in Hosted Kconfig
enum {
	HOSTED_UART_PARITY_NONE = 0,
	HOSTED_UART_PARITY_EVEN = 1,
	HOSTED_UART_PARITY_ODD = 2,
};

// these values should match ESP_HOSTED_UART_STOP_BITS values in Hosted Kconfig
enum {
	HOSTED_STOP_BITS_1 = 0,
	HOSTED_STOP_BITS_1_5 = 1,
	HOSTED_STOP_BITS_2 = 2,
};

// UART context structure
typedef struct uart_ctx_t {
	int uart_port;
} uart_ctx_t;

static uart_ctx_t * ctx = NULL;

int hosted_uart_read(void * ctx, uint8_t *data, uint16_t size)
{
	uart_ctx_t * pctx;

	UART_FAIL_IF_NULL_CTX(ctx);

	pctx = (uart_ctx_t *)ctx;

	return uart_read_bytes(pctx->uart_port, data, size, portMAX_DELAY);
}

int hosted_uart_write(void * ctx, uint8_t *data, uint16_t size)
{
	uart_ctx_t * pctx;

	UART_FAIL_IF_NULL_CTX(ctx);

	pctx = (uart_ctx_t *)ctx;

	return uart_write_bytes(pctx->uart_port, (const char*)data, size);
}

void * hosted_uart_init(void)
{
	uart_word_length_t uart_word_length;
	uart_parity_t parity;
	uart_stop_bits_t stop_bits;

	ctx = (uart_ctx_t*)g_h.funcs->_h_malloc(sizeof(uart_ctx_t));
	assert(ctx);

	switch (H_UART_NUM_DATA_BITS) {
	case 5:
		uart_word_length = UART_DATA_5_BITS;
		break;
	case 6:
		uart_word_length = UART_DATA_6_BITS;
		break;
	case 7:
		uart_word_length = UART_DATA_7_BITS;
		break;
	case 8:
		// drop through to default
	default:
		uart_word_length = UART_DATA_8_BITS;
		break;
	}

	switch (H_UART_PARITY) {
	case HOSTED_UART_PARITY_EVEN: // even parity
		parity = UART_PARITY_EVEN;
		break;
	case HOSTED_UART_PARITY_ODD: // odd parity
		parity = UART_PARITY_ODD;
		break;
	case HOSTED_UART_PARITY_NONE: // none
		// drop through to default
	default:
		parity = UART_PARITY_DISABLE;
		break;
	}

	switch (H_UART_STOP_BITS) {
	case HOSTED_STOP_BITS_1_5: // 1.5 stop bits
		stop_bits = UART_STOP_BITS_1_5;
		break;
	case HOSTED_STOP_BITS_2: // 2 stop bits
		stop_bits = UART_STOP_BITS_2;
		break;
	case HOSTED_STOP_BITS_1: // 1 stop bits
		// drop through to default
	default:
		stop_bits = UART_STOP_BITS_1;
		break;
	}

	// initialise bus and device in ctx
	const uart_config_t uart_config = {
		.baud_rate = H_UART_BAUD_RATE,
		.data_bits = uart_word_length,
		.parity = parity,
		.stop_bits = stop_bits,
		.flow_ctrl = H_UART_FLOWCTRL,
		.source_clk = H_UART_CLK_SRC,
	};

	ESP_ERROR_CHECK(uart_driver_install(H_UART_PORT, MAX_UART_BUFFER_SIZE, MAX_UART_BUFFER_SIZE,
			0, NULL, 0));
	ESP_ERROR_CHECK(uart_param_config(H_UART_PORT, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(H_UART_PORT, H_UART_TX_PIN, H_UART_RX_PIN,
			UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

	ESP_LOGI(TAG, "UART GPIOs: Tx: %"PRIu16 ", Rx: %"PRIu16 ", Baud Rate %i",
			H_UART_TX_PIN, H_UART_RX_PIN, H_UART_BAUD_RATE);

	ctx->uart_port = H_UART_PORT;

	return ctx;
}

esp_err_t hosted_uart_deinit(void *ctx)
{
	esp_err_t ret;
	uart_ctx_t * pctx;

	UART_FAIL_IF_NULL_CTX(ctx);

	pctx = (uart_ctx_t *)ctx;

	ret = uart_flush_input(pctx->uart_port);
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "%s: Failed to flush uart Rx", __func__);
	ret = uart_wait_tx_done(pctx->uart_port, 100); // wait 100 RTOS ticks for Tx to be empty
	if (ret != ESP_OK)
		ESP_LOGE(TAG, "%s: Failed to flush uart Tx", __func__);
	uart_driver_delete(pctx->uart_port);

	HOSTED_FREE(ctx);

	return ESP_OK;
}
