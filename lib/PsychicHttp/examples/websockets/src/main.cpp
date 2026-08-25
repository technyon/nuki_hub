/*
  PsychicHTTP Server Example

  This example code is in the Public Domain (or CC0 licensed, at your option.)

  Unless required by applicable law or agreed to in writing, this
  software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
  CONDITIONS OF ANY KIND, either express or implied.
*/

/**********************************************************************************************
 * Note: this demo relies on various files to be uploaded on the LittleFS partition
 * PlatformIO -> Build Filesystem Image and then PlatformIO -> Upload Filesystem Image
 **********************************************************************************************/

#include "_secret.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <PsychicHttp.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <freertos/queue.h>

#ifndef WIFI_SSID
  #error "You need to enter your wifi credentials. Rename secret.h to _secret.h and enter your credentials there."
#endif

// Enter your WIFI credentials in secret.h
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

// hostname for mdns (psychic.local)
const char* local_hostname = "psychic";

PsychicHttpServer server;
PsychicWebSocketHandler websocketHandler;

typedef struct
{
    int socket;
    char* buffer;
    size_t len;
} WebsocketMessage;

QueueHandle_t wsMessages;

bool connectToWifi()
{
  Serial.print("[WiFi] Connecting to ");
  Serial.println(ssid);

  // setup our wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Will try for about 10 seconds (20x 500ms)
  int tryDelay = 500;
  int numberOfTries = 20;

  // Wait for the WiFi event
  while (true)
  {
    switch (WiFi.status())
    {
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

    if (numberOfTries <= 0)
    {
      Serial.print("[WiFi] Failed to connect to WiFi!");
      // Use disconnect function to force stop trying to connect
      WiFi.disconnect();
      return false;
    }
    else
    {
      numberOfTries--;
    }
  }

  return false;
}

void setup()
{
  Serial.begin(115200);
  delay(10);

  // prepare our message queue of 10 messages
  wsMessages = xQueueCreate(10, sizeof(WebsocketMessage));
  if (wsMessages == 0)
    Serial.printf("Failed to create queue= %p\n", wsMessages);

  // We start by connecting to a WiFi network
  // To debug, please enable Core Debug Level to Verbose
  if (connectToWifi())
  {
    // set up our esp32 to listen on the local_hostname.local domain
    if (!MDNS.begin(local_hostname))
    {
      Serial.println("Error starting mDNS");
      return;
    }
    MDNS.addService("http", "tcp", 80);

    if (!LittleFS.begin())
    {
      Serial.println("LittleFS Mount Failed. Do Platform -> Build Filesystem Image and Platform -> Upload Filesystem Image from VSCode");
      return;
    }

    // this is where our /index.html file lives
    //  curl -i http://psychic.local/
    server.serveStatic("/", LittleFS, "/www/");

    // a websocket echo server
    //  npm install -g wscat
    //  wscat -c ws://psychic.local/ws
    websocketHandler.onOpen([](PsychicWebSocketClient* client)
                            {
      Serial.printf("[socket] connection #%u connected from %s\n", client->socket(), client->remoteIP().toString().c_str());
      client->sendMessage("Hello!"); });
    websocketHandler.onFrame([](PsychicWebSocketRequest* request, httpd_ws_frame* frame)
                             {
      Serial.printf("[socket] #%d sent: %s\n", request->client()->socket(), (char *)frame->payload);

      //we are allocating memory here, and the worker will free it
      WebsocketMessage wm;
      wm.socket = request->client()->socket();
      wm.len = frame->len;
      wm.buffer = (char *)malloc(frame->len);

      //did we flame out?
      if (wm.buffer == NULL)
      {
        Serial.printf("Queue message: unable to allocate %d bytes\n", frame->len);
        return ESP_FAIL;    
      }

      //okay, copy it over
      memcpy(wm.buffer, frame->payload, frame->len); 

      //try to throw it in our queue
      if (xQueueSend(wsMessages, &wm, 1) != pdTRUE)
      {
        Serial.printf("[socket] queue full #%d\n", wm.socket);

        //free the memory... no worker to do it for us.
        free(wm.buffer);
      }

      //send a throttle message if we're full
      if (!uxQueueSpacesAvailable(wsMessages))
        return request->reply("Queue Full");

      return ESP_OK; });
    websocketHandler.onClose([](PsychicWebSocketClient* client)
                             { Serial.printf("[socket] connection #%u closed from %s\n", client->socket(), client->remoteIP().toString().c_str()); });
    server.on("/ws", &websocketHandler);
  }
}

unsigned long lastUpdate = 0;
char output[60];

void loop()
{
  // process our websockets outside the callback.
  WebsocketMessage message;
  while (xQueueReceive(wsMessages, &message, 0) == pdTRUE)
  {
    // make sure our client is still good.
    PsychicWebSocketClient* client = websocketHandler.getClient(message.socket);
    if (client == NULL)
    {
      Serial.printf("[socket] client #%d bad, bailing\n", message.socket);
      return;
    }

    // echo it back to the client.
    // alternatively, this is where you would deserialize a json message, parse it, and generate a response if needed
    client->sendMessage(HTTPD_WS_TYPE_TEXT, message.buffer, message.len);

    // make sure to release our memory!
    free(message.buffer);
  }

  // send a periodic update to all clients
  if (millis() - lastUpdate > 2000)
  {
    sprintf(output, "Millis: %lu\n", millis());
    websocketHandler.sendAll(output);

    lastUpdate = millis();
  }
}