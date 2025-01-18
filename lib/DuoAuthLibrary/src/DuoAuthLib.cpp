/**
 *@license
 *
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


/**
 * @file DuoAuthLib.cpp
 * @brief An Arduino Library to simplify the operations of performing Duo Multi-Factor Authentication within your ESP32 Micro Controller project.
 * 
 * @url https://github.com/CiscoDevNet/Arduino-DuoAuthLibrary-ESP32
 * @version 1.0.0
 * @author Gary Oppel <gaoppel@cisco.com>
 */

//Include DuoAuthLib Library Header
#include "DuoAuthLib.h"

// Include ESP32 MDEDTLS Library
#include "mbedtls/md.h"

// Load CA CRT BUNDLE
extern const uint8_t x509_crt_imported_bundle_bin_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_imported_bundle_bin_end[]   asm("_binary_x509_crt_bundle_end");

// Include ESP NetworkClientSecure Library
#include <NetworkClientSecure.h>

// Include ESP HTTPClient Library
#include <HTTPClient.h>

// Include ArduinoJSON Library
#include <ArduinoJson.h>

//----------------------------------------------------------------
//DuoAuthLib Constructor
//----------------------------------------------------------------

//Function that handles the creation and setup of instance
DuoAuthLib::DuoAuthLib()
{
	//Initialize Return Variables
	_duoApiStat = "";
	_duoAuthTxId = "";
	_duoApiAuthResponseResult = "";
	_duoApiAuthResponseStatus = "";	

	_duoApiAuthResponseStatusMessage = "";

	_duoApiFailureCode = 0;
	_duoApiFailureMessage = "";
	_duoPushType[0] = '\0';
	_init = false;
	_async = false;
}

//Function that handles the deletion/removals of instance
DuoAuthLib::~DuoAuthLib()
{

}

//----------------------------------------------------------------
//'begin(...)' - used to initialize and prepare for an Auth request.
void DuoAuthLib::begin(const char* duoApiHost, const char* duoApiIKey, const char* duoApiSKey, struct tm* timeInfo)
{
	_timeinfo = timeInfo;
	
	_duoHost = (char *)duoApiHost;
	_duoIkey = (char *)duoApiIKey;
	_duoSkey = (char *)duoApiSKey;
	
	//Set Default HTTP Timeout to 30 Seconds
	_httpTimeout = _defaultTimeout;
	
	//Set Default IP Address to 0.0.0.0
	strcpy(_ipAddress, "0.0.0.0");

	//Set the library initialized to True
	_init = true;
}

//----------------------------------------------------------------
//DuoAuthLib Public Methods
//----------------------------------------------------------------

//----------------------------------------------------------------
// Duo Ping API
// https://duo.com/docs/authapi#/ping
bool DuoAuthLib::ping()
{
	if(!_init){
		return false;
	}
	
	bool pingSuccess = false;
	
	bool apiResponse = submitApiRequest(0, (char *)DUO_PING_API_PATH, "", (char *)"", (char *)"");

	if(apiResponse == true){
		if(_httpCode == 200){
			pingSuccess = true;
		}
	}

	return pingSuccess;
}

//----------------------------------------------------------------
// Duo Check API
// https://duo.com/docs/authapi#/check
bool DuoAuthLib::checkApiKey()
{
	if(!_init){
		return false;
	}
	
	bool duoAuthRequestResult = false;

	if(getLocalTime(_timeinfo)){  
		char hmac_password[41];
		char timeStringBuffer[TIME_BUF_STR_SIZE];	
		
		//Get Current time from timeinfo
		strftime(timeStringBuffer, sizeof(timeStringBuffer), "%a, %d %b %Y %T %z", _timeinfo);
					
		// Create empty char array to hold our string
		char hmacPayload[SIGNATURE_PAYLOAD_BUFFER_SIZE + CHECK_AUTH_BUFFER_SIZE];
				
		// Generate the required URL Request Contents for the DUO API Call
		generateHmacPayload(hmacPayload, timeStringBuffer, (char *)"GET", _duoHost, DUO_CHECK_API_PATH, (char *)"");
		
		// Generate the required URL Request Contents for the DUO API Call
		calculateHmacSha1((char *)_duoSkey, hmacPayload, hmac_password);  
		
		bool apiResponse = submitApiRequest(0, timeStringBuffer, DUO_CHECK_API_PATH, hmac_password, (char *)"");

		if(apiResponse == true){
			bool processResult = processResponse(&_lastHttpResponse);
			
			if(processResult == true){
				duoAuthRequestResult = true;
			}
		}
	}	
	return duoAuthRequestResult;
}

//----------------------------------------------------------------
// Duo Auth Status API
// https://duo.com/docs/authapi#/auth_status
bool DuoAuthLib::authStatus(String transactionId)
{	
	if(!_init){
		return false;
	}
	
	bool duoAuthRequestResult = false;

	if(transactionId.length() == 36){
		char txId[37];
		
		//Convert String to Character Array
		transactionId.toCharArray(txId, 37);
		
		if(getLocalTime(_timeinfo)){  
			char hmac_password[41];
			char timeStringBuffer[TIME_BUF_STR_SIZE];
			
			char authRequestContents[CHECK_AUTH_BUFFER_SIZE];
			authRequestContents[0] = '\0';			
			
			//Get Current time from timeinfo
			strftime(timeStringBuffer, sizeof(timeStringBuffer), "%a, %d %b %Y %T %z", _timeinfo);
			
			addRequestParameter(authRequestContents, (char *)TRANSACTION_PARAM, txId, true);
						
			// Create empty char array to hold our string
			char hmacPayload[SIGNATURE_PAYLOAD_BUFFER_SIZE + CHECK_AUTH_BUFFER_SIZE];
						
			// Generate the required URL Request Contents for the DUO API Call
			generateHmacPayload(hmacPayload, timeStringBuffer, (char *)"GET", _duoHost, DUO_AUTHSTATUS_API_PATH, authRequestContents);

			// Generate the required URL Request Contents for the DUO API Call
			calculateHmacSha1((char *)_duoSkey, hmacPayload, hmac_password);  
			
			bool apiResponse = submitApiRequest(0, timeStringBuffer, DUO_AUTHSTATUS_API_PATH, hmac_password, authRequestContents);

			if(apiResponse == true){
				bool processResult = processResponse(&_lastHttpResponse);
				
				if(processResult == true){
					duoAuthRequestResult = true;
				}
			}
		}
	}
	
	return duoAuthRequestResult;
}

//----------------------------------------------------------------
// Duo Auth API
// https://duo.com/docs/authapi#/auth
// https://duo.com/docs/authapi#authentication
bool DuoAuthLib::pushAuth(char* userName, bool async)
{
	if(!_init){
		return false;
	}
	
	bool authSuccess = false;
	_async = async;
	
	//Create new Buffer using the method below
	int userStrLen = encodeUrlBufferSize(userName);

	char encodedUsername[userStrLen];

	encodeUrl(encodedUsername, userName);
	
	return performAuth(encodedUsername, (char*)"", PUSH);
}

//----------------------------------------------------------------
// Duo Auth API
// https://duo.com/docs/authapi#/auth
// https://duo.com/docs/authapi#authentication
bool DuoAuthLib::passcodeAuth(char* userName, char* userPasscode)
{
	if(!_init){
		return false;
	}
	
	//Create new Buffer using the method below
	int userStrLen = encodeUrlBufferSize(userName);

	char encodedUsername[userStrLen];

	encodeUrl(encodedUsername, userName);
	
	return performAuth(encodedUsername, userPasscode, PASSCODE);
}

//----------------------------------------------------------------
//Duo - Set Public IP Address of Device for Auth API Request
void DuoAuthLib::setIPAddress(char* ipAddress)
{
	if(strlen(ipAddress) < MAX_IP_LENGTH){
		strcpy(_ipAddress, ipAddress);
	}
}

//----------------------------------------------------------------
//Duo - Set the Push Type String that is displayed to the end user
//See the Parameter 'Type' within the 'Duo Push' Section of the 
//Duo API Documentation: https://duo.com/docs/authapi#/auth
void DuoAuthLib::setPushType(char* pushType)
{
	int typeLength = strlen(pushType);
	
	if(typeLength > 0 && typeLength < MAX_TYPE_LENGTH){
		//Create new Buffer using the method below
		int userStrLen = encodeUrlBufferSize(pushType);

		char encodedUsername[userStrLen];

		encodeUrl(encodedUsername, pushType);
		
		strcpy(_duoPushType, encodedUsername);
	}
}

//----------------------------------------------------------------
//Duo - Set the HTTP Timeout from the default of 30 Seconds
void DuoAuthLib::setHttpTimeout(int httpTimeout)
{
	_httpTimeout = httpTimeout;
}

//----------------------------------------------------------------
//Duo Authentication API Result
//https://duo.com/docs/authapi#/auth
//https://duo.com/docs/authapi#authentication
//Returns True if the Duo Authentication Result is 'allow'
bool DuoAuthLib::authSuccessful()
{
	if(_duoApiAuthResponseResult == "allow"){
		return true;
	}else{
		return false;
	}
}

//----------------------------------------------------------------
//Duo Authentication API Result
//https://duo.com/docs/authapi#/auth
//https://duo.com/docs/authapi#authentication
//Returns True if the Duo Authentication Result is 'waiting'
bool DuoAuthLib::pushWaiting()
{
	if(_duoApiAuthResponseResult == "waiting"){
		return true;
	}else{
		return false;
	}
}

//----------------------------------------------------------------
String DuoAuthLib::getHttpResponse()
{
	return _lastHttpResponse;
}

String DuoAuthLib::getApiStat()
{
	return _duoApiStat;
}

String DuoAuthLib::getAuthResponseResult()
{
	return _duoApiAuthResponseResult;
}

String DuoAuthLib::getAuthResponseStatus()
{
	return _duoApiAuthResponseStatus;
}

String DuoAuthLib::getAuthResponseStatusMessage()
{
	return _duoApiAuthResponseStatusMessage;
}

String DuoAuthLib::getApiFailureCode()
{
	return String(_duoApiFailureCode);
}

String DuoAuthLib::getApiFailureMessage()
{
	return _duoApiFailureMessage;
}

String DuoAuthLib::getAuthTxId()
{
	return _duoAuthTxId;
}

int DuoAuthLib::getHttpCode() {
	return _httpCode;
}

//----------------------------------------------------------------

//----------------------------------------------------------------
//DuoAuthLib Private Methods
//----------------------------------------------------------------

//https://duo.com/docs/authapi#/auth
//https://duo.com/docs/authapi#authentication
bool DuoAuthLib::performAuth(char* userName, char* userPasscode, enum DUO_AUTH_METHOD authMethod)
{
	bool duoAuthRequestResult = false;
  
	if(getLocalTime(_timeinfo)){  
		char hmac_password[41];
		char timeStringBuffer[TIME_BUF_STR_SIZE];
		
		char authRequestContents[AUTH_REQUEST_BUFFER_SIZE];
		authRequestContents[0] = '\0';			
		
		//Get Current time from timeinfo
		strftime(timeStringBuffer, sizeof(timeStringBuffer), "%a, %d %b %Y %T %z", _timeinfo);
		
		//Check the authentication method and build the request accordingly.
		if(authMethod == PUSH){
			if(_async){
				addRequestParameter(authRequestContents, (char *)ASYNC_PARAM, (char *)"1");
			}
			
			addRequestParameter(authRequestContents, (char *)DEVICE_PARAM, (char *)AUTO_PARAM);
			addRequestParameter(authRequestContents, (char *)FACTOR_PARAM, (char *)PUSH_PARAM);
			addRequestParameter(authRequestContents, (char *)IPADDR_PARAM, _ipAddress);
			if(strlen(_duoPushType) > 0){
				addRequestParameter(authRequestContents, (char *)TYPE_PARAM, _duoPushType);
			}
			addRequestParameter(authRequestContents, (char *)USERNAME_PARAM, userName, true);
		}else if(authMethod == PASSCODE){
			addRequestParameter(authRequestContents, (char *)FACTOR_PARAM, (char *)PASSCODE_PARAM);
			addRequestParameter(authRequestContents, (char *)IPADDR_PARAM, _ipAddress);
			addRequestParameter(authRequestContents, (char *)PASSCODE_PARAM, userPasscode);
			if(strlen(_duoPushType) > 0){
				addRequestParameter(authRequestContents, (char *)TYPE_PARAM, _duoPushType);
			}
			addRequestParameter(authRequestContents, (char *)USERNAME_PARAM, userName, true);

		}else{
			return duoAuthRequestResult;
		}		
		
		// Create empty char array to hold our string
		char hmacPayload[SIGNATURE_PAYLOAD_BUFFER_SIZE + AUTH_REQUEST_BUFFER_SIZE];
		
		// Generate the required URL Request Contents for the DUO API Call
		generateHmacPayload(hmacPayload, timeStringBuffer, (char *)"POST", _duoHost, DUO_AUTH_API_PATH, authRequestContents);
				
		// Generate the required URL Request Contents for the DUO API Call
		calculateHmacSha1((char *)_duoSkey, hmacPayload, hmac_password);  
		
		bool apiResponse = submitApiRequest(1, timeStringBuffer, DUO_AUTH_API_PATH, hmac_password, authRequestContents);

		if(apiResponse == true){
			bool processResult = processResponse(&_lastHttpResponse);
			
			if(processResult == true){
				duoAuthRequestResult = true;
			}
		}
	}
	
	return duoAuthRequestResult;
}

//----------------------------------------------------------------
//Create and Submit API Request to Duo API Server
bool DuoAuthLib::submitApiRequest(uint8_t apiMethod, char *timeString, const char* apiPath, char *hmacPassword, char* requestContents)
{
    NetworkClientSecure *clientDuoAuth = new NetworkClientSecure;
    if (clientDuoAuth)
    {
        clientDuoAuth->setCACertBundle(x509_crt_imported_bundle_bin_start, x509_crt_imported_bundle_bin_end - x509_crt_imported_bundle_bin_start);
        {
            //Create our HTTPClient Instance
            HTTPClient http;
            
            //Build the Request URL based on the Method.
            //Append the requestContents to the end of 
            //the URL for an HTTP GET request
            String requestUrl = "https://";
            requestUrl += _duoHost;
            requestUrl += apiPath;
            
            if(apiMethod == 0 && strlen(requestContents) > 0){
                requestUrl += '?';
                requestUrl += requestContents;
            }
            
            http.begin(requestUrl); //Specify the URL
            
            // HTTP Connection Timeout
            http.setTimeout(_httpTimeout);
            
            //Set User Agent Header
            http.setUserAgent(_duoUserAgent);
            
            //Set Host Header
            http.addHeader("Host", _duoHost);
            
            //Add the required Date Header for DUO API Calls
            if(timeString){
                http.addHeader(F("Date"), String(timeString));
            }
            
            //Add Content Type Header for POST requests
            if(apiMethod == 1){
                http.addHeader(F("Content-Type"), F("application/x-www-form-urlencoded"));
            }

            //Add the required HTTP Authorization Header for the DUO API Call
            if(hmacPassword){
                http.setAuthorization(_duoIkey, hmacPassword);
            }
            
            if(apiMethod == 1){
                _httpCode = http.POST(requestContents);
            }else if(apiMethod == 0){
                _httpCode = http.GET();
            }else{
                http.end();
                return false;
            }
            
            //----------------------------------------------------------------------------------------
            //Valid Duo API Endpoints HTTP(S) Response codes.  Only respond with a valid request for 
            //these values:
            //		200 - Success
            //		400 - Invalid or missing parameters.
            //		401 - The "Authorization" and/or "Date" headers were missing or invalid.
            //NOTE: Other HTTP Codes exist; however, only those for the API endpoints are noted above,
            //		Please refer to the Duo Auth API Documentation @ https://duo.com/docs/authapi
            //----------------------------------------------------------------------------------------
            if ((_httpCode == 200) || (_httpCode == 400) || (_httpCode == 401)) { //Check for the returning code
                _lastHttpResponse = http.getString();
                http.end();
                return true;
            }else{
                _lastHttpResponse = "";
                http.end();
                return false;
            }
        }
        delete clientDuoAuth;
    }
    return false;
}

bool DuoAuthLib::processResponse(String* serializedJsonData)
{    
    JsonDocument doc;

	_duoApiStat = "";
	_duoAuthTxId = "";
	_duoApiAuthResponseResult = "";
	_duoApiAuthResponseStatus = "";	

	_duoApiAuthResponseStatusMessage = "";

	_duoApiFailureCode = 0;
	_duoApiFailureMessage = "";

	//Deserialize the json response from the Duo API Endpoints
	DeserializationError error = deserializeJson(doc, *serializedJsonData);
	
	//Check if have an error in our Deserialization before proceeding. 
	if(!error){
		const char* apiStat = doc["stat"];
		
		if(apiStat){
			_duoApiStat = String(apiStat);

			if(strcmp(apiStat,"OK") == 0){
				JsonObject response = doc["response"];

				if(_async){
					const char* duoTxId = response["txid"];
					if(duoTxId){
						_duoAuthTxId = String(duoTxId);
					}else{
						_duoAuthTxId = "";
						return false;
					}
				}else{
					const char* duoResult = response["result"];
					const char* duoStatus = response["status"];
					const char* duoStatusMsg = response["status_msg"];

					if(duoResult && duoStatus && duoStatusMsg){
						_duoApiAuthResponseResult = String(duoResult);
						_duoApiAuthResponseStatus = String(duoStatus);	
						_duoApiAuthResponseStatusMessage = String(duoStatusMsg);
					}else{
						_duoApiAuthResponseResult = "";
						_duoApiAuthResponseStatus = "";	
						_duoApiAuthResponseStatusMessage = "";
					}
				}
				return true;
			}else if(strcmp(apiStat,"FAIL") == 0){
				_duoApiFailureCode = doc["code"];
				const char* failureMessage = doc["message"];
				
				if(failureMessage){
					_duoApiFailureMessage = String(failureMessage);
				}

				return false;
			}else{
				return false;
			}
		}else{
			return false;
		}
	}else{
		_duoApiFailureCode = -1;
		_duoApiFailureMessage = "DuoAuthLib: Error processing received response";
		return false;
	}
}


void DuoAuthLib::addRequestParameter(char *requestBuffer, char* field, char* value, bool lastEntry)
{
	if(lastEntry == false){
		strcat(requestBuffer, field);
		strcat(requestBuffer, EQUALS_PAYLOAD_PARAM);
		strcat(requestBuffer, value);
		strcat(requestBuffer, AMPERSAND_PAYLOAD_PARAM);
	}else{
		strcat(requestBuffer, field);
		strcat(requestBuffer, EQUALS_PAYLOAD_PARAM);
		strcat(requestBuffer, value);
	}
}

void DuoAuthLib::generateHmacPayload(char *hmacPayload, char* timeBuffer, char* httpMethod, char* duoHost, const char* duoApiPath, char* postContents)
{
	hmacPayload[0] = 0;

	strcat(hmacPayload, timeBuffer);
	strcat(hmacPayload, NEWLINE_PAYLOAD_PARAM);
	strcat(hmacPayload, httpMethod);
	strcat(hmacPayload, NEWLINE_PAYLOAD_PARAM);
	strcat(hmacPayload, duoHost);
	strcat(hmacPayload, NEWLINE_PAYLOAD_PARAM);
	strcat(hmacPayload, duoApiPath);
	strcat(hmacPayload, NEWLINE_PAYLOAD_PARAM);
	strcat(hmacPayload, postContents);
}

void DuoAuthLib::calculateHmacSha1(char *signingKey, char *dataPayload, char *hmacSignatureChar)
{
	byte hmacSignature[20];
	hmacSignatureChar[0] = 0;

	mbedtls_md_context_t mbedTlsContext;
	
	//Select the SHA1 Hashtype
	mbedtls_md_type_t mbedTlsHashType = MBEDTLS_MD_SHA1;

	const size_t payloadLength = strlen(dataPayload);
	const size_t keyLength = strlen(signingKey);            

	mbedtls_md_init(&mbedTlsContext);
	
	mbedtls_md_setup(&mbedTlsContext, mbedtls_md_info_from_type(mbedTlsHashType), 1);
	
	mbedtls_md_hmac_starts(&mbedTlsContext, (const unsigned char *) signingKey, keyLength);
	mbedtls_md_hmac_update(&mbedTlsContext, (const unsigned char *) dataPayload, payloadLength);
	mbedtls_md_hmac_finish(&mbedTlsContext, hmacSignature);
	
	mbedtls_md_free(&mbedTlsContext);

	for(int i= 0; i< sizeof(hmacSignature); i++){
		char str[3];

		sprintf(str, "%02x", (int)hmacSignature[i]);
		strcat(hmacSignatureChar, str);
	}
}

//----------------------------------------------------------------
//Functions to read in a character array and output the calculated 
//buffer size, and Encode the input excluding the below 
//characters:
//		48-57 = Numbers ( 0123456789 )
//		65-90 = UPPPERCASE LETTERS ( ABCDEF )
//		97-122 = lowercase Letters ( abcdef )
//		45 = Dash ( - )
//		46 = Period ( . )
//		95 = Underscore ( _ )
//		126 = Tilde ( ~ )
//----------------------------------------------------------------

//----------------------------------------------------------------
//Calculate the length of the Character Array based on Input
//This function also takes into account Terminating Null
int DuoAuthLib::encodeUrlBufferSize(char *stringToEncode)
{
	//Start the count at 1 to account for the terminating null character
	int newStrLen = 1;

	//Loop Through Char Array t
	for(int i= 0; i< strlen(stringToEncode); i++){
		int charAscii = (int)stringToEncode[i];

		if((charAscii > 47 && charAscii < 58) || (charAscii > 64 && charAscii < 91) || (charAscii > 96 && charAscii < 123) || (charAscii == 45) || (charAscii == 46) || (charAscii == 95) || (charAscii == 126)){
			//Found Regular Character
			//Increment by 1
			newStrLen++;
		}else{
			//Found Character that requires URL encoding
			//Increment by 3
			newStrLen += 3;
		}
	}
	return newStrLen;
}

//----------------------------------------------------------------
//Function to read in a character array and output a URL Encoded 
//and write the new variable to the destination variable
void DuoAuthLib::encodeUrl(char *destination, char *stringToEncode)
{
	//Empty our Character Array before proceeding
	destination[0] = '\0';

	//Loop Through Char Array to perform urlEncode as required
	for(int i= 0; i< strlen(stringToEncode); i++){
		int charAscii = (int)stringToEncode[i];

		if((charAscii > 47 && charAscii < 58) || (charAscii > 64 && charAscii < 91) || (charAscii > 96 && charAscii < 123) || (charAscii == 45) || (charAscii == 95) || (charAscii == 126) || (charAscii == 46)){
			char str[2];
			
			//Output only the Single Character as it does not need to be encoded
			sprintf(str, "%c", (int)stringToEncode[i]);
			strcat(destination, str);
		}else{
			char str[4];
			
			//Output the URL Encoded Format '%XX'
			sprintf(str, "%%%02X", (int)stringToEncode[i]);
			strcat(destination, str);
		}
	}
}
