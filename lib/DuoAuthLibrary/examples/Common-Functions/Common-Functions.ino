/**
 *@license
 *Copyright 2020 Cisco Systems, Inc. or its affiliates
 *
 *Licensed under the Apache License, Version 2.0 (the "License");
 *you may not use this file except in compliance with the License.
 *You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing, software
 *distributed under the License is distributed on an "AS IS" BASIS,
 *WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *See the License for the specific language governing permissions and
 *limitations under the License.
 */
 
//-------------------------------------------------------------------------------------------
//Duo Authentication Library (Common Library Functions Example)
//GitHub: https://github.com/CiscoDevNet/Arduino-DuoAuthLibrary-ESP32
//-------------------------------------------------------------------------------------------

//Include WiFi Header File to enable Network Connectivity
#include <WiFi.h>

//Include DuoAuthLibrary Header File enabling the functions for Duo Authentication Requests
#include <DuoAuthLib.h>

//UPDATE: Update the below values with your Wireless SSID and Pre-Shared Key settings
const char* ssid = "MyWirelessAP";
const char* wirelessPsk =  "MySecretWiFiPassword";

//-------------------------------------------------------------------------------------------
//START: Duo Authentication Library Required Variables

//Initialize the Time Variable Globally since it is needed for Duo Auth and potentially the
//primary Authentication Provider
struct tm timeinfo;

//UPDATE: Provide an accurate Local or Internet Based Time-source (NTP) (IP or FQDN)
//If using a FQDN, please ensure that DNS Servers are configured or provided through DHCP
const char* ntpServer = "pool.ntp.org";

//Duo Auth API uses UTC time, which is queried against the above NTP Server. 
//Adjusting these values from '0' are not necessary
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;

//UPDATE: Update these variables with the appropriate Duo Application API values
const char* duo_host = "";
const char* duo_akey = "";
const char* duo_skey = "";

//END: Duo Authentication Library Required Variables
//-------------------------------------------------------------------------------------------

void setup(){
	//Start Serial Connection at 115200 Baud
	Serial.begin(115200);

	delay(1000);
	
	//Start WiFi with the provided SSID and PSK
	WiFi.begin(ssid, wirelessPsk); 
	
	//Wait for a valid WiFi Connection
	while (WiFi.status() != WL_CONNECTED){
		delay(1000);
		Serial.println("Connecting to WiFi..");
	}
	
	Serial.println("Connected to the WiFi network");    

	Serial.println("Configuring NTP Client");
	//Configure SNTP Settings for an Accurate Time-source
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
	
	Serial.println("Duo Authentication Library (Common Functions Example)");
	
	//Declare Duo Authentication Instance
	DuoAuthLib duoAuth;
	
	//Call the begin(...) function to initialize the Duo Auth Library
	duoAuth.begin(duo_host, duo_akey, duo_skey, &timeinfo);

	//This function allows you to check if the Duo API AKey, SKey, and Host are correct and Valid.
	//This is good for validating or testing that the Duo variables correctly are correctly set
	bool checkAPIKey = duoAuth.checkApiKey();
	
	//Let's see the response from the above command
	//If it is 'true' we have successfully authenticated against the Duo Auth API
	if(checkAPIKey){
		Serial.println("Duo API Credentials are VALID");
	}else{
		//The Credential validation has failed for the Duo Auth API, please check the AKey, SKey, and Duo Hostname
		Serial.println("Duo API Credentials are INVALID");
	}
	
	//Example providing the HTTP Response code from DuoAuthLib's last API request
	Serial.print("DuoAuthLib:HTTP Code : ");
	Serial.println(String(duoAuth.getHttpCode()));
	
	//Example providing the Duo Auth API Status from the DuoAuthLib's last API request
	Serial.print("DuoAuthLib:getApiStat : ");
	Serial.println(duoAuth.getApiStat());
	
	//Example providing the Duo Authentication Result from the DuoAuthLib's last API request
	//NOTE: This value is the decision on the Authentication Request.  This must match "allow"
	//for a successful Auth Request.  https://duo.com/docs/authapi#/auth under Response Formats
	//
	//To simplify the ease of use, leverage the 'authSuccessful(...)' function instead of the
	//below function, unless advanced control is required.
	Serial.print("DuoAuthLib:getAuthResponseResult : ");
	Serial.println(duoAuth.getAuthResponseResult());
	
	//Example providing Duo's Authentication Status from DuoAuthLib's last API request
	Serial.print("DuoAuthLib:getAuthResponseStatus : ");
	Serial.println(duoAuth.getAuthResponseStatus());
	
	//Example providing the HTTP Response code from DuoAuthLib's last API request
	Serial.print("DuoAuthLib:getAuthResponseStatusMessage : ");
	Serial.println(duoAuth.getAuthResponseStatusMessage());
	
	//Example providing Duo's API Failure code from DuoAuthLib's last API request
	//See https://help.duo.com/s/article/1338 for more details
	Serial.print("DuoAuthLib:getApiFailureCode : ");
	Serial.println(duoAuth.getApiFailureCode());
	
	//Example providing Duo's API Failure Message from DuoAuthLib's last API request
	//See https://help.duo.com/s/article/1338 for more details
	Serial.print("DuoAuthLib:getApiFailureMessage : ");
	Serial.println(duoAuth.getApiFailureMessage());
	
	//Example providing the RAW HTTP Response from DuoAuthLib's last API request
	Serial.print("DuoAuthLib:HTTP Raw Response : \n");
	Serial.println(duoAuth.getHttpResponse());
}

void loop(){

	if ((WiFi.status() == WL_CONNECTED)){
		//-------------------------------------------------------------------------------------------
		//Loop Function Code Block - Intentionally Left Blank (Example in 'setup()' function above)
		//-------------------------------------------------------------------------------------------
	}
}
