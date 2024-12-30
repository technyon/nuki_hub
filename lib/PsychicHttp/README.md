# PsychicHttp - HTTP on your ESP 🧙🔮

PsychicHttp is a webserver library for ESP32 + Arduino framework which uses the [ESP-IDF HTTP Server](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html) library under the hood.  It is written in a similar style to the [Arduino WebServer](https://github.com/espressif/arduino-esp32/tree/master/libraries/WebServer), [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer), and [ArduinoMongoose](https://github.com/jeremypoulter/ArduinoMongoose) libraries to make writing code simple and porting from those other libraries straightforward.

# Features

* Asynchronous approach (server runs in its own FreeRTOS thread)
* Handles all HTTP methods with lots of convenience functions:
    * GET/POST parameters
    * get/set headers
    * get/set cookies
    * basic key/value session data storage
    * authentication (basic and digest mode)
* HTTPS / SSL support
* Static fileserving (SPIFFS, LittleFS, etc.)
* Chunked response serving for large files
* File uploads (Basic + Multipart)
* Websocket support with onOpen, onFrame, and onClose callbacks
* EventSource / SSE support with onOpen, and onClose callbacks
* Request filters, including Client vs AP mode (ON_STA_FILTER / ON_AP_FILTER)
* TemplatePrinter class for dynamic variables at runtime

## Differences from ESPAsyncWebserver

* No templating system (anyone actually use this?)
* No url rewriting (but you can use response->redirect)

# Usage

## Installation

### Platformio

[PlatformIO](http://platformio.org) is an open source ecosystem for IoT development.

 Add "PsychicHttp" to project using [Project Configuration File `platformio.ini`](http://docs.platformio.org/page/projectconf.html) and [lib_deps](http://docs.platformio.org/page/projectconf/section_env_library.html#lib-deps) option:

```ini
[env:myboard]
platform = espressif...
board = ...
framework = arduino

# using the latest stable version
lib_deps = hoeken/PsychicHttp

# or using GIT Url (the latest development version)
lib_deps = https://github.com/hoeken/PsychicHttp
```

### Installation - Arduino

Open *Tools -> Manage Libraries...* and search for PsychicHttp.

# Principles of Operation

## Things to Note

* PsychicHttp is a fully asynchronous server and as such does not run on the loop thread.
* You should not use yield or delay or any function that uses them inside the callbacks.
* The server is smart enough to know when to close the connection and free resources.
* You can not send more than one response to a single request.

## PsychicHttp

* Listens for connections.
* Wraps the incoming request into PsychicRequest.
* Keeps track of clients + calls optional callbacks on client open and close.
* Find the appropriate handler (if any) for a request and pass it on.

## Request Life Cycle

* TCP connection is received by the server.
* HTTP request is wrapped inside ```PsychicRequest``` object + TCP Connection wrapped inside PsychicConnection object.
* When the request head is received, the server goes through all ```PsychicEndpoints``` and finds one that matches the url + method.
    * ```handler->filter()``` and ```handler->canHandle()``` are called on the handler to verify the handler should process the request.
    * ```handler->needsAuthentication()``` is called and sends an authorization response if required.
    * ```handler->handleRequest()``` is called to actually process the HTTP request.
* If the handler cannot process the request, the server will loop through any global handlers and call that handler if it passes filter(), canHandle(), and needsAuthentication().
* If no global handlers are called, the server.defaultEndpoint handler will be called.
* Each handler is responsible for processing the request and sending a response.
* When the response is sent, the client is closed and freed from the memory.
    * Unless its a special handler like websockets or eventsource.

![Flowchart of Request Lifecycle](/assets/request-flow.svg)

### Handlers

* ```PsychicHandler``` is used for processing and responding to specific HTTP requests.
* ```PsychicHandler``` instances can be attached to any endpoint or as global handlers.
* Setting a ```Filter``` to the ```PsychicHandler``` controls when to apply the handler, decision can be based on
  request method, url, request host/port/target host, the request client's localIP or remoteIP.
* Two filter callbacks are provided: ```ON_AP_FILTER``` to execute the rewrite when request is made to the AP interface,
  ```ON_STA_FILTER``` to execute the rewrite when request is made to the STA interface.
* The ```canHandle``` method is used for handler specific control on whether the requests can be handled. Decision can be based on request method, request url, request host/port/target host.
* Depending on how the handler is implemented, it may provide callbacks for adding your own custom processing code to the handler.
* Global ```Handlers``` are evaluated in the order they are attached to the server. The ```canHandle``` is called only
  if the ```Filter``` that was set to the ```Handler``` return true.
* The first global ```Handler``` that can handle the request is selected, no further processing of handlers is called.

![Flowchart of Request Lifecycle](/assets/handler-callbacks.svg)

### Responses and how do they work

* The ```PsychicResponse``` objects are used to send the response data back to the client.
* Typically the response should be fully generated and sent from the callback.
* It may be possible to generate the response outside the callback, but it will be difficult.
   * The exceptions are websockets + eventsource where the response is sent, but the connection is maintained and new data can be sent/received outside the handler.

# Porting From ESPAsyncWebserver

If you have existing code using ESPAsyncWebserver, you will feel right at home with PsychicHttp.  Even if internally it is much different, the external interface is very similar.  Some things are mostly cosmetic, like different class names and callback definitions.  A few things might require a bit more in-depth approach.  If you're porting your code and run into issues that aren't covered here, please post and issue.

## Globals Stuff

* Change your #include to ```#include <PsychicHttp.h>```
* Change your server instance: ```PsychicHttpServer server;```
* Define websocket handler if you have one: ```PsychicWebSocketHandler websocketHandler;```
* Define eventsource if you have one: ```PsychicEventSource eventSource;```

## setup() Stuff

* add your handlers and call server.begin()
* server has a configurable limit on .on() endpoints. change it with ```server.config.max_uri_handlers = 20;``` as needed.
* check your callback function definitions:
   * AsyncWebServerRequest -> PsychicRequest
   * no more onBody() event
      * for small bodies (server.maxRequestBodySize, default 16k) it will be automatically loaded and accessed by request->body()
      * for large bodies, use an upload handler and onUpload()   
   * websocket callbacks are much different (and simpler!)
   * websocket / eventsource handlers get attached to url in server.on("/url", &handler) instead of passing url to handler constructor.
   * eventsource callbacks are onOpen and onClose now.
* HTTP_ANY is not supported by ESP-IDF, so we can't use it either.
* NO server.onFileUpload(onUpload); (you could attach an UploadHandler to the default endpoint i guess?)
* NO server.onRequestBody(onBody); (same)

## Requests / Responses

* request->send is now response->send()
* if you create a response, call response->send() directly, not request->send(reply)
* request->headers() is not supported by ESP-IDF, you have to just check for the header you need.
* No AsyncCallbackJsonWebHandler (for now... can add if needed)
* No request->beginResponse().  Instanciate a PsychicResponse instead: ```PsychicResponse response(request);```
* No PROGMEM suppport (its not relevant to ESP32: https://esp32.com/viewtopic.php?t=20595)
* No Stream response support just yet

# Usage

## Create the Server

Here is an example of the typical server setup:

```cpp
#include <PsychicHttp.h>
PsychicHttpServer server;

void setup()
{
   //optional low level setup server config stuff here.
   //server.config is an ESP-IDF httpd_config struct
   //see: https://docs.espressif.com/projects/esp-idf/en/v4.4.6/esp32/api-reference/protocols/esp_http_server.html#_CPPv412httpd_config
   //increase maximum number of uri endpoint handlers (.on() calls)
   server.config.max_uri_handlers = 20; 

   //connect to wifi

   //call server methods to attach endpoints and handlers
   server.on(...);
   server.serveStatic(...);
   server.attachHandler(...);
}
```

## Add Handlers

One major difference from ESPAsyncWebserver is that handlers can be attached to a specific url (endpoint) or as a global handler.  The reason for this, is that attaching to a specific URL is more efficient and makes for cleaner code.

### Endpoint Handlers

An endpoint is basically just the URL path (eg. /path/to/file) without any query string.  The ```server.on(...)``` function is a convenience function for creating endpoints and attaching a handler to them.  There are two main styles: attaching a basic ```WebRequest``` handler and attaching an external handler.

```cpp
//creates a basic PsychicWebHandler that calls the request_callback callback
server.on("/url", HTTP_GET, request_callback);

//same as above, but defaults to HTTP_GET
server.on("/url", request_callback);

//attaches a websocket handler to /ws
PsychicWebSocketHandler websocketHandler;
server.on("/ws", &websocketHandler);
```

The ```server.on(...)``` returns a pointer to the endpoint, which can be used to call various functions like ```setHandler()```, ```setFilter()```, and ```setAuthentication()```.

```cpp
//respond to /url only from requests to the AP
server.on("/url", HTTP_GET, request_callback)->addFilter(ON_AP_FILTER);

//require authentication on /url
server.on("/url", HTTP_GET, request_callback)->setAuthentication("user", "pass");

//attach websocket handler to /ws
PsychicWebSocketHandler websocketHandler;
server.on("/ws")->attachHandler(&websocketHandler);
```

### Basic Requests

The ```PsychicWebHandler``` class is for handling standard web requests.  It provides a single callback: ```onRequest()```.  This callback is called when the handler receives a valid HTTP request.

One major difference from ESPAsyncWebserver is that this callback needs to return an esp_err_t variable to let the server know the result of processing the request.  The ```response->send()``` and ```request->send()``` functions will return this.  It is a good habit to return the result of these functions as sending the response will close the connection.

The function definition for the onRequest callback is:

```cpp
esp_err_t function_name(PsychicRequest *request);
```

Here is a simple example that sends back the client's IP on the URL /ip

```cpp
server.on("/ip", [](PsychicRequest *request)
{
   String output = "Your IP is: " + request->client()->remoteIP().toString();
   return response->send(output.c_str());
});
```

### Uploads

The ```PsychicUploadHandler``` class is for handling uploads, both large POST bodies and multipart encoded forms.  It provides two callbacks: ```onUpload()``` and ```onRequest()```.

```onUpload(...)``` is called when there is new data.  This function may be called multiple times so that you can process the data in chunks. The function definition for the onUpload callback is:

```cpp
esp_err_t function_name(PsychicRequest *request, const String& filename, uint64_t index, uint8_t *data, size_t len, bool final);
```

* request is a pointer to the Request object
* filename is the name of the uploaded file
* index is the overall byte position of the current data
* data is a pointer to the data buffer
* len is the length of the data buffer
* final is a flag to tell if its the last chunk of data

```onRequest(...)``` is called after the successful handling of the upload.  Its definition and usage is the same as the basic request example as above.

#### Basic Upload (file is the entire POST body)

It's worth noting that there is no standard way of passing in a filename for this method, so the handler attempts to guess the filename with the following methods:

* Checking the Content-Disposition header
* Checking the _filename query parameter (eg. /upload?filename=filename.txt becomes filename.txt)
* Checking the url and taking the last part as filename (eg. /upload/filename.txt becomes filename.txt).  You must set a wildcard url for this to work as in the example below.

```cpp
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

   return response->send(output.c_str());
 });

 //wildcard basic file upload - POST to /upload/filename.ext
 server.on("/upload/*", HTTP_POST, uploadHandler);
```

#### Multipart Upload

Very similar to the basic upload, with 2 key differences:

* multipart requests don't know the total size of the file until after it has been fully processed.  You can get a rough idea with request->contentLength(), but that is the length of the entire multipart encoded request.
* you can access form variables, including multipart file infor (name + size) in the onRequest handler using request->getParam()

```cpp
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
   PsychicWebParameter *file = request->getParam("file_upload");

   String url = "/" + file->value();
   String output;

   output += "<a href=\"" + url + "\">" + url + "</a><br/>\n";
   output += "Bytes: " + String(file->size()) + "<br/>\n";
   output += "Param 1: " + request->getParam("param1")->value() + "<br/>\n";
   output += "Param 2: " + request->getParam("param2")->value() + "<br/>\n";
   
   return response->send(output.c_str());
 });

 //upload to /multipart url
 server.on("/multipart", HTTP_POST, multipartHandler);
```

### Static File Serving

The ```PsychicStaticFileHandler``` is a special handler that does not provide any callbacks.  It is used to serve a file or files from a specific directory in a filesystem to a directory on the webserver.  The syntax is exactly the same as ESPAsyncWebserver. Anything that is derived from the ```FS``` class should work (eg. SPIFFS, LittleFS, SD, etc)

A couple important notes:

* If it finds a file with an extra .gz extension, it will serve it as gzip encoded (eg: /targetfile.ext -> {targetfile.ext}.gz)
* If the file is larger than FILE_CHUNK_SIZE (default 8kb) then it will send it as a chunked response.
* It will detect most basic filetypes and automatically set the appropriate Content-Type

The ```server.serveStatic()``` function handles creating the handler and assigning it to the server:

```cpp
//serve static files from LittleFS/www on / only to clients on same wifi network
//this is where our /index.html file lives
server.serveStatic("/", LittleFS, "/www/")->addFilter(ON_STA_FILTER);

//serve static files from LittleFS/www-ap on / only to clients on SoftAP
//this is where our /index.html file lives
server.serveStatic("/", LittleFS, "/www-ap/")->addFilter(ON_AP_FILTER);

//serve static files from LittleFS/img on /img
//it's more efficient to serve everything from a single www directory, but this is also possible.
server.serveStatic("/img", LittleFS, "/img/");

//you can also serve single files
server.serveStatic("/myfile.txt", LittleFS, "/custom.txt");
```

You could also theoretically use the file response directly:

```cpp
server.on("/ip", [](PsychicRequest *request)
{
   String filename = "/path/to/file";
   PsychicFileResponse response(request, LittleFS, filename);

   return response.send();
});
PsychicFileResponse(PsychicRequest *request, FS &fs, const String& path)
```

### Websockets

The ```PsychicWebSocketHandler``` class is for handling WebSocket connections.  It provides 3 callbacks:

```onOpen(...)``` is called when a new WebSocket client connects.
```onFrame(...)``` is called when a new WebSocket frame has arrived.
```onClose(...)``` is called when a new WebSocket client disconnects.

Here are the callback definitions:

```cpp
void open_function(PsychicWebSocketClient *client);
esp_err_t frame_function(PsychicWebSocketRequest *request, httpd_ws_frame *frame);
void close_function(PsychicWebSocketClient *client);
```

WebSockets were the main reason for starting PsychicHttp, so they are well tested.  They are also much simplified from the ESPAsyncWebserver style.  You do not need to worry about error handling, partial frame assembly, PONG messages, etc.  The onFrame() function is called when a complete frame has been received, and can handle frames up to the entire available heap size.

Here is a basic example of using WebSockets:

```cpp
 //create our handler... note this should be located as a global or somewhere it wont go out of scope and be destroyed.
 PsychicWebSocketHandler websocketHandler();

 websocketHandler.onOpen([](PsychicWebSocketClient *client) {
   Serial.printf("[socket] connection #%u connected from %s\n", client->socket(), client->remoteIP().toString().c_str());
   client->sendMessage("Hello!");
 });

 websocketHandler.onFrame([](PsychicWebSocketRequest *request, httpd_ws_frame *frame) {
     Serial.printf("[socket] #%d sent: %s\n", request->client()->socket(), (char *)frame->payload);
     return response->send(frame);
 });

 websocketHandler.onClose([](PsychicWebSocketClient *client) {
   Serial.printf("[socket] connection #%u closed from %s\n", client->socket(), client->remoteIP().toString().c_str());
 });

 //attach the handler to /ws.  You can then connect to ws://ip.address/ws
 server.on("/ws", &websocketHandler);
```

The onFrame() callback has 2 parameters:

* ```PsychicWebSocketRequest *request``` a special request with helper functions for replying in websocket format.
* ```httpd_ws_frame *frame``` ESP-IDF websocket struct.  The important struct members we care about are:
   * ```uint8_t *payload; /*!< Pre-allocated data buffer */```
   * ```size_t len; /*!< Length of the WebSocket data */```
 
For sending data on the websocket connection, there are 3 methods:

* ```response->send()``` - only available in the onFrame() callback context.
* ```webSocketHandler.sendAll()``` - can be used anywhere to send websocket messages to all connected clients.
* ```client->send()``` - can be used anywhere* to send a websocket message to a specific client

All of the above functions either accept simple ```char *``` string of you can construct your own httpd_ws_frame.

*Special Note:*  Do not hold on to the ```PsychicWebSocketClient``` for sending messages to clients outside the callbacks. That pointer is destroyed when a client disconnects.  Instead, store the ```int client->socket()```.  Then when you want to send a message, use this code:

```cpp
//make sure our client is still connected.
PsychicWebSocketClient *client = websocketHandler.getClient(socket);
if (client != NULL)
  client->send("Your Message")
```

### EventSource / SSE

The ```PsychicEventSource``` class is for handling EventSource / SSE connections.  It provides 2 callbacks:

```onOpen(...)``` is called when a new EventSource client connects.
```onClose(...)``` is called when a new EventSource client disconnects.

Here are the callback definitions:

```cpp
void open_function(PsychicEventSourceClient *client);
void close_function(PsychicEventSourceClient *client);
```

Here is a basic example of using PsychicEventSource:

```cpp
 //create our handler... note this should be located as a global or somewhere it wont go out of scope and be destroyed.
 PsychicEventSource eventSource;

 eventSource.onOpen([](PsychicEventSourceClient *client) {
   Serial.printf("[eventsource] connection #%u connected from %s\n", client->socket(), client->remoteIP().toString().c_str());
   client->send("Hello user!", NULL, millis(), 1000);
 });

 eventSource.onClose([](PsychicEventSourceClient *client) {
   Serial.printf("[eventsource] connection #%u closed from %s\n", client->socket(), client->remoteIP().toString().c_str());
 });

 //attach the handler to /events
 server.on("/events", &eventSource);
```

For sending data on the EventSource connection, there are 2 methods:

* ```eventSource.send()``` - can be used anywhere to send events to all connected clients.
* ```client->send()``` - can be used anywhere* to send events to a specific client

All of the above functions accept a simple ```char *``` message, and optionally: ```char *``` event name, id, and reconnect time.

*Special Note:*  Do not hold on to the ```PsychicEventSourceClient``` for sending messages to clients outside the callbacks. That pointer is destroyed when a client disconnects.  Instead, store the ```int client->socket()```.  Then when you want to send a message, use this code:

```cpp
//make sure our client is still connected.
PsychicEventSourceClient *client = eventSource.getClient(socket);
if (client != NULL)
  client->send("Your Event")
```

### HTTPS / SSL

PsychicHttp supports HTTPS / SSL out of the box, however there are some limitations (see performance below).  Enabling it also increases the code size by about 100kb.  To use HTTPS, you need to modify your setup like so:

```cpp
#include <PsychicHttp.h>
#include <PsychicHttpsServer.h>
PsychicHttpsServer server;
server.setCertificate(server_cert, server_key);
```

```server_cert``` and ```server_key``` are both ```const char *``` parameters which contain the server certificate and private key, respectively.

To generate your own key and self signed certificate, you can use the command below:

```
openssl req -x509 -newkey rsa:4096 -nodes -keyout server.key -out server.crt -sha256 -days 365
```

Including the ```PsychicHttpsServer.h``` also defines ```PSY_ENABLE_SSL``` which you can use in your code to allow enabling / disabling calls in your code based on if the HTTPS server is available:

```cpp
//our main server object
#ifdef PSY_ENABLE_SSL
  PsychicHttpsServer server;
#else
  PsychicHttpServer server;
#endif
```

Last, but not least, you can create a separate HTTP server on port 80 that redirects all requests to the HTTPS server:

```cpp
//this creates a 2nd server listening on port 80 and redirects all requests HTTPS
PsychicHttpServer *redirectServer = new PsychicHttpServer();
redirectServer->config.ctrl_port = 20420; // just a random port different from the default one
redirectServer->onNotFound([](PsychicRequest *request) {
   String url = "https://" + request->host() + request->url();
   return response->redirect(url.c_str());
});
```

# TemplatePrinter

**This is not specific to PsychicHttp, and it works with any `Print` object. You could for example, template data out to `File`, `Serial`, etc...**.

The template engine is a `Print` interface and can be printed to directly, however,  if you are just templating a few short strings, I'd probably just use `response.printf()` instead. **Its benefit will be seen when templating large inputs such as files.**

One benefit may be **templating a **JSON** file avoiding the need to use ArduinoJson.**

Before closing the underlying `Print`/`Stream` that this writes to, it must be flushed as small amounts of data can be buffered. A convenience method to take care of this is shows in `example 3`.

The header file is not currently added to `PsychicHttp.h` and users will have to add it manually:

```C++
#include <TemplatePrinter.h>
```
 
## Template parameter definition:

- Must start and end with a preset delimiter, the default is `%`
- Can only contain `a-z`, `A-Z`, `0-9`, and `_`
- Maximum length of 63 characters (buffer is 64 including `null`).
- A parameter must not be zero length (not including delimiters).
- Spaces or any other character do not match as a parameter, and will be output as is.
- Valid examples
  - `%MY_PARAM%`
  - `%SOME1%`
- **Invalid** examples
  - `%MY PARAM%`
  - `%SOME1 %`
  - `%UNFINISHED`
  - `%%`

## Template processing
A function or lambda is used to receive the parameter replacement.

```C++
bool templateHandler(Print &output, const char *param){
  //...
}

[](Print &output, const char *param){
  //...
}
```

Parameters:
- `Print &output` - the underlying `Print`, print the results of templating to this.
- `const char *param` - a string containing the current parameter.

The handler must return a `bool`.
- `true`: the parameter was handled, continue as normal.
- `false`: the input detected as a parameter is not, print literal.

See output in **example 1** regarding the effects of returning `true` or `false`.

## Template input handler
This is not needed unless using the static convenience function `TemplatePrinter::start()`. See **example 3**.

```C++
bool inputHandler(TemplatePrinter &printer){
  //...
}

[](TemplatePrinter &printer){
  //...
}
```

Parameters:
- `TemplatePrinter &printer` - The template engine, print your template text to this for processing.


## Example 1 - Simple use with `PsychicStreamResponse`:
This example highlights its most basic usage.

```C++

//  Function to handle parameter requests.

bool templateHandler(Print &output, const char *param){

  if(strcmp(param, "FREE_HEAP") == 0){
    output.print((double)ESP.getFreeHeap() / 1024.0, 2);

  }else if(strcmp(param, "MIN_FREE_HEAP") == 0){
    output.print((double)ESP.getMinFreeHeap() / 1024.0, 2);

  }else if(strcmp(param, "MAX_ALLOC_HEAP") == 0){
    output.print((double)ESP.getMaxAllocHeap() / 1024.0, 2);
    
  }else if(strcmp(param, "HEAP_SIZE") == 0){
    output.print((double)ESP.getHeapSize() / 1024.0, 2);
  }else{
    return false;
  }
  output.print("Kb");
  return true;
}

//  Example serving a request
server.on("/template", [](PsychicRequest *request) {
  PsychicStreamResponse response(request, "text/plain");

  response.beginSend();
  
  TemplatePrinter printer(response, templateHandler);

  printer.println("My ESP has %FREE_HEAP% left. Its lifetime minimum heap is %MIN_FREE_HEAP%.");
  printer.println("The maximum allocation size is %MAX_ALLOC_HEAP%, and its total size is %HEAP_SIZE%.");
  printer.println("This is an unhandled parameter: %UNHANDLED_PARAM% and this is an invalid param %INVALID PARAM%.");
  printer.println("This line finished with %UNFIN");
  printer.flush();

  return response.endSend();
});   
```

The output for example looks like:
```
My ESP has 170.92Kb left. Its lifetime minimum heap is 169.83Kb.
The maximum allocation size is 107.99Kb, and its total size is 284.19Kb.
This is an unhandled parameter: %UNHANDLED_PARAM% and this is an invalid param %INVALID PARAM%.
This line finished with %UNFIN
```

## Example 2 - Templating a file

```C++
server.on("/home", [](PsychicRequest *request) {
  PsychicStreamResponse response(request, "text/html");
  File file = SD.open("/www/index.html");

  response.beginSend();

  TemplatePrinter printer(response, templateHandler);

  printer.copyFrom(file);
  printer.flush();
  file.close();

  return response.endSend();
}); 
```

## Example 3 - Using the `TemplatePrinter::start` method.
This static method allows an RAII approach, allowing you to template a stream, etc... without needing a `flush()`. The function call is laid out as:

```C++
TemplatePrinter::start(host_stream, template_handler, input_handler);
```

\*these examples use the `templateHandler` function defined in example 1.

### Serve a file like example 2
```C++
server.on("/home", [](PsychicRequest *request) {
  PsychicStreamResponse response(request, "text/html");
  File file = SD.open("/www/index.html");

  response.beginSend();
  TemplatePrinter::start(response, templateHandler, [&file](TemplatePrinter &printer){
    printer.copyFrom(file);
  });
  file.close();

  return response.endSend();
});
```

### Template a string like example 1
```C++
server.on("/template2", [](PsychicRequest *request) {

  PsychicStreamResponse response(request, "text/plain");

  response.beginSend();

  TemplatePrinter::start(response, templateHandler, [](TemplatePrinter &printer){
    printer.println("My ESP has %FREE_HEAP% left. Its lifetime minimum heap is %MIN_FREE_HEAP%.");
    printer.println("The maximum allocation size is %MAX_ALLOC_HEAP%, and its total size is %HEAP_SIZE%.");
    printer.println("This is an unhandled parameter: %UNHANDLED_PARAM% and this is an invalid param %INVALID PARAM%.");
  });

  return response.endSend();
});
```

# Performance

In order to really see the differences between libraries, I created some basic benchmark firmwares for PsychicHttp, ESPAsyncWebserver, and ArduinoMongoose.  I then ran the loadtest-http.sh and loadtest-websocket.sh scripts against each firmware to get some real numbers on the performance of each server library.  All of the code and results are available in the /benchmark folder.  If you want to see the collated data and graphs, there is a [LibreOffice spreadsheet](/benchmark/comparison.ods).

![Performance graph](/benchmark/performance.png)
![Latency graph](/benchmark/latency.png)

## HTTPS / SSL

Yes, PsychicHttp supports SSL out of the box, but there are a few caveats:

* Due to memory limitations, it can only handle 2 connections at a time. Each SSL connection takes about 45k ram, and a blank PsychicHttp sketch has about 150k ram free.
* Speed and latency are still pretty good (see graph above) but the SSH handshake seems to take 1500ms.  With websockets or browser its not an issue since the connection is kept alive, but if you are loading requests in another way it will be a bit slow
* Unless you want to expose your ESP to the internet, you are limited to self signed keys and the annoying browser security warnings that come with them.

## Analysis

The results clearly show some of the reasons for writing PsychicHttp: ESPAsyncWebserver crashes under heavy load on each test, across the board in a 60s test.  That means in normal usage, you're just rolling the dice with how long it will go until it crashes.  Every other number is moot, IMHO.

ArduinoMongoose doesn't crash under heavy load, but it does bog down with extremely high latency (15s) for web requests and appears to not even respond at the highest loadings as the loadtest script crashes instead.  The code itself doesnt crash, so bonus points there.  After the high load, it does go back to serving normally.  One area ArduinoMongoose does shine, is in websockets where its performance is almost 2x the performance of PsychicHttp.  Both in requests per second and latency.  Clearly an area of improvement for PsychicHttp.

PsychicHttp has good performance across the board.  No crashes and continously responds during each test.  It is a clear winner in requests per second when serving files from memory, dynamic JSON, and has consistent performance when serving files from LittleFS. The only real downside is the lower performance of the websockets with a single connection handling 38rps, and maxing out at 120rps across multiple connections.

## Takeaways

With all due respect to @me-no-dev who has done some amazing work in the open source community, I cannot recommend anyone use the ESPAsyncWebserver for anything other than simple projects that don't need to be reliable.  Even then, PsychicHttp has taken the arcane api of the ESP-IDF web server library and made it nice and friendly to use with a very similar API to ESPAsyncWebserver.  Also, ESPAsyncWebserver is more or less abandoned, with 150 open issues, 77 pending pull requests, and the last commit in over 2 years.

ArduinoMongoose is a good alternative, although the latency issues when it gets fully loaded can be very annoying. I believe it is also cross platform to other microcontrollers as well, but I haven't tested that. The other issue here is that it is based on an old version of a modified Mongoose library that will be difficult to update as it is a major revision behind and several security updates behind as well.  Big thanks to @jeremypoulter though as PsychicHttp is a fork of ArduinoMongoose so it's built on strong bones.

# Community / Support

The best way to get support is probably with Github issues.  There is also a [Discord chat](https://discord.gg/CM5abjGG) that is pretty active.

# Roadmap

## v2.0: ESPAsyncWebserver Parity

* As much ESPAsyncWebServer compatibility as possible
* Update benchmarks and get new data
  * we should also track program size and memory usage

## Longterm Wants

* investigate websocket performance gap
* support for esp-idf framework
* Enable worker based multithreading with esp-idf v5.x
* 100-continue support?
     
If anyone wants to take a crack at implementing any of the above features I am more than happy to accept pull requests.
