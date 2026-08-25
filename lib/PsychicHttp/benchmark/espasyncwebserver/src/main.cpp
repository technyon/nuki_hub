/* Wi-Fi STA Connect and Disconnect Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.

*/
#include "_secret.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>

#ifndef WIFI_SSID
  #error "You need to enter your wifi credentials.  Copy secret.h to _secret.h and enter your credentials there."
#endif

// Enter your WIFI credentials in secret.h
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

// hostname for mdns (psychic.local)
const char* local_hostname = "psychic";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char* htmlContent = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Sample HTML</title>
</head>
<body>
    <h1>Hello, World!</h1>
    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin euismod, purus a euismod
    rhoncus, urna ipsum cursus massa, eu dictum tellus justo ac justo. Quisque ullamcorper
    arcu nec tortor ullamcorper, vel fermentum justo fermentum. Vivamus sed velit ut elit
    accumsan congue ut ut enim. Ut eu justo eu lacus varius gravida ut a tellus. Nulla facilisi.
    Integer auctor consectetur ultricies. Fusce feugiat, mi sit amet bibendum viverra, orci leo
    dapibus elit, id varius sem dui id lacus.</p>
    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin euismod, purus a euismod
    rhoncus, urna ipsum cursus massa, eu dictum tellus justo ac justo. Quisque ullamcorper
    arcu nec tortor ullamcorper, vel fermentum justo fermentum. Vivamus sed velit ut elit
    accumsan congue ut ut enim. Ut eu justo eu lacus varius gravida ut a tellus. Nulla facilisi.
    Integer auctor consectetur ultricies. Fusce feugiat, mi sit amet bibendum viverra, orci leo
    dapibus elit, id varius sem dui id lacus.</p>
    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin euismod, purus a euismod
    rhoncus, urna ipsum cursus massa, eu dictum tellus justo ac justo. Quisque ullamcorper
    arcu nec tortor ullamcorper, vel fermentum justo fermentum. Vivamus sed velit ut elit
    accumsan congue ut ut enim. Ut eu justo eu lacus varius gravida ut a tellus. Nulla facilisi.
    Integer auctor consectetur ultricies. Fusce feugiat, mi sit amet bibendum viverra, orci leo
    dapibus elit, id varius sem dui id lacus.</p>
    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin euismod, purus a euismod
    rhoncus, urna ipsum cursus massa, eu dictum tellus justo ac justo. Quisque ullamcorper
    arcu nec tortor ullamcorper, vel fermentum justo fermentum. Vivamus sed velit ut elit
    accumsan congue ut ut enim. Ut eu justo eu lacus varius gravida ut a tellus. Nulla facilisi.
    Integer auctor consectetur ultricies. Fusce feugiat, mi sit amet bibendum viverra, orci leo
    dapibus elit, id varius sem dui id lacus.</p>
    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin euismod, purus a euismod
    rhoncus, urna ipsum cursus massa, eu dictum tellus justo ac justo. Quisque ullamcorper
    arcu nec tortor ullamcorper, vel fermentum justo fermentum. Vivamus sed velit ut elit
    accumsan congue ut ut enim. Ut eu justo eu lacus varius gravida ut a tellus. Nulla facilisi.
    Integer auctor consectetur ultricies. Fusce feugiat, mi sit amet bibendum viverra, orci leo
    dapibus elit, id varius sem dui id lacus.</p>
    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin euismod, purus a euismod
    rhoncus, urna ipsum cursus massa, eu dictum tellus justo ac justo. Quisque ullamcorper
    arcu nec tortor ullamcorper, vel fermentum justo fermentum. Vivamus sed velit ut elit
    accumsan congue ut ut enim. Ut eu justo eu lacus varius gravida ut a tellus. Nulla facilisi.
    Integer auctor consectetur ultricies. Fusce feugiat, mi sit amet bibendum viverra, orci leo
    dapibus elit, id varius sem dui id lacus.</p>
    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin euismod, purus a euismod
    rhoncus, urna ipsum cursus massa, eu dictum tellus justo ac justo. Quisque ullamcorper
    arcu nec tortor ullamcorper, vel fermentum justo fermentum. Vivamus sed velit ut elit
    accumsan congue ut ut enim. Ut eu justo eu lacus varius gravida ut a tellus. Nulla facilisi.
    Integer auctor consectetur ultricies. Fusce feugiat, mi sit amet bibendum viverra, orci leo
    dapibus elit, id varius sem dui id lacus.</p>
    <p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin euismod, purus a euismod
    rhoncus, urna ipsum cursus massa, eu dictum tellus justo ac justo. Quisque ullamcorper
    arcu nec tortor ullamcorper, vel fermentum justo fermentum. Vivamus sed velit ut elit
    accumsan congue ut ut enim. Ut eu justo eu lacus varius gravida ut a tellus. Nulla facilisi.
    Integer auctor consectetur ultricies. Fusce feugiat, mi sit amet bibendum viverra, orci leo
    dapibus elit, id varius sem dui id lacus.</p>
</body>
</html>
)";

const size_t htmlContentLen = strlen(htmlContent);

bool connectToWifi()
{
  Serial.println();
  Serial.print("[WiFi] Connecting to ");
  Serial.println(ssid);

  // WiFi.setSleep(false);
  // WiFi.useStaticBuffers(true);

  WiFi.begin(ssid, password);

  // Will try for about 10 seconds (20x 500ms)
  int tryDelay = 500;
  int numberOfTries = 20;

  // Wait for the WiFi event
  while (true) {
    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL:
        Serial.println("[WiFi] SSID not found");
        break;
      case WL_CONNECT_FAILED:
        Serial.print("[WiFi] Failed - WiFi not connected! Reason: ");
        return false;
        break;
      case WL_CONNECTION_LOST:
        Serial.println("[WiFi] Connection was lost");
        break;
      case WL_SCAN_COMPLETED:
        Serial.println("[WiFi] Scan is completed");
        break;
      case WL_DISCONNECTED:
        Serial.println("[WiFi] WiFi is disconnected");
        break;
      case WL_CONNECTED:
        Serial.println("[WiFi] WiFi is connected!");
        Serial.print("[WiFi] IP address: ");
        Serial.println(WiFi.localIP());
        return true;
        break;
      default:
        Serial.print("[WiFi] WiFi Status: ");
        Serial.println(WiFi.status());
        break;
    }
    delay(tryDelay);

    if (numberOfTries <= 0) {
      Serial.print("[WiFi] Failed to connect to WiFi!");
      // Use disconnect function to force stop trying to connect
      WiFi.disconnect();
      return false;
    } else {
      numberOfTries--;
    }
  }

  return false;
}

void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len)
{
  if (type == WS_EVT_CONNECT) {
    // client connected
    //  Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    //  client->printf("Hello Client %u :)", client->id());
    //  client->ping();
  } else if (type == WS_EVT_DISCONNECT) {
    // client disconnected
    //  Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  } else if (type == WS_EVT_ERROR) {
    // error was received from the other end
    //  Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if (type == WS_EVT_PONG) {
    // pong message was received (in response to a ping request maybe)
    //  Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if (type == WS_EVT_DATA) {
    // data packet
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len) {
      // the whole message is in a single frame and we got all of it's data
      //  Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
      if (info->opcode == WS_TEXT) {
        data[len] = 0;
        // Serial.printf("%s\n", (char*)data);
      } else {
        // for(size_t i=0; i < info->len; i++){
        //   Serial.printf("%02x ", data[i]);
        // }
        // Serial.printf("\n");
      }
      if (info->opcode == WS_TEXT) {
        client->text((char*)data, len);
      }
      // else
      //   client->binary("I got your binary message");
    } else {
      // message is comprised of multiple frames or the frame is split into multiple packets
      if (info->index == 0) {
        // if(info->num == 0)
        //   Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        // Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);
      if (info->message_opcode == WS_TEXT) {
        data[len] = 0;
        // Serial.printf("%s\n", (char*)data);
      } else {
        // for(size_t i=0; i < len; i++){
        //   Serial.printf("%02x ", data[i]);
        // }
        // Serial.printf("\n");
      }

      if ((info->index + len) == info->len) {
        // Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if (info->final) {
          // Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          if (info->message_opcode == WS_TEXT) {
            client->text((char*)data, info->len);
          }
          // else
          //   client->binary("I got your binary message");
        }
      }
    }
  }
}

void setup()
{
  Serial.begin(115200);
  delay(10);
  Serial.println("ESPAsyncWebserver Benchmark");

  // We start by connecting to a WiFi network
  // To debug, please enable Core Debug Level to Verbose
  if (connectToWifi()) {
    // set up our esp32 to listen on the local_hostname.local domain
    if (!MDNS.begin(local_hostname)) {
      Serial.println("Error starting mDNS");
      return;
    }
    MDNS.addService("http", "tcp", 80);

    if (!LittleFS.begin()) {
      Serial.println("LittleFS Mount Failed. Do Platform -> Build Filesystem Image and Platform -> Upload Filesystem Image from VSCode");
      return;
    }

    // api - parameters passed in via query eg. /api/endpoint?foo=bar
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      // ESPAsyncWebServer, sending a char* does a buffer copy, unlike Psychic.
      // Sending flash data is done with the uint8_t* overload.
      request->send(200, "text/html", (uint8_t*)htmlContent, htmlContentLen);
    });

    // api - parameters passed in via query eg. /api/endpoint?foo=bar
    server.on("/api", HTTP_GET, [](AsyncWebServerRequest* request) {
      // create a response object
      JsonDocument output;
      output["msg"] = "status";
      output["status"] = "success";
      output["millis"] = millis();

      // work with some params
      if (request->hasParam("foo")) {
        const AsyncWebParameter* foo = request->getParam("foo");
        output["foo"] = foo->value();
      }

      // serialize and return
      String jsonBuffer;
      serializeJson(output, jsonBuffer);
      request->send(200, "application/json", jsonBuffer.c_str());
    });

    ws.onEvent(onEvent);
    server.addHandler(&ws);

    // put this last, otherwise it clogs the other requests
    // serve static files from LittleFS/www on /
    server.serveStatic("/", LittleFS, "/www/");

    server.begin();
  }
}

void loop()
{
  ws.cleanupClients();
  Serial.printf("Free Heap: %d\n", esp_get_free_heap_size());
  delay(1000);
}