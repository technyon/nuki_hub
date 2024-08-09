/*
 * This example shows how to use WebSerial variant to send logging data to the browser.
 *
 * Before using this example, make sure to look at the WebSerial example before and its description.\
 *
 * You might want to control these flags to control the async library performance:
 *  -D CONFIG_ASYNC_TCP_QUEUE_SIZE=128
 *  -D CONFIG_ASYNC_TCP_RUNNING_CORE=1
 *  -D WS_MAX_QUEUED_MESSAGES=128
 */
#include <Arduino.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#elif defined(ESP32)
#include <AsyncTCP.h>
#include <WiFi.h>
#endif
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <WString.h>
#include <MycilaWebSerial.h>

AsyncWebServer server(80);

static uint32_t last = millis();
static uint32_t count = 0;

void setup() {
  Serial.begin(115200);

  WiFi.softAP("WSLDemo");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP().toString());

  WebSerial.onMessage([](const String& msg) { Serial.println(msg); });
  WebSerial.begin(&server);
  WebSerial.setBuffer(100);

  server.onNotFound([](AsyncWebServerRequest* request) { request->redirect("/webserial"); });
  server.begin();
}

void loop() {
  if (millis() - last > 1000) {
    count++;

    WebSerial.print(F("IP address: "));
    WebSerial.println(WiFi.softAPIP());
    WebSerial.printf("Uptime: %lums\n", millis());
    WebSerial.printf("Free heap: %" PRIu32 "\n", ESP.getFreeHeap());

    last = millis();
  }
}
