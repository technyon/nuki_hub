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
//Duo Authentication Library (Passcode Authentication Example)
//GitHub: https://github.com/CiscoDevNet/Arduino-DuoAuthLibrary-ESP32
//-------------------------------------------------------------------------------------------

//Include WiFi Header File to enable Network Connectivity
#include <WiFi.h>

//Include DuoAuthLibrary Header File enabling the functions for Duo Authentication Requests
#include <DuoAuthLib.h>

//UPDATE: Update the below values with your Wireless SSID and Pre-Shared Key settings
const char* ssid = "MyWirelessAP";
const char* wirelessPsk =  "MySecretWiFiPassword";

//Global variables used for example
char duoUsername[25];
char duoPasscode[7];
bool duoUser;
const byte numChars = 25;
char userInputChars[numChars];
bool newUserInput = false;
bool readyToAuth = false;

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

	Serial.println("Duo Authentication Library (Passcode Example)");
	Serial.print("Please enter your Username :");
}

void loop(){
	//Check if Wifi is connected before proceeding
	if ((WiFi.status() == WL_CONNECTED)){

		//Declare Duo Authentication Instance
		DuoAuthLib duoAuth;

		//Check for New user input from Serial Interface
		//NOTE:  See checkSerialInput(...) function below for more details
		checkSerialInput();
		
		//Check if new data is available to be processed by the application
		if (newUserInput == true){
			newUserInput = false;
			
			Serial.println(userInputChars);
			
			if(!duoUser){
				//New Username being entered, empty the existing contents of both variables
				duoUsername[0] = '\0';
				duoPasscode[0] = '\0';
				
				//Copy the user input into the duoUsername Variable
				strcat(duoUsername, userInputChars);
				userInputChars[0] = '\0';
				
				Serial.print("Please enter your Duo Passcode :");
				
				duoUser = true;
			}else{
				//Copy the user input into the duoUsername Variable
				strcat(duoPasscode, userInputChars);
				userInputChars[0] = '\0';
				
				duoUser = false;
				readyToAuth = true;
			}
		}
		
		//-------------------------------------------------------------------------------------------
		//Primary Identity Authentication will need to be inserted where appropriate
		//If we have received both a Username and Passcode, we can proceed to requesting
		//Duo Passcode Authentication
		//-------------------------------------------------------------------------------------------
		if(readyToAuth){
			readyToAuth = false;
			
			//-------------------------------------------------------------------------------------------
			//BEGIN : Duo Authentication
			//-------------------------------------------------------------------------------------------
			//Declare Variable to hold our Duo Push Request Status
			bool duoRequestResult;
			
			//Reset the new data flag as its being processed
			newUserInput = false;
			
			//Call the begin(...) function to initialize the Duo Auth Library
			duoAuth.begin(duo_host, duo_akey, duo_skey, &timeinfo);
			
			//-------------------------------------------------------------------------------------------
			//Optional configuration functions available to control some intermediate features
			//
			//--------
			//Set HTTP Timeout for the Duo Auth Library HTTP Request.  Adjust to your requirements
			//		Default Setting - 30000ms (30 Seconds)
			//
			//duoAuth.setHttpTimeout(10000); 
			//--------
			//
			//--------
			//Sets the IP Address of the device for Duo API Auth Requests
			//		Default: 0.0.0.0
			//
			//duoAuth.setIPAddress("255.255.255.255"); 
			//--------
			//
			//-------------------------------------------------------------------------------------------
			
			//-------------------------------------------------------------------------------------------
			//Call passcodeAuth(...) function for user entered
			//This is a stopping function and will wait until a response is received, an error occurs,
			//or the session times out.  NOTE: The default HTTP Timeout for the Duo Library is 30 Seconds
			//See 'Asynchronous Push Authentication' example for an alternative Push Method
			//-------------------------------------------------------------------------------------------
			duoRequestResult = duoAuth.passcodeAuth(duoUsername, duoPasscode);

			//-------------------------------------------------------------------------------------------
			//Check if Duo Auth Request was Successful
			//This returns the True/False if the Duo Auth Library API 
			//Request was successful and received the appropriate "OK" response
			//If the result returned is 'true'
			//-------------------------------------------------------------------------------------------
			//https://duo.com/docs/authapi#/auth
			//-------------------------------------------------------------------------------------------
			if(duoRequestResult == true){
				//Check if Duo Authentication for the user was Allowed
				if(duoAuth.authSuccessful()){
					//Duo Authentication was Successful as the "allow" response was received from the API
					Serial.println("Successful Auth!");

					//-------------------------------------------------------------------------------------------
					//INSERT Your PROTECTED Code HERE
					//-------------------------------------------------------------------------------------------

				}else{
					Serial.println("Failed Auth!");
					//Duo Authentication Failed as the "allow" response was NOT received from the API

					//-------------------------------------------------------------------------------------------
					//INSERT Your Authentication FAILURE Code HERE
					//-------------------------------------------------------------------------------------------
				}
			}else{
				Serial.println("Failed Auth!");
				//Duo Authentication Failed as the "allow" response was NOT received from the API

				//-------------------------------------------------------------------------------------------
				//INSERT Your Duo API Request FAILURE Code HERE
				//-------------------------------------------------------------------------------------------
			}
			//-------------------------------------------------------------------------------------------
			//END : Duo Authentication
			//-------------------------------------------------------------------------------------------

			//-------------------------------------------------------------------------------------------
			//Reset our Variables for the next User Input/Authentication Request
			//-------------------------------------------------------------------------------------------
			userInputChars[0] = '\0';
			Serial.println("Duo Authentication Library (Passcode Example)");
			Serial.print("Please enter your Username :");
		}
	}
}

//-------------------------------------------------------------------------------------------
//Check Serial for Input and wait for the "Enter" key to be pressed
//-------------------------------------------------------------------------------------------
void checkSerialInput(){
	static byte index = 0;
	//Define Terminating Character
	char endingCharacter = '\n';
	char receivedChar;
	
	//-------------------------------------------------------------------------------------------
	//Only check for new Serial Data when new data is available from the Serial Interface and we 
	//have not seen the "Enter" key press yet.
	//-------------------------------------------------------------------------------------------
	while (Serial.available() > 0 && newUserInput == false){
		//We have new input
		receivedChar = Serial.read();
				
		if(receivedChar != endingCharacter){
			//Detected that other characters were entered
			userInputChars[index] = receivedChar;
			index++;
			if (index >= numChars) {
				index = numChars - 1;
			}
		}else{
			//Detected that the 'Enter' button was pressed
			userInputChars[index] = '\0';
			index = 0;
			
			//Update the flag telling the main loop we now have User Input for processing
			newUserInput = true;
		}
	}
}
