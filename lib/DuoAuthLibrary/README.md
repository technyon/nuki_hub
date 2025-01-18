# Duo Authentication Library for ESP32 Micro Controllers

*An Arduino Library to simplify the operations of performing Duo Multi-Factor Authentication within your ESP32 Micro Controller project*

---

## Overview

The Duo Authentication Library was created to abstract the [Duo Auth API Authentication Requirements](https://duo.com/docs/authapi#authentication).  These requirements, which utilizes the HMAC_SHA1 algorithm, NTP Time Source, and Duo Application details (API Host, Application/Integration Key, and Secret Key) to sign the properly formatted requests before being sent to Duo's Auth API.  The library has been targeted for the Espressif ESP32 Micro Controllers, due to the built-in crypto hardware support for both the HTTPS and HMAC Signature requirements.  

Duo Security extends various API Endpoints for use.  In particular, this Library's focus is on Duo's [Authentication API ](https://duo.com/docs/authapi).

## Features

The Duo Authentication Library enables the following features/functionality:

- Duo Mobile Push Authentication
- Duo Asynchronous Mobile Push Authentication
- Duo Passcode Authentication
- Ability to set device's IP Address for Duo Auth API Requests
- Ability to set Duo's Mobile Push Notification Type (This string is displayed before the word "Request")
- Various Duo Authentication API Response fields exposed with easy to use functions
- **Includes 4 example sketches** to demonstrate the use of the library

## Key Design Requirements

The library was created with the following Key Duo API Requirements in Mind:

- Accurate Time Source for Signing the Authorization Signature
	- *Note: This variable can be declared as Global, Private 'setup();' or 'loop();', or within your own Method/Function that will interface with the Duo Auth Library! It is strongly suggested that you leverage a Global variable if you are using a primary authentication source as well!*
	
- Simple to use functions for anyone to get up and quickly as well as incorporating into your existing projects
- Built-in URL Encoding as required for certain usernames or other API properties
- Functions that enable granular 'Authentication' decision making based on Duo's Auth API Responses
- Initial Library scope primarily targeted mainly Duo Push and Passcode functionality

## Technologies & Frameworks Used
The library was built to provide a simple and easy to use interface for the Duo Auth API on Espressif's ESP32 Micro Controllers.  The library is written for the use within the Arduino IDE.

**Cisco Products & Services:**

- Duo Security

**Duo Authentication API Endpoints in use:**

This library abstracts most of the [Duo Auth API](https://duo.com/docs/authapi) functionality.  The specific API Endpoints leveraged by this library are as follows:

- GET ```/auth/v2/ping```
- GET ```/auth/v2/check```
- POST ```/auth/v2/auth```
- GET ```/auth/v2/auth_status```

**Tools & Frameworks:**

- Arduino IDE (*Tested on v1.8.12*) or PlatformIO for VSCode IDE
- [Arduino Core for the ESP32](https://github.com/espressif/arduino-esp32)
- [ArduinoJson Library for Arduino and IoT Devices](https://github.com/bblanchon/ArduinoJson)

	**I would like to thank all the authors & contributers on the previously mentioned Tools & Frameworks!**
	
	The Documentation and Guides available from these tools, contained the details needed to bring this Library to life.  Below are links to references used for the above Tools & Frameworks:

	- Arduino - Writing a Library for Arduino [https://www.arduino.cc/en/Hacking/LibraryTutorial](https://www.arduino.cc/en/Hacking/LibraryTutorial)
	- ArduinoJson - Deserialization Tutorial [https://arduinojson.org/v6/doc/deserialization/](https://arduinojson.org/v6/doc/deserialization/)

**Hardware Required:**

- Espressif ESP32 WiFi Micro-controller
- User Input Device
	- 10 Digit Keypad
	- LCD Touch Screen
	- Serial/RS-232 Device
	- RFID Reader/NFC Reader
	- Etc..

## Installation

#### Arduino IDE
*Note: This document assumes that your Arduino IDE is already setup and configured for Espressif's ESP32 Micro Controllers (Arduino Core for the ESP32)*

- To install the Duo Authentication Library in your Arduino IDE, follow the instructions below:
	- Browse to the Duo Authentication Library Releases GitHub Page
	- Download Latest Duo Authentication Library package version (Choose ZIP Package)
	- Follow the **"Installing Additional Arduino Libraries"** documentation at [Arduino's Library Documentation Site](https://www.arduino.cc/en/guide/libraries)

#### PlatformIO for VSCode IDE
- To install the Duo Authentication Library in your PlatformIO for VSCode IDE, follow the instructions below:
	- Open VSCode PlatformIO Core CLI
	- Execute the following installation command
		- `pio lib install https://github.com/CiscoDevNet/Arduino-DuoAuthLibrary-ESP32/archive/v1.0.0.zip`

## Library Usage

### Example Sketches

Included as part of the Duo Authentication Library are 4 Arduino example sketches, which outline and demonstrate how to use the library.  The examples included are:
- **Push Authentication** - Provides example on a Duo Library Push Authentication
- **Passcode Authentication** - Provides example on a Duo Library Passcode Authentication
- **Asynchronous Push Authentication** - Provides example on an Asynchronous Duo Push Authentication
- **Common Functions** - Provides examples on the Libraries Common Functions

### Configuration Requirements

The following parameters are required inputs into the Duo Authentication Library:

- NTP Server (Intranet or Internet) and other Time Variables

	```cpp
	const char* ntpServer = "pool.ntp.org";
	
	//Adjusting these values from '0' are not necessary
	const long  gmtOffset_sec = 0;
	const int   daylightOffset_sec = 0;
	```
	
- WiFi SSID & Pre-Shared Key
	```cpp
	const char* ssid = "MyWirelessAP";
	const char* wirelessPsk =  "MySecretWiFiPassword";
	```
	
- Time Info Variable Declaration & Configuration

	The Time Info structure provides the necessary variable for the NTP server to update the time, which is used by the Duo Auth Library.

	```cpp
	//Create timeinfo variable.  Note: This can be declared globally or within the function using the Duo Auth Library
	struct tm timeinfo;
	
	//Configure the Time settings
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
	```
	
- Root Certificate Authority (CA) Public Certificate for the Duo API Host

	See [Root Certificate Authority Section](/README.md#root-certificate-authority)

- Duo API Hostname
	```cpp
	const char* duo_host = "";
	```

- Duo Application/Integration Key
	```cpp
	const char* duo_akey = "";
	```

- Duo Secret Key
	```cpp
	const char* duo_skey = "";
	```
	
- Username
	
- Passcode of User, if using Duo Passcode Authentication
	
*For a more comprehensive view into the Configuration Requirements and Library use, please view the included example sketches.*

### Root Certificate Authority
Duo Authentication API calls are performed over HTTPS, and as such we require a trustpoint to know that we are communicating with our trusted Duo API Host.  

You will need to export the Base64 Encoded X.509 Public Key of the Root Certificate Authority for your Duo API Host (```duo_host```).  

Once exported, you will need to format the Base64 Output similar to the example below:

```cpp
const char* root_ca= \
"-----BEGIN CERTIFICATE-----\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx\n" \
"XxXxXxXxXxXx\n" \
"-----END CERTIFICATE-----\n";
```

## Authors & Maintainers

The following people are responsible for the creation and/or maintenance of this project:

- Gary Oppel <gaoppel@cisco.com>

## License

This project is licensed to you under the terms of the [Apache License, Version 2.0](./LICENSE).

---

Copyright 2020 Cisco Systems, Inc. or its affiliates

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
