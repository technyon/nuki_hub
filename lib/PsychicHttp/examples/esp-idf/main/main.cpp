/*
  PsychicHTTP Server Example

  This example code is in the Public Domain (or CC0 licensed, at your option.)

  Unless required by applicable law or agreed to in writing, this
  software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
  CONDITIONS OF ANY KIND, either express or implied.
*/

/**********************************************************************************************
 * Note: this demo relies on the following libraries (Install via Library Manager)
 * ArduinoJson UrlEncode
 **********************************************************************************************/

/**********************************************************************************************
 * Note: this demo relies on various files to be uploaded on the LittleFS partition
 * Follow instructions here: https://randomnerdtutorials.com/esp32-littlefs-arduino-ide/
 **********************************************************************************************/

#include "secret.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <PsychicHttp.h>
#include <WiFi.h>
#ifdef CONFIG_ESP_HTTPS_SERVER_ENABLE // set this to y in menuconfig to enable SSL
  #include <PsychicHttpsServer.h>
#endif

#ifndef WIFI_SSID
  #error "You need to enter your wifi credentials. Rename secret.h to _secret.h and enter your credentials there."
#endif

// Enter your WIFI credentials in secret.h
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

// Set your SoftAP credentials
const char* softap_ssid = "PsychicHttp";
const char* softap_password = "";
IPAddress softap_ip(10, 0, 0, 1);

// credentials for the /auth-basic and /auth-digest examples
const char* app_user = "admin";
const char* app_pass = "admin";
const char* app_name = "Your App";

AuthenticationMiddleware basicAuth;
AuthenticationMiddleware digestAuth;

// hostname for mdns (psychic.local)
const char* local_hostname = "psychic";

// #define CONFIG_ESP_HTTPS_SERVER_ENABLE to enable ssl
#ifdef CONFIG_ESP_HTTPS_SERVER_ENABLE
bool app_enable_ssl = true;
String server_cert;
String server_key;
#endif

// our main server object
#ifdef CONFIG_ESP_HTTPS_SERVER_ENABLE
PsychicHttpsServer server;
#else
PsychicHttpServer server;
#endif
PsychicWebSocketHandler websocketHandler;
PsychicEventSource eventSource;

bool connectToWifi()
{
  // dual client and AP mode
  WiFi.mode(WIFI_AP_STA);

  // Configure SoftAP
  WiFi.softAPConfig(softap_ip, softap_ip, IPAddress(255, 255, 255, 0)); // subnet FF FF FF 00
  WiFi.softAP(softap_ssid, softap_password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("SoftAP IP Address: ");
  Serial.println(myIP);

  Serial.println();
  Serial.print("[WiFi] Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  // Auto reconnect is set true as default
  // To set auto connect off, use the following function
  // WiFi.setAutoReconnect(false);

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

void setup()
{
  Serial.begin(115200);
  delay(10);

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

// look up our keys?
#ifdef CONFIG_ESP_HTTPS_SERVER_ENABLE
    if (app_enable_ssl) {
      File fp = LittleFS.open("/server.crt");
      if (fp) {
        server_cert = fp.readString();

        // Serial.println("Server Cert:");
        // Serial.println(server_cert);
      } else {
        Serial.println("server.pem not found, SSL not available");
        app_enable_ssl = false;
      }
      fp.close();

      File fp2 = LittleFS.open("/server.key");
      if (fp2) {
        server_key = fp2.readString();

        // Serial.println("Server Key:");
        // Serial.println(server_key);
      } else {
        Serial.println("server.key not found, SSL not available");
        app_enable_ssl = false;
      }
      fp2.close();
    }
#endif

    // setup server config stuff here
    server.config.max_uri_handlers = 20; // maximum number of uri handlers (.on() calls)

#ifdef CONFIG_ESP_HTTPS_SERVER_ENABLE
    server.ssl_config.httpd.max_uri_handlers = 20; // maximum number of uri handlers (.on() calls)

    // do we want secure or not?
    if (app_enable_ssl) {
      server.setCertificate(server_cert.c_str(), server_key.c_str());

      // this creates a 2nd server listening on port 80 and redirects all requests HTTPS
      PsychicHttpServer* redirectServer = new PsychicHttpServer();
      redirectServer->config.ctrl_port = 20424; // just a random port different from the default one
      redirectServer->onNotFound([](PsychicRequest* request, PsychicResponse* response) {
          String url = "https://" + request->host() + request->url();
          return response->redirect(url.c_str()); });
    }
#endif

    basicAuth.setUsername(app_user);
    basicAuth.setPassword(app_pass);
    basicAuth.setRealm(app_name);
    basicAuth.setAuthMethod(HTTPAuthMethod::BASIC_AUTH);
    basicAuth.setAuthFailureMessage("You must log in.");

    digestAuth.setUsername(app_user);
    digestAuth.setPassword(app_pass);
    digestAuth.setRealm(app_name);
    digestAuth.setAuthMethod(HTTPAuthMethod::DIGEST_AUTH);
    digestAuth.setAuthFailureMessage("You must log in.");

    // serve static files from LittleFS/www on / only to clients on same wifi network
    // this is where our /index.html file lives
    server.serveStatic("/", LittleFS, "/www/")->addFilter(ON_STA_FILTER);

    // serve static files from LittleFS/www-ap on / only to clients on SoftAP
    // this is where our /index.html file lives
    server.serveStatic("/", LittleFS, "/www-ap/")->addFilter(ON_AP_FILTER);

    // serve static files from LittleFS/img on /img
    // it's more efficient to serve everything from a single www directory, but this is also possible.
    server.serveStatic("/img", LittleFS, "/img/");

    // you can also serve single files
    server.serveStatic("/myfile.txt", LittleFS, "/custom.txt");

    // example callback everytime a connection is opened
    server.onOpen([](PsychicClient* client) { Serial.printf("[http] connection #%u connected from %s\n", client->socket(), client->localIP().toString().c_str()); });

    // example callback everytime a connection is closed
    server.onClose([](PsychicClient* client) { Serial.printf("[http] connection #%u closed from %s\n", client->socket(), client->localIP().toString().c_str()); });

    // api - json message passed in as post body
    server.on("/api", HTTP_POST, [](PsychicRequest* request, PsychicResponse* response) {
      //load our JSON request
      JsonDocument json;
      String body = request->body();
      DeserializationError err = deserializeJson(json, body);

      //create our response json
      JsonDocument output;
      output["msg"] = "status";

      //did it parse?
      if (err)
      {
        output["status"] = "failure";
        output["error"] = err.c_str();
      }
      else
      {
        output["status"] = "success";
        output["millis"] = millis();

        //work with some params
        if (json.containsKey("foo"))
        {
          String foo = json["foo"];
          output["foo"] = foo;
        }
      }

      //serialize and return
      String jsonBuffer;
      serializeJson(output, jsonBuffer);
      return response->send(200, "application/json", jsonBuffer.c_str()); });

    // api - parameters passed in via query eg. /api/endpoint?foo=bar
    server.on("/ip", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
      String output = "Your IP is: " + request->client()->remoteIP().toString();
      return response->send(output.c_str()); });

    // api - parameters passed in via query eg. /api/endpoint?foo=bar
    server.on("/api", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
      //create a response object
      JsonDocument output;
      output["msg"] = "status";
      output["status"] = "success";
      output["millis"] = millis();

      //work with some params
      if (request->hasParam("foo"))
      {
        String foo = request->getParam("foo")->name();
        output["foo"] = foo;
      }

      //serialize and return
      String jsonBuffer;
      serializeJson(output, jsonBuffer);
      return response->send(200, "application/json", jsonBuffer.c_str()); });

    // how to redirect a request
    server.on("/redirect", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) { return response->redirect("/alien.png"); });

    // how to do basic auth
    server.on("/auth-basic", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) { return response->send("Auth Basic Success!"); })->addMiddleware(&basicAuth);

    // how to do digest auth
    server.on("/auth-digest", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) { return response->send("Auth Digest Success!"); })->addMiddleware(&digestAuth);

    // example of getting / setting cookies
    server.on("/cookies", HTTP_GET, [](PsychicRequest* request, PsychicResponse* response) {
      int counter = 0;
      char cookie[14];
      size_t size = 14;
      if (request->getCookie("counter", cookie, &size) == ESP_OK)
      {
        // value is null-terminated.
        counter = std::stoi(cookie);
        counter++;
      }
      sprintf(cookie, "%d", counter);

      response->setCookie("counter", cookie);
      response->setContent(cookie);
      return response->send(); });

    // example of getting POST variables
    server.on("/post", HTTP_POST, [](PsychicRequest* request, PsychicResponse* response) {
      String output;
      output += "Param 1: " + request->getParam("param1")->value() + "<br/>\n";
      output += "Param 2: " + request->getParam("param2")->value() + "<br/>\n";

      return response->send(output.c_str()); });

    // you can set up a custom 404 handler.
    server.onNotFound([](PsychicRequest* request, PsychicResponse* response) { return response->send(404, "text/html", "Custom 404 Handler"); });

    // handle a very basic upload as post body
    PsychicUploadHandler* uploadHandler = new PsychicUploadHandler();
    uploadHandler->onUpload([](PsychicRequest* request, const String& filename, uint64_t index, uint8_t* data, size_t len, bool last) {
      File file;
      String path = "/www/" + filename;

      Serial.printf("Writing %d/%d bytes to: %s\n", (int)index+(int)len, request->contentLength(), path.c_str());

      if (last)
        Serial.printf("%s is finished. Total bytes: %d\n", path.c_str(), (int)index+(int)len);

      //our first call?
      if (!index)
        file = LittleFS.open(path, FILE_WRITE);
      else
        file = LittleFS.open(path, FILE_APPEND);
      
      if(!file) {
        Serial.println("Failed to open file");
        return ESP_FAIL;
      }

      if(!file.write(data, len)) {
        Serial.println("Write failed");
        return ESP_FAIL;
      }

      return ESP_OK; });

    // gets called after upload has been handled
    uploadHandler->onRequest([](PsychicRequest* request, PsychicResponse* response) {
      String url = "/" + request->getFilename();
      String output = "<a href=\"" + url + "\">" + url + "</a>";

      return response->send(output.c_str()); });

    // wildcard basic file upload - POST to /upload/filename.ext
    server.on("/upload/*", HTTP_POST, uploadHandler);

    // a little bit more complicated multipart form
    PsychicUploadHandler* multipartHandler = new PsychicUploadHandler();
    multipartHandler->onUpload([](PsychicRequest* request, const String& filename, uint64_t index, uint8_t* data, size_t len, bool last) {
      File file;
      String path = "/www/" + filename;

      //some progress over serial.
      Serial.printf("Writing %d bytes to: %s\n", (int)len, path.c_str());
      if (last)
        Serial.printf("%s is finished. Total bytes: %d\n", path.c_str(), (int)index+(int)len);

      //our first call?
      if (!index)
        file = LittleFS.open(path, FILE_WRITE);
      else
        file = LittleFS.open(path, FILE_APPEND);
      
      if(!file) {
        Serial.println("Failed to open file");
        return ESP_FAIL;
      }

      if(!file.write(data, len)) {
        Serial.println("Write failed");
        return ESP_FAIL;
      }

      return ESP_OK; });

    // gets called after upload has been handled
    multipartHandler->onRequest([](PsychicRequest* request, PsychicResponse* response) {
      PsychicWebParameter *file = request->getParam("file_upload");

      String url = "/" + file->value();
      String output;

      output += "<a href=\"" + url + "\">" + url + "</a><br/>\n";
      output += "Bytes: " + String(file->size()) + "<br/>\n";
      output += "Param 1: " + request->getParam("param1")->value() + "<br/>\n";
      output += "Param 2: " + request->getParam("param2")->value() + "<br/>\n";
      
      return response->send(output.c_str()); });

    // wildcard basic file upload - POST to /upload/filename.ext
    server.on("/multipart", HTTP_POST, multipartHandler);

    // a websocket echo server
    websocketHandler.onOpen([](PsychicWebSocketClient* client) {
      Serial.printf("[socket] connection #%u connected from %s\n", client->socket(), client->localIP().toString().c_str());
      client->sendMessage("Hello!"); });
    websocketHandler.onFrame([](PsychicWebSocketRequest* request, httpd_ws_frame* frame) {
        Serial.printf("[socket] #%d sent: %s\n", request->client()->socket(), (char *)frame->payload);
        return request->reply(frame); });
    websocketHandler.onClose([](PsychicWebSocketClient* client) { Serial.printf("[socket] connection #%u closed from %s\n", client->socket(), client->localIP().toString().c_str()); });
    server.on("/ws", &websocketHandler);

    // EventSource server
    eventSource.onOpen([](PsychicEventSourceClient* client) {
      Serial.printf("[eventsource] connection #%u connected from %s\n", client->socket(), client->localIP().toString().c_str());
      client->send("Hello user!", NULL, millis(), 1000); });
    eventSource.onClose([](PsychicEventSourceClient* client) { Serial.printf("[eventsource] connection #%u closed from %s\n", client->socket(), client->localIP().toString().c_str()); });
    server.on("/events", &eventSource);
  }
}

unsigned long lastUpdate = 0;
char output[60];

void loop()
{
  if (millis() - lastUpdate > 2000) {
    sprintf(output, "Millis: %lu\n", millis());
    websocketHandler.sendAll(output);

    sprintf(output, "%lu", millis());
    eventSource.send(output, "millis", millis(), 0);

    lastUpdate = millis();
  }
  vTaskDelay(1 / portTICK_PERIOD_MS); // Feed WDT
}