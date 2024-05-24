/* Copyright (c) Dirk-Willem van Gulik, All rights reserved.
 *  
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * 
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <lwip/apps/sntp.h>

#include <TOTP-generator.hpp>

#ifndef WIFI_NETWORK
#define WIFI_NETWORK "mySecretWiFiPassword"
#warning "You propably want to change this line!"
#endif

#ifndef WIFI_PASSWD
#define WIFI_PASSWD "mySecretWiFiPassword"
#warning "You propably want to change this line!"
#endif

#ifndef NTP_SERVER
#define NTP_SERVER "nl.pool.ntp.org"
#warning "You MUST set an appropriate ntp pool - see http://ntp.org"
#endif

#ifndef NTP_DEFAULT_TZ
#define NTP_DEFAULT_TZ "CET-1CEST,M3.5.0,M10.5.0/3"
#endif

const char* ssid = WIFI_NETWORK;
const char* password = WIFI_PASSWD;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("\n\n" __FILE__ "Started");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Connect Failed! Rebooting...");
    delay(1000);
    ESP.restart();
  }

  // we need a reasonable accurate time for TOTP to work.
  //
  configTzTime(NTP_DEFAULT_TZ, NTP_SERVER);
}


void loop() {
  // Print the one time passcode every seconds;
  //
  static unsigned long lst = millis();
  if (millis() - lst < 1000)
    return;
  lst = millis();

  time_t t = time(NULL);
  if (t < 1000000) {
    Serial.println("Not having a stable time yet.. TOTP is not going to work.");
    return;
  };

  // Seed value - as per the QR code; which is in fact a base32 encoded
  // byte array (i.e. it is binary).
  //
  const char * seed = "ORUGKU3FMNZGK5CTMVSWI===";

  // Example of the same thing - but as usually formatted when shown
  // as the 'alternative text to enter'
  //
  // const char * seed = "ORU GKU 3FM NZG K5C TMV SWI";

  String * otp = TOTP::currentOTP(seed);

  Serial.print(ctime(&t));
  Serial.print("   TOTP at this time is: ");
  Serial.println(*otp);
  Serial.println();

  delete otp;
}

