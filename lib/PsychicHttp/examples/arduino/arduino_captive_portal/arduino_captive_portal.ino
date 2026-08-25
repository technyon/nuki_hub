/*
  PsychicHTTP Server Captive Portal Example

  This example code is in the Public Domain (or CC0 licensed, at your option.)

  Unless required by applicable law or agreed to in writing, this
  software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
  CONDITIONS OF ANY KIND, either express or implied.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <PsychicHttp.h>

#define TAG "CAPTPORT"

// captiveportal
// credits https://github.com/me-no-dev/ESPAsyncWebServer/blob/master/examples/CaptivePortal/CaptivePortal.ino
//https://github.com/espressif/arduino-esp32/blob/master/libraries/DNSServer/examples/CaptivePortal/CaptivePortal.ino
#include <DNSServer.h> 
DNSServer dnsServer;            
class CaptiveRequestHandler : public PsychicWebHandler { // handler 
public:
  CaptiveRequestHandler() {};
  virtual ~CaptiveRequestHandler() {};
  bool canHandle(PsychicRequest*request){
    // ... if needed some tests ... return(false);
    return true;  // activate captive portal
  }
  esp_err_t handleRequest(PsychicRequest *request, PsychicResponse *response) {   
   //PsychicFileResponse response(request, LittleFS, "/captiveportal.html"); // uncomment : for captive portal page, if any, eg "captiveportal.html"
   //return response.send();                                                 // uncomment : return captive portal page
   return response->send(200,"text/html","Welcome to captive portal !");     // simple text, comment if captive portal page
  }
};
CaptiveRequestHandler *captivehandler=NULL;             // handler for captive portal

const char* ssid = "mySSID";                            // replace with your SSID (mode STATION)
const char* password = "myPassword";                    // replace with you password (mode STATION)

// Set your SoftAP credentials
const char *softap_ssid = "PsychicHttp";
const char *softap_password = "";
IPAddress softap_ip(10, 0, 0, 1);

//hostname for mdns (psychic.local)
const char *local_hostname = "psychic";

//our main server object
PsychicHttpServer server;

bool connectToWifi() {
 //dual client and AP mode
  WiFi.mode(WIFI_AP_STA);

  // Configure SoftAP
  WiFi.softAPConfig(softap_ip, softap_ip, IPAddress(255, 255, 255, 0)); // subnet FF FF FF 00
  WiFi.softAP(softap_ssid, softap_password);
  IPAddress myIP = WiFi.softAPIP();
  ESP_LOGI(TAG,"SoftAP IP Address: %s", myIP.toString().c_str());
  ESP_LOGI(TAG,"[WiFi] Connecting to %s", ssid);

  WiFi.begin(ssid, password);
  
  // Will try for about 10 seconds (20x 500ms)
  int tryDelay = 500;
  int numberOfTries = 20;

  // Wait for the WiFi event
  while (true) {
    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL:
        ESP_LOGE(TAG,"[WiFi] SSID not found");
        break;
      case WL_CONNECT_FAILED:
        ESP_LOGI(TAG,"[WiFi] Failed - WiFi not connected! Reason: ");
        return false;
        break;
      case WL_CONNECTION_LOST:
        ESP_LOGI(TAG,"[WiFi] Connection was lost");
        break;
      case WL_SCAN_COMPLETED:
        ESP_LOGI(TAG,"[WiFi] Scan is completed");
        break;
      case WL_DISCONNECTED:
        ESP_LOGI(TAG,"[WiFi] WiFi is disconnected");
        break;
      case WL_CONNECTED:
        ESP_LOGI(TAG,"[WiFi] WiFi is connected, IP address %s",WiFi.localIP().toString().c_str());
        return true;
        break;
      default:
        ESP_LOGI(TAG,"[WiFi] WiFi Status: %d",WiFi.status());
        break;
    }
    delay(tryDelay);
    
    if (numberOfTries <= 0) {
      ESP_LOGI(TAG,"[WiFi] Failed to connect to WiFi!");
      // Use disconnect function to force stop trying to connect
      WiFi.disconnect();
      return false;
    }
    else numberOfTries--;
  }

  return false;
} // end connectToWifi

void setup() {
  Serial.begin(115200);
  delay(10);

  // Wifi
  if (connectToWifi()) {  // set up our esp32 to listen on the local_hostname.local domain
    if (!MDNS.begin(local_hostname)) {
      ESP_LOGE(TAG,"Error starting mDNS");
      return;
    }
    MDNS.addService("http", "tcp", 80);

    if(!LittleFS.begin()) {
      ESP_LOGI(TAG,"ERROR : LittleFS Mount Failed.");
      return;
    }

    //setup server config stuff here
    server.config.max_uri_handlers = 20; //maximum number of uri handlers (.on() calls)
        
    DefaultHeaders::Instance().addHeader("Server", "PsychicHttp");

    // captive portal
    dnsServer.start(53, "*", WiFi.softAPIP());    // DNS requests are executed over port 53 (standard)     
    captivehandler= new CaptiveRequestHandler();  // create captive portal handler, important : after server.on since handlers are triggered on a first created/first trigerred basis
    server.addHandler(captivehandler);            // captive portal handler (last handler)
  } // end set up our esp32 to listen on the local_hostname.local domain
} // end setup

void loop() {
  dnsServer.processNextRequest();       // captive portal
}
