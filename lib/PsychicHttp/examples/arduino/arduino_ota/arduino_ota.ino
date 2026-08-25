/*
  Over The Air (OTA) update example for PsychicHttp web server

  This example code is in the Public Domain (or CC0 licensed, at your option.)

  Unless required by applicable law or agreed to in writing, this
  software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
  CONDITIONS OF ANY KIND, either express or implied.
  
*/
#define TAG "OTA" // ESP_LOG tag

// PsychicHttp
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <PsychicHttp.h>
PsychicHttpServer server; // main server object
const char *local_hostname = "psychichttp"; // hostname for mdns

// OTA
#include <Update.h> 
bool esprestart=false; // true if/when ESP should be restarted, after OTA update

// Wifi
const char *ssid = "SSID";         // your SSID
const char *password = "PASSWORD"; // your PASSWORD

bool connectToWifi() { // Wifi
  //client in STA mode
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid,password);

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
}


// =======================================================================
// setup
// =======================================================================
void setup()
{ Serial.begin(115200);
  delay(10);
  
  // Wifi
  if (connectToWifi()) {  //set up our esp32 to listen on the local_hostname.local domain
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
    server.config.max_uri_handlers = 20;     //maximum number of uri handlers (.on() calls) 

    DefaultHeaders::Instance().addHeader("Server", "PsychicHttp");

    //server.maxRequestBodySize=2*1024*1024;   // 2Mb, change default value if needed
    //server.maxUploadSize=64*1024*1024;       // 64Mb, change default value if needed 
    
    //you can set up a custom 404 handler.
    // curl -i http://psychic.local/404
    server.onNotFound([](PsychicRequest *request, PsychicResponse *response) {
      return response->send(404, "text/html", "Custom 404 Handler");
    });

    // OTA
    PsychicUploadHandler *updateHandler = new PsychicUploadHandler(); // create handler for OTA update
    updateHandler->onUpload([](PsychicRequest *request, const String& filename, uint64_t index, uint8_t *data, size_t len, bool last) { // onUpload
      /* callback to upload code (firmware) or data (littlefs)
       * callback is triggered for each file chunk, from first chunk (index is 0) to last chunk (last is true)
       * callback is triggered by handler in handleRequest(), after _multipartUploadHandler() * 
       * filename : name of file to upload, with naming convention below 
       * "*code.bin" for code (firmware) update, eg "v1_code.bin"
       * "*littlefs.bin" for data (little fs) update, eg "v1_littlefs.bin"     * 
       */

    int command;   //command : firmware and filesystem update type, ie code (U_FLASH 0) or data (U_SPIFFS 100)       
    ESP_LOGI(TAG,"updateHandler->onUpload _error %d Update.hasError() %d last %d", Update.getError(), Update.hasError(), last);

    // Update.abort() replaces 1st error (eg "UPDATE_ERROR_ERASE") with abort error ("UPDATE_ERROR_ABORT") so root cause is lost
    if (!Update.hasError()) { // no error encountered so far during update, process current chunk
      if (!index){ // index is 0, begin update (first chunk)
        ESP_LOGI(TAG,"update begin, filename %s", filename.c_str());
        Update.clearError();         // first chunk, clear Update error if any
        // check if update file is code, data or sd card one
        if (!filename.endsWith("code.bin") && !filename.endsWith("littlefs.bin")) { //  incorrect file name         
            ESP_LOGE(TAG,"ERROR : filename %s format is incorrect", filename.c_str());
            if (!Update.hasError()) Update.abort();
            return(ESP_FAIL);
        } // end incorrect file name
        else { // file name is correct
              // check update type : code or data
              if (filename.endsWith("code.bin")) command=U_FLASH;       // update code
              else command=U_SPIFFS;                                    // update data
              if (!Update.begin(UPDATE_SIZE_UNKNOWN, command)) {        // start update with max available size           
                // error, begin is KO        
                if (!Update.hasError()) Update.abort();                 // abort
                ESP_LOGE(TAG,"ERROR : update.begin error Update.errorString() %s",Update.errorString());                   
                return(ESP_FAIL);
              }
        } // end file name is correct          
      } // end begin update
        
      if ((len) && (!Update.hasError())) { // ongoing update if no error encountered
        if (Update.write(data, len) != len) {  
          // error, write is KO
          if (!Update.hasError()) Update.abort();
          ESP_LOGE(TAG,"ERROR : update.write len %d Update.errorString() %s",len, Update.errorString()) ;           
          return(ESP_FAIL);
        }   
      } // end ongoing update
        
      if ((last) && (!Update.hasError())) { // last update if no error encountered
        if (Update.end(true)) { // update end is OKTEST
          ESP_LOGI(TAG, "Update Success: %u written", index+len);          
        }
        else { // update end is KO                  
          if (!Update.hasError()) Update.abort(); // abort             
          ESP_LOGE(TAG,"ERROR : update end error Update.errorString() %s", Update.errorString());            
          return(ESP_FAIL);
        }
      } // last update if no error encountered
      return(ESP_OK);
    } // end no error encountered so far during update, process current chunk
    else { // error encountered so far during update
      return(ESP_FAIL);
    }
    });  // end onUpload

    updateHandler->onRequest([](PsychicRequest *request, PsychicResponse *response) {  // triggered when update is completed (either OK or KO) and returns request's response (important)
      String result; // request result
      // code below is executed when update is finished
      if (!Update.hasError()) { // update is OK
        ESP_LOGI(TAG,"Update code or data OK Update.errorString() %s", Update.errorString());
        result = "<b style='color:green'>Update done for file.</b>";
        return response->send(200,"text/html",result.c_str());
        // ESP.restart();                                              // restart ESP if needed
      } // end update is OK
      else { // update is KO, send request with pretty print error
        result = " Update.errorString() " + String(Update.errorString()); 
        ESP_LOGE(TAG,"ERROR : error %s",result.c_str());    
        return response->send(500, "text/html", result.c_str());
      } // end update is KO
    });

    server.on("/update", HTTP_GET, [](PsychicRequest*request, PsychicResponse *res){
      PsychicFileResponse response(res, LittleFS, "/update.html");
      return response.send();
    });
    
    server.on("/update", HTTP_POST, updateHandler);

    server.on("/restart", HTTP_POST, [](PsychicRequest *request, PsychicResponse *response) {
      String output = "<b style='color:green'>Restarting ...</b>";
      ESP_LOGI(TAG,"%s",output.c_str());      
      esprestart=true;
      return response->send(output.c_str());      
    });    
 } // end onRequest

} // end setup
  
// =======================================================================
// loop
// =======================================================================
void loop() {
  delay(2000);
  if (esprestart) ESP.restart(); // restart ESP
} // end loop
