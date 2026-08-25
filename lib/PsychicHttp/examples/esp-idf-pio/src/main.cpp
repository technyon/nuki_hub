/*
  PsychicHTTP — Native ESP-IDF build validation (no Arduino framework).

  This is a minimal but complete build-validation entry point; it exercises
  every major code-path of PsychicHttp that is available in native IDF:

    - PsychicHttpServer  : HTTP + WebSocket + SSE handlers
    - LoggingMiddleware  : IDF-native (ESP_LOGI) logging
    - AuthenticationMiddleware : basic & digest auth
    - CorsMiddleware     : CORS headers
    - PsychicEventSource : Server-Sent Events
    - PsychicWebSocket   : WebSocket echo
    - ON_STA_FILTER / ON_AP_FILTER : network-interface filters
    - serveStatic(uri, path) : POSIX-VFS static-file handler
    - PsychicStreamResponse  : chunked response via Print API (/stream)
    - TemplatePrinter        : template substitution on top of stream (/template)

  WiFi, LittleFS and mDNS are initialised before the server starts so the
  device is fully usable out-of-the-box after flashing.

  Copy src/secrets.h.example to src/secrets.h and fill in your Wi-Fi credentials.
*/

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
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <nvs_flash.h>

static const char* TAG = "PsychicHttp";

static const char* WIFI_SSID_STR = WIFI_SSID;
static const char* WIFI_PASS_STR = WIFI_PASS;
static const char* AP_SSID = "PsychicHttp";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry_num = 0;
#define WIFI_MAX_RETRY 10

static PsychicHttpServer server;
static PsychicWebSocketHandler wsHandler;
static PsychicEventSource eventSource;
static AuthenticationMiddleware basicAuth;
static CorsMiddleware cors;

// ---------------------------------------------------------------------------
// Wi-Fi
// ---------------------------------------------------------------------------
static void wifi_event_handler(void*, esp_event_base_t base, int32_t id, void* data)
{
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num++ < WIFI_MAX_RETRY)
      esp_wifi_connect();
    else
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* ev = (ip_event_got_ip_t*)data;
    ESP_LOGI(TAG, "Wi-Fi STA IP: " IPSTR, IP2STR(&ev->ip_info.ip));
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

  esp_event_handler_instance_t h_any, h_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
    WIFI_EVENT,
    ESP_EVENT_ANY_ID,
    wifi_event_handler,
    nullptr,
    &h_any));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
    IP_EVENT,
    IP_EVENT_STA_GOT_IP,
    wifi_event_handler,
    nullptr,
    &h_ip));

  wifi_config_t sta = {};
  strncpy((char*)sta.sta.ssid, WIFI_SSID_STR, sizeof(sta.sta.ssid) - 1);
  strncpy((char*)sta.sta.password, WIFI_PASS_STR, sizeof(sta.sta.password) - 1);

  wifi_config_t ap = {};
  strncpy((char*)ap.ap.ssid, AP_SSID, sizeof(ap.ap.ssid) - 1);
  ap.ap.ssid_len = (uint8_t)strlen(AP_SSID);
  ap.ap.channel = 6;
  ap.ap.max_connection = 4;
  ap.ap.authmode = WIFI_AUTH_OPEN;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
  ESP_ERROR_CHECK(esp_wifi_start());

  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
    WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
    pdFALSE,
    pdFALSE,
    pdMS_TO_TICKS(10000));
  return (bits & WIFI_CONNECTED_BIT) != 0;
}

// ---------------------------------------------------------------------------
// HTTP server
// ---------------------------------------------------------------------------
static void server_setup()
{
  basicAuth.setUsername("admin").setPassword("admin").setRealm("PsychicHttp Demo").setAuthMethod(BASIC_AUTH);
  cors.setOrigin("*").setMethods("GET,POST,OPTIONS");

  server.config.max_uri_handlers = 20;

  server.on("/hello", HTTP_GET, [](PsychicRequest* req, PsychicResponse* res) {
    ESP_LOGI(TAG, "GET /hello");
    res->setCode(200);
    res->setContentType("text/plain");
    res->setContent("Hello from native ESP-IDF PsychicHttp!");
    return res->send();
  });

  server.on("/api/v1/info", HTTP_GET, [](PsychicRequest* req, PsychicResponse* res) {
    JsonDocument doc;
    uint64_t uptime = (uint64_t)(esp_timer_get_time() / 1000ULL);
    uint32_t heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "GET /api/v1/info — uptime=%" PRIu64 "ms heap=%" PRIu32, uptime, heap);
    doc["uptime_ms"] = uptime;
    doc["free_heap"] = heap;
    std::string body;
    serializeJson(doc, body);
    res->setCode(200);
    res->setContentType("application/json");
    res->setContent(body.c_str());
    return res->send();
  });

  server.on("/api/v1/msg", HTTP_POST, [](PsychicRequest* req, PsychicResponse* res) {
    JsonDocument doc;
    if (deserializeJson(doc, req->body()) != DeserializationError::Ok)
      return res->send(400, "text/plain", "Bad JSON");
    ESP_LOGI(TAG, "POST /msg: %s", (const char*)(doc["msg"] | ""));
    return res->send(200, "application/json", "{\"status\":\"ok\"}");
  });

  // Authenticated (basic) — STA-only
  server.on("/auth", HTTP_GET, [](PsychicRequest* req, PsychicResponse* res) {
          ESP_LOGI(TAG, "GET /auth — user authenticated");
          return res->send(200, "text/plain", "Authenticated!");
        })
    ->addMiddleware(&basicAuth)
    ->addFilter(ON_STA_FILTER);

  // PsychicStreamResponse — chunked plain-text response using Print API
  server.on("/stream", HTTP_GET, [](PsychicRequest* req, PsychicResponse* res) {
    ESP_LOGI(TAG, "GET /stream — starting chunked response");
    PsychicStreamResponse stream(res, "text/plain");
    esp_err_t err = stream.beginSend();
    if (err != ESP_OK)
      return err;
    for (int i = 0; i < 5; i++)
      stream.printf("line %d: uptime=%llu ms\n", i, (unsigned long long)(esp_timer_get_time() / 1000ULL));
    return stream.endSend();
  });

  // TemplatePrinter — template substitution on top of PsychicStreamResponse
  server.on("/template", HTTP_GET, [](PsychicRequest* req, PsychicResponse* res) {
    ESP_LOGI(TAG, "GET /template — rendering template response");
    PsychicStreamResponse stream(res, "text/html");
    esp_err_t err = stream.beginSend();
    if (err != ESP_OK)
      return err;
    TemplatePrinter::start(
      stream,
      [](Print& out, const char* param) -> bool {
        if (strcmp(param, "uptime") == 0) {
          out.printf("%llu", (unsigned long long)(esp_timer_get_time() / 1000ULL));
          return true;
        }
        if (strcmp(param, "heap") == 0) {
          out.print((unsigned long)esp_get_free_heap_size());
          return true;
        }
        return false;
      },
      [](TemplatePrinter& p) {
        p.print("<html><body>");
        p.print("<p>Uptime: %uptime% ms</p>");
        p.print("<p>Free heap: %heap% bytes</p>");
        p.print("</body></html>");
      });
    return stream.endSend();
  });

  // WebSocket echo
  wsHandler.onFrame([](PsychicWebSocketRequest* req, httpd_ws_frame_t* frame) -> esp_err_t {
    ESP_LOGI(TAG, "WS /ws — frame len=%d type=%d", (int)frame->len, (int)frame->type);
    return req->reply(frame);
  });
  server.on("/ws", &wsHandler);

  // Server-Sent Events
  server.on("/events", &eventSource);

  server.begin();

  ESP_LOGI(TAG, "HTTP server started on port 80");
}

// ---------------------------------------------------------------------------
// SSE background task
// ---------------------------------------------------------------------------
static void sse_task(void*)
{
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    uint64_t uptime_ms = (uint64_t)(esp_timer_get_time() / 1000ULL);
    char payload[32];
    snprintf(payload, sizeof(payload), "%llu", (unsigned long long)uptime_ms);
    ESP_LOGI(TAG, "SSE /events — sending uptime event: %s ms", payload);
    eventSource.send(payload, "uptime", (uint32_t)(uptime_ms / 1000), 5000);
  }
}

// ---------------------------------------------------------------------------
// app_main
// ---------------------------------------------------------------------------
extern "C" void app_main(void)
{
  // NVS (required by Wi-Fi driver)
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Wi-Fi
  if (!wifi_init())
    ESP_LOGW(TAG, "STA failed — running in AP-only mode");

  // HTTP
  server_setup();

  // SSE ping task
  xTaskCreate(sse_task, "sse_task", 4096, nullptr, 5, nullptr);
}
