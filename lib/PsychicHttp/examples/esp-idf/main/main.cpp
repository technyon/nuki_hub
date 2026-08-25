/*
  PsychicHTTP Server Example — Native ESP-IDF (no Arduino framework)

  This example code is in the Public Domain (or CC0 licensed, at your option.)

  Unless required by applicable law or agreed to in writing, this
  software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
  CONDITIONS OF ANY KIND, either express or implied.
*/

/*******************************************************************************
 * Note: this demo requires various files to be uploaded to the LittleFS
 * partition. See the README for instructions.
 *
 * Note: copy secrets.h.example to secrets.h and fill in your Wi-Fi credentials.
 ******************************************************************************/

#if __has_include("secrets.h")
  #include "secrets.h"
#elif __has_include("../../../secrets.h")
  #include "../../../secrets.h"
#else
  #error "Missing secrets.h (place it next to this example or in repository root)"
#endif
#include <ArduinoJson.h>
#include <PsychicHttp.h>
#include <esp_event.h>
#include <esp_idf_version.h>
#include <esp_littlefs.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <mdns.h>
#include <nvs_flash.h>

static const char* TAG = "PsychicHttp";

// Wi-Fi credentials — from secrets.h
static const char* WIFI_SSID_STR = WIFI_SSID;
static const char* WIFI_PASS_STR = WIFI_PASS;

// Soft-AP settings (open network, always on alongside STA)
static const char* AP_SSID = "PsychicHttp";
static const char* AP_PASS = ""; // empty → open network

// mDNS hostname — device reachable as psychic.local
static const char* LOCAL_HOSTNAME = "psychic";

// Credentials for the /auth-basic and /auth-digest examples
static const char* APP_USER = "admin";
static const char* APP_PASS = "admin";
static const char* APP_NAME = "PsychicHttp Demo";

// LittleFS VFS mount base path (must match sdkconfig / partition table)
static const char* LFS_BASE = "/littlefs";

// -------------------------------------------------------------------------
// Globals
// -------------------------------------------------------------------------
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry_num = 0;
#define WIFI_MAX_RETRY 10

PsychicHttpServer server;
PsychicWebSocketHandler websocketHandler;
PsychicEventSource eventSource;
AuthenticationMiddleware basicAuth;
AuthenticationMiddleware digestAuth;

// -------------------------------------------------------------------------
// Wi-Fi helpers
// -------------------------------------------------------------------------
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
  int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < WIFI_MAX_RETRY) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "Wi-Fi: retrying… (%d/%d)", s_retry_num, WIFI_MAX_RETRY);
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
      ESP_LOGE(TAG, "Wi-Fi: connection failed after %d attempts", WIFI_MAX_RETRY);
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
    ESP_LOGI(TAG, "Wi-Fi: got IP " IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

static bool wifi_init()
{
  s_wifi_event_group = xEventGroupCreate();

  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t h_any, h_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
    WIFI_EVENT,
    ESP_EVENT_ANY_ID,
    &wifi_event_handler,
    NULL,
    &h_any));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
    IP_EVENT,
    IP_EVENT_STA_GOT_IP,
    &wifi_event_handler,
    NULL,
    &h_got_ip));

  // STA configuration
  wifi_config_t sta_cfg = {};
  strncpy((char*)sta_cfg.sta.ssid, WIFI_SSID_STR, sizeof(sta_cfg.sta.ssid) - 1);
  strncpy((char*)sta_cfg.sta.password, WIFI_PASS_STR, sizeof(sta_cfg.sta.password) - 1);

  // AP configuration (open network)
  wifi_config_t ap_cfg = {};
  strncpy((char*)ap_cfg.ap.ssid, AP_SSID, sizeof(ap_cfg.ap.ssid) - 1);
  ap_cfg.ap.ssid_len = (uint8_t)strlen(AP_SSID);
  ap_cfg.ap.channel = 6;
  ap_cfg.ap.max_connection = 4;
  ap_cfg.ap.authmode = (strlen(AP_PASS) > 0) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
  ESP_ERROR_CHECK(esp_wifi_start());

  // Wait up to ~10 s for STA connection
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
    WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
    pdFALSE,
    pdFALSE,
    pdMS_TO_TICKS(10000));

  return (bits & WIFI_CONNECTED_BIT) != 0;
}

// -------------------------------------------------------------------------
// mDNS
// -------------------------------------------------------------------------
static void mdns_start()
{
  ESP_ERROR_CHECK(mdns_init());
  ESP_ERROR_CHECK(mdns_hostname_set(LOCAL_HOSTNAME));
  mdns_instance_name_set("PsychicHttp Web Server");
  mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0);
  ESP_LOGI(TAG, "mDNS: http://%s.local", LOCAL_HOSTNAME);
}

// -------------------------------------------------------------------------
// LittleFS VFS mount
// -------------------------------------------------------------------------
static void lfs_mount()
{
  esp_vfs_littlefs_conf_t conf = {
    .base_path = LFS_BASE,
    .partition_label = "littlefs",
    .partition = nullptr,
#if ESP_LITTLEFS_HAS_BLOCKDEV
    .blockdev = nullptr,
#endif
    .format_if_mount_failed = true,
    .read_only = false,
    .dont_mount = false,
    .grow_on_mount = false,
  };
  esp_err_t ret = esp_vfs_littlefs_register(&conf);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "LittleFS mount failed (%s) — static files unavailable", esp_err_to_name(ret));
  } else {
    size_t total = 0, used = 0;
    esp_littlefs_info("littlefs", &total, &used);
    ESP_LOGI(TAG, "LittleFS: %d / %d bytes used", (int)used, (int)total);
  }
}

// -------------------------------------------------------------------------
// HTTP server setup
// -------------------------------------------------------------------------
static void server_setup()
{
  // ---- Auth middleware ----
  basicAuth.setUsername(APP_USER).setPassword(APP_PASS).setRealm(APP_NAME).setAuthMethod(BASIC_AUTH).setAuthFailureMessage("Unauthorized");
  digestAuth.setUsername(APP_USER).setPassword(APP_PASS).setRealm(APP_NAME).setAuthMethod(DIGEST_AUTH).setAuthFailureMessage("Unauthorized");

  server.config.max_uri_handlers = 20;

  // ---- Static files from LittleFS (/littlefs/www/) ----
  char www_path[64];
  snprintf(www_path, sizeof(www_path), "%s/www", LFS_BASE);
  server.serveStatic("/", www_path)->setDefaultFile("index.html");

  // ---- Simple GET ----
  server.on("/hello", HTTP_GET, [](PsychicRequest* req, PsychicResponse* res) {
    res->setCode(200);
    res->setContentType("text/plain");
    res->setContent("Hello from native ESP-IDF PsychicHttp!");
    return res->send();
  });

  // ---- JSON info ----
  server.on("/api/v1/info", HTTP_GET, [](PsychicRequest* req, PsychicResponse* res) {
    JsonDocument doc;
    doc["firmware"] = "PsychicHttp";
    doc["uptime_ms"] = (uint64_t)(esp_timer_get_time() / 1000ULL);
    doc["free_heap"] = esp_get_free_heap_size();
    std::string body;
    serializeJson(doc, body);
    res->setCode(200);
    res->setContentType("application/json");
    res->setContent(body.c_str());
    return res->send();
  });

  // ---- POST JSON ----
  server.on("/api/v1/msg", HTTP_POST, [](PsychicRequest* req, PsychicResponse* res) {
    JsonDocument doc;
    if (deserializeJson(doc, req->body()) != DeserializationError::Ok) {
      res->setCode(400);
      res->setContent("Bad JSON");
      return res->send();
    }
    const char* msg = doc["msg"] | "";
    ESP_LOGI(TAG, "POST /api/v1/msg: %s", msg);
    res->setCode(200);
    res->setContentType("application/json");
    res->setContent("{\"status\":\"ok\"}");
    return res->send();
  });

  // ---- Authenticated endpoint ----
  server.on("/auth-basic", HTTP_GET, [](PsychicRequest* req, PsychicResponse* res) {
          res->setCode(200);
          res->setContent("You are authenticated (basic)!");
          return res->send();
        })
    ->addMiddleware(&basicAuth);

  // ---- WebSocket ----
  websocketHandler.onFrame([](PsychicWebSocketRequest* req, httpd_ws_frame_t* frame) -> esp_err_t {
    ESP_LOGI(TAG, "WS msg (%d bytes): %.*s", (int)frame->len, (int)frame->len, frame->payload);
    // Echo back
    return req->reply(frame);
  });
  server.on("/ws", &websocketHandler);

  // ---- Server-Sent Events ----
  server.on("/events", &eventSource);

  server.begin();

  ESP_LOGI(TAG, "HTTP server listening on port 80");
}

// -------------------------------------------------------------------------
// Background task: push an SSE event every 5 s
// -------------------------------------------------------------------------
static void sse_task(void*)
{
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    uint64_t uptime_ms = esp_timer_get_time() / 1000ULL;
    char payload[32];
    snprintf(payload, sizeof(payload), "%llu", (unsigned long long)uptime_ms);
    eventSource.send(payload, "uptime", (uint32_t)(uptime_ms / 1000), 5000);
  }
}

// -------------------------------------------------------------------------
// Entry point
// -------------------------------------------------------------------------
extern "C" void app_main(void)
{
  // 1. NVS flash (required by Wi-Fi)
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // 2. TCP/IP stack + default event loop
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // 3. Mount LittleFS
  lfs_mount();

  // 4. Connect Wi-Fi (STA + AP)
  if (!wifi_init()) {
    ESP_LOGW(TAG, "Running in AP-only mode (STA connection failed)");
  }

  // 5. mDNS
  mdns_start();

  // 6. HTTP server + handlers
  server_setup();

  // 7. SSE background task
  xTaskCreate(sse_task, "sse_task", 4096, nullptr, 5, nullptr);
}
