/*
  --------------
  WebSerial Demo
  --------------
  
  Skill Level: Beginner

  This example provides with a bare minimal app with WebSerial functionality using softAP mode.

  Github: https://github.com/ayushsharma82/WebSerial
  Wiki: https://docs.webserial.pro

  Works with following hardware:
  - ESP8266
  - ESP32

  WebSerial terminal will be accessible at your microcontroller's <IPAddress>/webserial URL.

  Checkout WebSerial Pro: https://webserial.pro
*/

#include <Arduino.h>
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <AsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>

AsyncWebServer server(80);

const char* ssid = "WSLDemo"; // WiFi AP SSID
const char* password = ""; // WiFi AP Password

unsigned long last_print_time = millis();

void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  // Once connected, print IP
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! This is WebSerial demo. You can access webserial interface at http://" + WiFi.softAPIP().toString() + "/webserial");
  });

  // WebSerial is accessible at "<IP Address>/webserial" in browser
  WebSerial.begin(&server);

  /* Attach Message Callback */
  WebSerial.onMessage([&](uint8_t *data, size_t len) {
    Serial.printf("Received %u bytes from WebSerial: ", len);
    Serial.write(data, len);
    Serial.println();
    WebSerial.println("Received Data...");
    String d = "";
    for(size_t i=0; i < len; i++){
      d += char(data[i]);
    }
    WebSerial.println(d);
  });

  // Start server
  server.begin();
}

void loop() {
  // Print every 2 seconds (non-blocking)
  if ((unsigned long)(millis() - last_print_time) > 2000) {
    WebSerial.print(F("IP address: "));
    WebSerial.println(WiFi.softAPIP());
    WebSerial.printf("Uptime: %lums\n", millis());
    WebSerial.printf("Free heap: %" PRIu32 "\n", ESP.getFreeHeap());
    last_print_time = millis();
  }
}
