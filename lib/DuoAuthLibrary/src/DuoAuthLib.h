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
 * @file DuoAuthLib.h
 * @brief An Arduino Library to simplify the operations of performing Duo Multi-Factor Authentication within your ESP32 Micro Controller project.
 * 
 * @url https://github.com/CiscoDevNet/Arduino-DuoAuthLibrary-ESP32
 * @version 1.0.0
 * @author Gary Oppel <gaoppel@cisco.com>
 */

//Verify that the Duo Auth Library descriptor is only included once
#ifndef DuoAuthLib_H
#define DuoAuthLib_H

#include <memory>
#include <Arduino.h>

enum DUO_AUTH_METHOD
{
	PUSH,
	PASSCODE
};
/*!
 * @brief Main Duo Authentication Library class
 */
class DuoAuthLib
{
public:
	//Public Class Functions
	//-------------------------
	//External Functions which abstract Duo API Authentication Requests
	
	DuoAuthLib();
	~DuoAuthLib();
	
	/**
	 * @brief Initializes the Duo Auth Library
	 * 
	 * @param duoApiHost Contains the Duo API Hostname
	 * @param duoApiIKey Contains the Duo Integration Key (IKey)
	 * @param duoApiSKey Contains the Duo Secret Key (SKey)
	 * @param timeInfo Contains the Memory Pointer to the timeinfo Structure Declaration
	 */
	void begin(const char* duoApiHost, const char* duoApiIKey, const char* duoApiSKey, struct tm* timeInfo);
	
	/**
	 * @brief Performs a Duo 'ping' API Query to check if alive
	 * @return Returns `true` if Ping API Call is Successful
	 */
	bool ping();
	
	/**
	 * @brief Performs a Duo 'check' API Query to check if provided API-Host, Integration Key (IKey), or Signing Key (SKey) are valid
	 * @return Returns `true` if provided details for the Duo API are correct
	 */
	bool checkApiKey();
	
	/**
	 * @brief Performs a Duo 'auth_status' API query to check if alivethe status of provided transaction Id
	 * @param transactionId Asynchronous Duo Push Transaction Id to get the status 
	 * @return Returns `true` API call was successful
	 */
	bool authStatus(String transactionId);
	
	/**
	 * @brief Performs a Duo Push Request
	 * @param userName Username of the user to send a Push Request
	 * @param async Set this to 'true' to enable Asynchronous mode.  Duo will return a Transaction ID for checking status updates later.
	 * @return Returns `true` API call was successful
	 */
	bool pushAuth(char* userName, bool async = false);
	
	/**
	 * @brief Performs a Duo Passcode Authentication Request
	 * @param userName Username of the user who is verifying their Duo passcode
	 * @param userPasscode 6 Digit Duo Passcode of the user
	 * @return Returns `true` API call was successful
	 */
	bool passcodeAuth(char* userName, char* userPasscode);

	/**
	 * @brief Checks if the last Duo Push/Passcode Request's Authentication was Successful
	 * @return Returns `true` if Duo Authentication was successful (E.g. Duo Auth API Result = 'allow')
	 */
	bool authSuccessful();
	
	/**
	 * @brief Checks if the status returned from the last Duo Request is in the Waiting State (Only Applies to Asynchronous Pushes)
	 * @return Returns `true` if Duo Push is in waiting state (E.g. Duo Auth API Result = 'waiting')
	 */	
	bool pushWaiting();
	
	/**
	 * @brief Sets the HTTP Timeout for the Duo API Request (Default: 30000ms [30 seconds])
	 * @param httpTimeout Value in milliseconds for Duo's HTTP API Requests
	 */	
	void setHttpTimeout(int httpTimeout);

	/**
	 * @brief Sets the IP Address of the device for Duo API Auth Requests (Default: 0.0.0.0)
	 * @param ipAddress Value contains the Public IP Address of the Device for additonal Duo Policy Controls
	 */	
	void setIPAddress(char* ipAddress);

	/**
	 * @brief Sets the Duo Mobile Application's Push Value.  This string is displayed in the Duo Mobile app before the word "request".
	 * @param pushType Value contains the text to be displayed on the Push Notification Screen
	 */	
	void setPushType(char* pushType);
	
	/**
	 * @brief Sets the Push Type Notification Text that is displayed to the Enduser's Mobile Device. (Only supported with Duo Push functionality)
	 * @param Character Array of the Duo Push Type Text
	 */		
	int getHttpCode();
	
	/**
	 * @brief Gets the RAW HTTP Response from the last Duo Request 
	 * @return Returns the RAW HTTP Response
	 */	
	String getHttpResponse();
	
	/**
	 * @brief Gets the API Stat Response from the last Duo Request 
	 * @return Returns the API Stat Response ('OK', 'FAIL')
	 */	
	String getApiStat();
	
	/**
	 * @brief Gets the Auth Response Result from the last Duo Request
	 * @return Returns the Auth Response Result
	 */	
	String getAuthResponseResult();
	
	/**
	 * @brief Gets the Auth Response Status from the last Duo Request
	 * @return Returns the Auth Response Status
	 */		
	String getAuthResponseStatus();
	
	/**
	 * @brief Gets the Auth Response Status Message from the last Duo Request 
	 * @return Returns the Auth Response Status Message
	 */		
	String getAuthResponseStatusMessage();
	
	/**
	 * @brief Sets the Duo Mobile Application's Push Value.  This string is displayed in the Duo Mobile app before the word "request".
	 * @param pushType Value contains the text to be displayed on the Push Notification Screen
	 */	
	String getAuthTxId();
		
	//Duo API Error Code Reference Table
	//https://help.duo.com/s/article/1338
	
	/**
	 * @brief Gets the API Failure Code from the last Duo Request 
	 * @return Returns the API Failure Code
	 */	
	String getApiFailureCode();
	
	/**
	 * @brief Gets the API Failure Message from the last Duo Request 
	 * @return Returns the API Failure Message
	 */	
	String getApiFailureMessage();
	
protected:
	
	//Protected Class Functions
	//-------------------------
	
	//Internal Functions to handle abstracting core and common Library Functions
	bool performAuth(char* userId, char* userPasscode, enum DUO_AUTH_METHOD authMethod);
	bool submitApiRequest(uint8_t apiMethod, char *timeString, const char* apiPath, char *hmacPassword, char* requestContents);
	bool processResponse(String* serializedJsonData);
	
	//Functions to generate the API Request to the required format
	void generateHmacPayload(char *hmacPayload, char* timeBuffer, char* httpMethod, char* duoHost, const char* duoApiPath, char* postContents);
	void addRequestParameter(char *requestBuffer, char* field, char* value, bool lastEntry = false);
	
	//Function that Calculates the HMAC-SHA1 Signature for the Duo API Call
	void calculateHmacSha1(char *signingKey, char *dataPayload, char *hmacSignatureChar);
	
	//URL Encoding Functions
	int encodeUrlBufferSize(char *stringToEncode);
	void encodeUrl(char *destination, char *stringToEncode);
	
	//Duo Auth Library Initialized Flag
	bool _init;
	
	//Buffer size for Date/Time Output Character Array
	const int TIME_BUF_STR_SIZE = 36;
	
	//Maximum IPv4 Length is 16 including null termination
	static const int MAX_IP_LENGTH = 16;

	//Maximum Length of the 'Push Type' Notification String
	static const int MAX_TYPE_LENGTH = 21;	
	
	//Maximum Length of various miscellaneous variables
	static const int MAX_METHOD_LENGTH = 5;
	static const int MAX_HOSTNAME_LENGTH = 64;
	static const int MAX_API_ENDPOINT_LENGTH = 24;
	
	//Maximum Username Length
	//NOTE: Usernames can contain e-mail addresses. 
	//      Tested & validated with 44 character length Email Address (username). 
	//      Maximum username length selected is 50, with 49 being the usable maximum.
	static const int MAX_USER_LENGTH = 50;
	
	static const int MAX_PARAM_LENGTH = 10;
	static const int MAX_PAYLOAD_LENGTH = 20;
	
	const int SIGNATURE_PAYLOAD_BUFFER_SIZE = TIME_BUF_STR_SIZE + MAX_METHOD_LENGTH + MAX_HOSTNAME_LENGTH + MAX_API_ENDPOINT_LENGTH;
	
	//Required Parameters for Duo API Auth Requests
	const char* ASYNC_PARAM = "async";
	const char* AUTO_PARAM = "auto";
	const char* DEVICE_PARAM = "device";
	const char* FACTOR_PARAM = "factor";
	const char* IPADDR_PARAM = "ipaddr"; 
	const char* PASSCODE_PARAM = "passcode";
	const char* PUSH_PARAM = "push";
	const char* TRANSACTION_PARAM = "txid";
	const char* TYPE_PARAM = "type";
	const char* USERNAME_PARAM = "username";
	
	//Common variables for Duo API Auth Requests
	const char* NEWLINE_PAYLOAD_PARAM = "\n";
	const char* AMPERSAND_PAYLOAD_PARAM = "&";
	const char* EQUALS_PAYLOAD_PARAM = "=";

	//Duo Auth API URL Paths
	const char* DUO_PING_API_PATH = "/auth/v2/ping";
	const char* DUO_CHECK_API_PATH = "/auth/v2/check";
	const char* DUO_AUTH_API_PATH = "/auth/v2/auth";
	const char* DUO_AUTHSTATUS_API_PATH = "/auth/v2/auth_status";
		
	//Duo Auth Library HTTP User Agent
	String _duoUserAgent = "DuoAuthLib/1.0 (ESP32HTTPClient)";
	
	//Variable to hold IP Address for Duo Authentication Requests
	char _ipAddress[MAX_IP_LENGTH];
	
	//Variable to hold Push Type string, which is displayed on Push notifications
	char _duoPushType[MAX_TYPE_LENGTH];
	
	//Variable holds if the Push request is Asynchronous
	bool _async;
	
	//Duo Auth Library Required Variables for API interface
	char* _duoHost;
	char* _duoIkey;
	char* _duoSkey;
	
	//Asynchronous Request ID Variable
	char* _asyncRequestId;
	
	//Duo Auth Library HTTP Timeout value (Default: 30000 [30 seconds])
	int _defaultTimeout = 30000;
	int _httpTimeout;

	//HTTP code returned from the Duo API Request
	int _httpCode;

	//Return Variables for Public Functions
	String _duoApiStat;
	String _duoAuthTxId;
	String _duoApiAuthResponseResult;
	String _duoApiAuthResponseStatus;
	String _duoApiAuthResponseStatusMessage;
	int _duoApiFailureCode;
	String _duoApiFailureMessage;
	String _lastHttpResponse;
	
	//Buffer sizes for Check Auth API User provided variables
	const int CHECK_AUTH_BUFFER_SIZE = 42;
	
	//Buffer size for Authentication Request API.
	//This buffer multiplies the max user length by 3 as the assumption would be all characters would require URL Encoding
	const int AUTH_REQUEST_BUFFER_SIZE = 64 + (MAX_USER_LENGTH * 3);
	
	//Empty Pointer for the `timeinfo` Variable passed in by the end user from the 'begin(...)' function
	struct tm* _timeinfo = nullptr;
};

#endif
