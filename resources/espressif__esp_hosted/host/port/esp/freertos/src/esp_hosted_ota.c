/*
* SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

/* Current OTA method(s) supported:
 * - OTA from a HTTP URL
 *
 * Procedure:
 * 1. Prepare OTA binary
 * 2. Call rpc_ota_begin() to start OTA
 * 3. Repeatedly call rpc_ota_write() with a continuous chunk of OTA data
 * 4. Call rpc_ota_end()
 */

#include "esp_http_client.h"
#include "esp_log.h"

#include "rpc_wrap.h"
#include "esp_hosted_ota.h"

#define CHUNK_SIZE                                        1400
#define OTA_FROM_WEB_URL                                  1

static char* TAG = "hosted_ota";

#if OTA_FROM_WEB_URL
/* Default: Chunk by chunk transfer using esp http client library */
uint8_t http_err = 0;
static esp_err_t http_client_event_handler(esp_http_client_event_t *evt)
{
	switch(evt->event_id) {

	case HTTP_EVENT_ERROR:
		ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
		http_err = 1;
		break;
	case HTTP_EVENT_ON_CONNECTED:
		ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
		break;
	case HTTP_EVENT_HEADER_SENT:
		ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
		break;
	case HTTP_EVENT_ON_HEADER:
		ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
		break;
	case HTTP_EVENT_ON_DATA:
		/* Nothing to handle here */
		break;
	case HTTP_EVENT_ON_FINISH:
		ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
		break;
	case HTTP_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
		break;
	case HTTP_EVENT_REDIRECT:
		ESP_LOGW(TAG, "HTTP_EVENT_REDIRECT");
		break;
	// Other trivial events like HTTP_EVENT_ON_HEADERS_COMPLETE can be handled when needed
	default:
		ESP_LOGD(TAG, "Unhandled event id: %d", evt->event_id);
		break;
	}

	return ESP_OK;
}

static esp_err_t _hosted_ota(const char* image_url)
{
	uint8_t *ota_chunk = NULL;
	esp_err_t err = 0;
	int data_read = 0;
	int ota_failed = 0;

	if (image_url == NULL) {
		ESP_LOGE(TAG, "Invalid image URL");
		return ESP_FAIL;
	}

	/* Initialize HTTP client configuration */
	esp_http_client_config_t config = {
		.url = image_url,
		.timeout_ms = 5000,
		.event_handler = http_client_event_handler,
	};

	esp_http_client_handle_t client = esp_http_client_init(&config);

	ESP_LOGI(TAG, "http_open");
	if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
		ESP_LOGE(TAG, "Check if URL is correct and connectable: %s", image_url);
		esp_http_client_cleanup(client);
		return ESP_FAIL;
	}

	if (http_err) {
		ESP_LOGE(TAG, "Exiting OTA, due to http failure");
		esp_http_client_close(client);
		esp_http_client_cleanup(client);
		http_err = 0;
		return ESP_FAIL;
	}

	ESP_LOGI(TAG, "http_fetch_headers");
	int64_t content_length = esp_http_client_fetch_headers(client);
	if (content_length <= 0) {
		ESP_LOGE(TAG, "HTTP client fetch headers failed");
		ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
				esp_http_client_get_status_code(client),
				esp_http_client_get_content_length(client));
		esp_http_client_close(client);
		esp_http_client_cleanup(client);
		return ESP_FAIL;
	}

	ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %"PRId64,
			esp_http_client_get_status_code(client),
			esp_http_client_get_content_length(client));

	ESP_LOGW(TAG, "********* Started Slave OTA *******************");
	ESP_LOGI(TAG, "*** Please wait for 5 mins to let slave OTA complete ***");

	ESP_LOGI(TAG, "Preparing OTA");
	if ((err = rpc_ota_begin())) {
		ESP_LOGW(TAG, "********* Slave OTA Begin Failed *******************");
		ESP_LOGI(TAG, "esp_ota_begin failed, error=%s", esp_err_to_name(err));
		esp_http_client_close(client);
		esp_http_client_cleanup(client);
		return ESP_FAIL;
	}

	ota_chunk = (uint8_t*)g_h.funcs->_h_calloc(1, CHUNK_SIZE);
	if (!ota_chunk) {
		ESP_LOGE(TAG, "Failed to allocate otachunk mem\n");
		err = -ENOMEM;
	}

	ESP_LOGI(TAG, "Starting OTA");

	if (!err) {
		while ((data_read = esp_http_client_read(client, (char*)ota_chunk, CHUNK_SIZE)) > 0) {

			ESP_LOGV(TAG, "Read image length %d", data_read);
			if ((err = rpc_ota_write(ota_chunk, data_read))) {
				ESP_LOGI(TAG, "rpc_ota_write failed");
				ota_failed = err;
				break;
			}
		}
	}

	g_h.funcs->_h_free(ota_chunk);
	if (err) {
		ESP_LOGW(TAG, "********* Slave OTA Failed *******************");
		ESP_LOGI(TAG, "esp_ota_write failed, error=%s", esp_err_to_name(err));
		ota_failed = -1;
	}

	if (data_read < 0) {
		ESP_LOGE(TAG, "Error: SSL data read error");
		ota_failed = -2;
	}

	if ((err = rpc_ota_end())) {
		ESP_LOGW(TAG, "********* Slave OTA Failed *******************");
		ESP_LOGI(TAG, "esp_ota_end failed, error=%s", esp_err_to_name(err));
		esp_http_client_close(client);
		esp_http_client_cleanup(client);
		ota_failed = err;
		return ESP_FAIL;
	}

	esp_http_client_cleanup(client);
	if (!ota_failed) {
		ESP_LOGW(TAG, "********* Slave OTA Complete *******************");
		ESP_LOGI(TAG, "OTA Successful, Slave will restart in while");
		ESP_LOGE(TAG, "Need to restart host after slave OTA is complete, to avoid sync issues");
		sleep(5);
		ESP_LOGE(TAG, "********* Restarting Host **********************");
		esp_restart();
	}
	return ota_failed;
}

static esp_err_t esp_hosted_slave_ota_chunked(const char* image_url)
{
	uint8_t ota_retry = 2;
	int ret = 0;

	do {
		ret = _hosted_ota(image_url);

		ota_retry--;
		if (ota_retry && ret)
			ESP_LOGI(TAG, "OTA retry left: %u\n", ota_retry);
	} while (ota_retry && ret);

	return ret;
}
#else
/* This assumes full slave binary is present locally */
static esp_err_t esp_hosted_slave_ota_whole_image(const char* image_path)
{
	FILE* f = NULL;
	char ota_chunk[CHUNK_SIZE] = {0};
	int ret = rpc_ota_begin();
	if (ret == ESP_OK) {
		f = fopen(image_path,"rb");
		if (f == NULL) {
			ESP_LOGE(TAG, "Failed to open file %s", image_path);
			return ESP_FAIL;
		} else {
			ESP_LOGV(TAG, "Success in opening %s file", image_path);
		}
		while (!feof(f)) {
			fread(&ota_chunk, CHUNK_SIZE, 1, f);
			ret = rpc_ota_write((uint8_t* )&ota_chunk, CHUNK_SIZE);
			if (ret) {
				ESP_LOGE(TAG, "OTA procedure failed!!");
				/* TODO: Do we need to do OTA end irrespective of success/failure? */
				rpc_ota_end();
				return ESP_FAIL;
			}
		}
		ret = rpc_ota_end();
		if (ret) {
			return ESP_FAIL;
		}
	} else {
		return ESP_FAIL;
	}
	ESP_LOGE(TAG, "ESP32 will restart after 5 sec");
	return ESP_OK;
	ESP_LOGE(TAG, "For OTA, user need to integrate HTTP client lib and then invoke OTA");
	return ESP_FAIL;
}
#endif // ENABLE_HTTP_OTA

esp_err_t esp_hosted_slave_ota(const char* image_url)
{
#if OTA_FROM_WEB_URL
	return esp_hosted_slave_ota_chunked(image_url);
#else
	return esp_hosted_slave_ota_whole_image(image_url);
#endif
}
