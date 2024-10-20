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

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <esp_sntp.h>
#include "_secret.h"
#include <PsychicHttp.h>
//#include <PsychicHttpsServer.h> //uncomment this to enable HTTPS / SSL

#ifndef WIFI_SSID
  #error "You need to enter your wifi credentials. Rename secret.h to _secret.h and enter your credentials there."
#endif

//Enter your WIFI credentials in secret.h
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;

// Set your SoftAP credentials
const char *softap_ssid = "PsychicHttp";
const char *softap_password = "";
IPAddress softap_ip(10, 0, 0, 1);  

//credentials for the /auth-basic and /auth-digest examples
const char *app_user = "admin";
const char *app_pass = "admin";
const char *app_name = "Your App";

//hostname for mdns (psychic.local)
const char *local_hostname = "psychic";

//#define PSY_ENABLE_SSL to enable ssl
#ifdef PSY_ENABLE_SSL
  bool app_enable_ssl = true;
  String server_cert;
  String server_key;
#endif

//our main server object
#ifdef PSY_ENABLE_SSL
  PsychicHttpsServer server;
#else
  PsychicHttpServer server;
#endif
PsychicWebSocketHandler websocketHandler;
PsychicEventSource eventSource;

//NTP server stuff
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;
struct tm timeinfo;

// Callback function (gets called when time adjusts via NTP)
void timeAvailable(struct timeval *t)
{
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  Serial.print("NTP update: ");
  char buffer[40];
  strftime(buffer, 40, "%FT%T%z", &timeinfo);
  Serial.println(buffer);
}

bool connectToWifi()
{
  //dual client and AP mode
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

  // We start by connecting to a WiFi network
  // To debug, please enable Core Debug Level to Verbose
  if (connectToWifi())
  {
    //Setup our NTP to get the current time.
    sntp_set_time_sync_notification_cb(timeAvailable);
    sntp_servermode_dhcp(1);  // (optional)
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

    //set up our esp32 to listen on the local_hostname.local domain
    if (!MDNS.begin(local_hostname)) {
      Serial.println("Error starting mDNS");
      return;
    }
    MDNS.addService("http", "tcp", 80);

    if(!LittleFS.begin())
    {
      Serial.println("LittleFS Mount Failed. Do Platform -> Build Filesystem Image and Platform -> Upload Filesystem Image from VSCode");
      return;
    }

    //look up our keys?
    #ifdef PSY_ENABLE_SSL
      if (app_enable_ssl)
      {
        File fp = LittleFS.open("/server.crt");
        if (fp)
        {
          server_cert = fp.readString();

          // Serial.println("Server Cert:");
          // Serial.println(server_cert);
        }
        else
        {
          Serial.println("server.pem not found, SSL not available");
          app_enable_ssl = false;
        }
        fp.close();

        File fp2 = LittleFS.open("/server.key");
        if (fp2)
        {
          server_key = fp2.readString();

          // Serial.println("Server Key:");
          // Serial.println(server_key);
        }
        else
        {
          Serial.println("server.key not found, SSL not available");
          app_enable_ssl = false;
        }
        fp2.close();
      }
    #endif

    //setup server config stuff here
    server.config.max_uri_handlers = 20; //maximum number of uri handlers (.on() calls)

    #ifdef PSY_ENABLE_SSL
      server.ssl_config.httpd.max_uri_handlers = 20; //maximum number of uri handlers (.on() calls)

      //do we want secure or not?
      if (app_enable_ssl)
      {
        server.listen(443, server_cert.c_str(), server_key.c_str());
        
        //this creates a 2nd server listening on port 80 and redirects all requests HTTPS
        PsychicHttpServer *redirectServer = new PsychicHttpServer();
        redirectServer->config.ctrl_port = 20424; // just a random port different from the default one
        redirectServer->listen(80);
        redirectServer->onNotFound([](PsychicRequest *request) {
          String url = "https://" + request->host() + request->url();
          return request->redirect(url.c_str());
        });
      }
      else
        server.listen(80);
    #else
      server.listen(80);
    #endif

    DefaultHeaders::Instance().addHeader("Server", "PsychicHttp");

    //serve static files from LittleFS/www on / only to clients on same wifi network
    //this is where our /index.html file lives
    // curl -i http://psychic.local/
    PsychicStaticFileHandler* handler = server.serveStatic("/", LittleFS, "/www/");
    handler->setFilter(ON_STA_FILTER);
    handler->setCacheControl("max-age=60");

    //serve static files from LittleFS/www-ap on / only to clients on SoftAP
    //this is where our /index.html file lives
    server.serveStatic("/", LittleFS, "/www-ap/")->setFilter(ON_AP_FILTER);

    //serve static files from LittleFS/img on /img
    //it's more efficient to serve everything from a single www directory, but this is also possible.
    // curl -i http://psychic.local/img/request_flow.png
    server.serveStatic("/img", LittleFS, "/img/");

    //you can also serve single files
    // curl -i http://psychic.local/myfile.txt
    server.serveStatic("/myfile.txt", LittleFS, "/custom.txt");

    //example callback everytime a connection is opened
    server.onOpen([](PsychicClient *client) {
      Serial.printf("[http] connection #%u connected from %s\n", client->socket(), client->remoteIP().toString());
    });

    //example callback everytime a connection is closed
    server.onClose([](PsychicClient *client) {
      Serial.printf("[http] connection #%u closed from %s\n", client->socket(), client->remoteIP().toString());
    });

    //api - json message passed in as post body
    // curl -i -X POST -H "Content-Type: application/json" -d '{"foo":"bar"}' http://psychic.local/api
    server.on("/api", HTTP_POST, [](PsychicRequest *request, JsonVariant &json)
    {
      JsonObject input = json.as<JsonObject>();

      //create our response json
      PsychicJsonResponse response = PsychicJsonResponse(request);
      JsonObject output = response.getRoot();

      output["msg"] = "status";
      output["status"] = "success";
      output["millis"] = millis();

      //work with some params
      if (input.containsKey("foo"))
      {
        String foo = input["foo"];
        output["foo"] = foo;
      }
  
      return response.send();
    });

    //ip - get info about the client
    // curl -i http://psychic.local/ip
    server.on("/ip", HTTP_GET, [](PsychicRequest *request)
    {
      String output = "Your IP is: " + request->client()->remoteIP().toString();
      return request->reply(output.c_str());
    });

    //client connect/disconnect to a url
    // curl -i http://psychic.local/handler
    PsychicWebHandler *connectionHandler = new PsychicWebHandler();
    connectionHandler->onRequest([](PsychicRequest *request)
    {
      return request->reply("OK");
    });
    connectionHandler->onOpen([](PsychicClient *client) {
      Serial.printf("[handler] connection #%u connected from %s\n", client->socket(), client->remoteIP().toString());
    });
    connectionHandler->onClose([](PsychicClient *client) {
      Serial.printf("[handler] connection #%u closed from %s\n", client->socket(), client->remoteIP().toString());
    });

    //add it to our server
    server.on("/handler", connectionHandler);

    //api - parameters passed in via query eg. /api?foo=bar
    // curl -i 'http://psychic.local/api?foo=bar'
    server.on("/api", HTTP_GET, [](PsychicRequest *request)
    {
      //showcase some of the variables
      Serial.println(request->host());
      Serial.println(request->uri());
      Serial.println(request->path());
      Serial.println(request->queryString());

      //create a response object
      //create our response json
      PsychicJsonResponse response = PsychicJsonResponse(request);
      JsonObject output = response.getRoot();

      output["msg"] = "status";
      output["status"] = "success";
      output["millis"] = millis();

      //work with some params
      if (request->hasParam("foo"))
      {
        String foo = request->getParam("foo")->value();
        output["foo"] = foo;
      }

      return response.send();
    });

    //JsonResponse example
    // curl -i http://psychic.local/json
    server.on("/json", HTTP_GET, [](PsychicRequest *request)
    {
      PsychicJsonResponse response = PsychicJsonResponse(request);

      char key[16];
      char value[32];
      JsonObject root = response.getRoot();
      for (int i=0; i<100; i++)
      {
        sprintf(key, "key%d", i);
        sprintf(value, "value is %d", i);
        root[key] = value;
      }

      return response.send();
    });    

    //how to redirect a request
    // curl -i  http://psychic.local/redirect
    server.on("/redirect", HTTP_GET, [](PsychicRequest *request)
    {
      return request->redirect("/alien.png");
    });

    //how to do basic auth
    // curl -i --user admin:admin http://psychic.local/auth-basic
    server.on("/auth-basic", HTTP_GET, [](PsychicRequest *request)
    {
      if (!request->authenticate(app_user, app_pass))
        return request->requestAuthentication(BASIC_AUTH, app_name, "You must log in.");
      return request->reply("Auth Basic Success!");
    });

    //how to do digest auth
    // curl -i --user admin:admin http://psychic.local/auth-digest
    server.on("/auth-digest", HTTP_GET, [](PsychicRequest *request)
    {
      if (!request->authenticate(app_user, app_pass))
        return request->requestAuthentication(DIGEST_AUTH, app_name, "You must log in.");
      return request->reply("Auth Digest Success!");
    });

    //example of getting / setting cookies
    // curl -i -b cookie.txt -c cookie.txt http://psychic.local/cookies
    server.on("/cookies", HTTP_GET, [](PsychicRequest *request)
    {
      PsychicResponse response(request);

      int counter = 0;
      if (request->hasCookie("counter"))
      {
        counter = std::stoi(request->getCookie("counter").c_str());
        counter++;
      }

      char cookie[10];
      sprintf(cookie, "%i", counter);

      response.setCookie("counter", cookie);
      response.setContent(cookie);
      return response.send();
    });

    //example of getting POST variables
    // curl -i -d "param1=value1&param2=value2" -X POST http://psychic.local/post
    server.on("/post", HTTP_POST, [](PsychicRequest *request)
    {
      String output;
      output += "Param 1: " + request->getParam("param1")->value() + "<br/>\n";
      output += "Param 2: " + request->getParam("param2")->value() + "<br/>\n";

      return request->reply(output.c_str());
    });

    //you can set up a custom 404 handler.
    // curl -i http://psychic.local/404
    server.onNotFound([](PsychicRequest *request)
    {
      return request->reply(404, "text/html", "Custom 404 Handler");
    });

    //handle a very basic upload as post body
    PsychicUploadHandler *uploadHandler = new PsychicUploadHandler();
    uploadHandler->onUpload([](PsychicRequest *request, const String& filename, uint64_t index, uint8_t *data, size_t len, bool last) {
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

      return ESP_OK;
    });

    //gets called after upload has been handled
    uploadHandler->onRequest([](PsychicRequest *request)
    {
      String url = "/" + request->getFilename();
      String output = "<a href=\"" + url + "\">" + url + "</a>";

      return request->reply(output.c_str());
    });

    //wildcard basic file upload - POST to /upload/filename.ext
    // use http://psychic.local/ to test
    server.on("/upload/*", HTTP_POST, uploadHandler);

    //a little bit more complicated multipart form
    PsychicUploadHandler *multipartHandler = new PsychicUploadHandler();
    multipartHandler->onUpload([](PsychicRequest *request, const String& filename, uint64_t index, uint8_t *data, size_t len, bool last) {
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

      return ESP_OK;
    });

    //gets called after upload has been handled
    multipartHandler->onRequest([](PsychicRequest *request)
    {
      if (request->hasParam("file_upload"))
      {
        PsychicWebParameter *file = request->getParam("file_upload");

        String url = "/" + file->value();
        String output;

        output += "<a href=\"" + url + "\">" + url + "</a><br/>\n";
        output += "Bytes: " + String(file->size()) + "<br/>\n";
        output += "Param 1: " + request->getParam("param1")->value() + "<br/>\n";
        output += "Param 2: " + request->getParam("param2")->value() + "<br/>\n";
        
        return request->reply(output.c_str());
      }
      else
        return request->reply("No upload.");
    });

    //wildcard basic file upload - POST to /upload/filename.ext
    // use http://psychic.local/ to test
    server.on("/multipart", HTTP_POST, multipartHandler);

    //a websocket echo server
    // npm install -g wscat
    // wscat -c ws://psychic.local/ws
    websocketHandler.onOpen([](PsychicWebSocketClient *client) {
      Serial.printf("[socket] connection #%u connected from %s\n", client->socket(), client->remoteIP().toString());
      client->sendMessage("Hello!");
    });
    websocketHandler.onFrame([](PsychicWebSocketRequest *request, httpd_ws_frame *frame) {
      Serial.printf("[socket] #%d sent: %s\n", request->client()->socket(), (char *)frame->payload);
      return request->reply(frame);
    });
    websocketHandler.onClose([](PsychicWebSocketClient *client) {
      Serial.printf("[socket] connection #%u closed from %s\n", client->socket(), client->remoteIP().toString());
    });
    server.on("/ws", &websocketHandler);

    //EventSource server
    // curl -i -N http://psychic.local/events
    eventSource.onOpen([](PsychicEventSourceClient *client) {
      Serial.printf("[eventsource] connection #%u connected from %s\n", client->socket(), client->remoteIP().toString());
      client->send("Hello user!", NULL, millis(), 1000);
    });
    eventSource.onClose([](PsychicEventSourceClient *client) {
      Serial.printf("[eventsource] connection #%u closed from %s\n", client->socket(), client->remoteIP().toString());
    });
    server.on("/events", &eventSource);
  }
}

unsigned long lastUpdate = 0;
char output[60];

void loop()
{
  if (millis() - lastUpdate > 2000)
  {
    sprintf(output, "Millis: %d\n", millis());
    websocketHandler.sendAll(output);

    sprintf(output, "%d", millis());
    eventSource.send(output, "millis", millis(), 0);

    lastUpdate = millis();
  }
}